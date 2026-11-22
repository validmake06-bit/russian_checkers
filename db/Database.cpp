#include "Database.h"

#include <QCryptographicHash>
#include <QSaveFile>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <atomic>

Database::Database() {
    static std::atomic<quint64> nextConnectionId{1};
    connectionName_ = QStringLiteral("checkers_%1").arg(
        nextConnectionId.fetch_add(1, std::memory_order_relaxed));
}

Database::~Database() {
    const QString name = connectionName_;
    if (db_.isValid()) db_.close();
    db_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(name);
}

bool Database::open(const QString& filePath, QString* error) {
    if (QSqlDatabase::contains(connectionName_)) db_ = QSqlDatabase::database(connectionName_);
    else db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    db_.setDatabaseName(filePath);
    if (!db_.open()) {
        if (error) *error = db_.lastError().text();
        return false;
    }
    const QStringList pragmas = {
        QStringLiteral("PRAGMA foreign_keys = ON"),
        QStringLiteral("PRAGMA journal_mode = WAL"),
        QStringLiteral("PRAGMA busy_timeout = 5000")
    };
    for (const QString& sql : pragmas) {
        QSqlQuery query(db_);
        if (!query.exec(sql)) {
            if (error) *error = query.lastError().text();
            return false;
        }
    }
    return initializeSchema(error);
}

bool Database::initializeSchema(QString* error) {
    const QStringList statements = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT NOT NULL COLLATE NOCASE UNIQUE, salt BLOB NOT NULL, password_hash BLOB NOT NULL, password_scheme INTEGER NOT NULL DEFAULT 0, created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS games (id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, started_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, finished_at TEXT, human_side INTEGER NOT NULL, difficulty INTEGER NOT NULL, result TEXT NOT NULL DEFAULT 'in_progress')"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS moves (id INTEGER PRIMARY KEY AUTOINCREMENT, game_id INTEGER NOT NULL REFERENCES games(id) ON DELETE CASCADE, ply INTEGER NOT NULL, notation TEXT NOT NULL, created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, UNIQUE(game_id, ply))"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_games_user ON games(user_id, started_at DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_moves_game ON moves(game_id, ply)")
    };
    for (const QString& sql : statements) {
        QSqlQuery query(db_);
        if (!query.exec(sql)) { if (error) *error = query.lastError().text(); return false; }
    }

    // Upgrade databases created by versions that did not store the hash scheme.
    bool hasScheme = false;
    QSqlQuery columns(db_);
    if (!columns.exec(QStringLiteral("PRAGMA table_info(users)"))) {
        if (error) *error = columns.lastError().text();
        return false;
    }
    while (columns.next())
        if (columns.value(1).toString() == QStringLiteral("password_scheme")) hasScheme = true;
    columns.finish();
    if (!hasScheme) {
        QSqlQuery alter(db_);
        if (!alter.exec(QStringLiteral("ALTER TABLE users ADD COLUMN password_scheme INTEGER NOT NULL DEFAULT 0"))) {
            if (error) *error = alter.lastError().text();
            return false;
        }
    }

    // Add a persistent favorite flag without breaking existing user databases.
    bool hasFavorite = false;
    QSqlQuery gameColumns(db_);
    if (!gameColumns.exec(QStringLiteral("PRAGMA table_info(games)"))) {
        if (error) *error = gameColumns.lastError().text();
        return false;
    }
    while (gameColumns.next())
        if (gameColumns.value(1).toString() == QStringLiteral("is_favorite")) hasFavorite = true;
    gameColumns.finish();
    if (!hasFavorite) {
        QSqlQuery alterGames(db_);
        if (!alterGames.exec(QStringLiteral(
            "ALTER TABLE games ADD COLUMN is_favorite INTEGER NOT NULL DEFAULT 0"))) {
            if (error) *error = alterGames.lastError().text();
            return false;
        }
    }

    // Apply the retention policy to databases upgraded from older versions too.
    QSqlQuery pruneExisting(db_);
    if (!pruneExisting.exec(QStringLiteral(
        "DELETE FROM games WHERE finished_at IS NOT NULL AND is_favorite=0 AND id IN ("
        " SELECT old.id FROM games old WHERE old.finished_at IS NOT NULL AND old.is_favorite=0 "
        " AND old.id NOT IN (SELECT recent.id FROM games recent "
        " WHERE recent.user_id=old.user_id AND recent.finished_at IS NOT NULL AND recent.is_favorite=0 "
        " ORDER BY recent.finished_at DESC, recent.id DESC LIMIT 50))"))) {
        if (error) *error = pruneExisting.lastError().text();
        return false;
    }
    return true;
}

namespace {
QByteArray hmacSha256(const QByteArray& keyInput, const QByteArray& message) {
    QByteArray key = keyInput;
    if (key.size() > 64) key = QCryptographicHash::hash(key, QCryptographicHash::Sha256);
    // HMAC requires zero-padding short keys. Append explicit zero bytes rather
    // than relying on the contents of newly resized QByteArray storage.
    if (key.size() < 64) key.append(QByteArray(64 - key.size(), char(0)));
    QByteArray innerPad(64, char(0x36));
    QByteArray outerPad(64, char(0x5c));
    for (int i = 0; i < 64; ++i) {
        innerPad[i] = static_cast<char>(static_cast<unsigned char>(innerPad[i]) ^ static_cast<unsigned char>(key[i]));
        outerPad[i] = static_cast<char>(static_cast<unsigned char>(outerPad[i]) ^ static_cast<unsigned char>(key[i]));
    }
    const QByteArray inner = QCryptographicHash::hash(innerPad + message, QCryptographicHash::Sha256);
    return QCryptographicHash::hash(outerPad + inner, QCryptographicHash::Sha256);
}
}

QByteArray Database::passwordDigest(const QString& password, const QByteArray& salt, int iterations) {
    const QByteArray key = password.toUtf8();
    QByteArray block = salt;
    block.append(char(0)); block.append(char(0)); block.append(char(0)); block.append(char(1));
    QByteArray u = hmacSha256(key, block);
    QByteArray result = u;
    for (int i = 1; i < iterations; ++i) {
        u = hmacSha256(key, u);
        for (qsizetype j = 0; j < result.size(); ++j)
            result[j] = static_cast<char>(static_cast<unsigned char>(result[j]) ^ static_cast<unsigned char>(u[j]));
    }
    return result;
}

QByteArray Database::legacyPasswordDigest(const QString& password, const QByteArray& salt) {
    const QByteArray passwordBytes = password.toUtf8();
    QByteArray digest = QCryptographicHash::hash(salt + passwordBytes, QCryptographicHash::Sha256);
    for (int i = 0; i < 120000; ++i)
        digest = QCryptographicHash::hash(digest + salt + passwordBytes, QCryptographicHash::Sha256);
    return digest;
}

bool Database::constantTimeEqual(const QByteArray& a, const QByteArray& b) noexcept {
    if (a.size() != b.size()) return false;
    unsigned char difference = 0;
    for (qsizetype i = 0; i < a.size(); ++i)
        difference |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    return difference == 0;
}

QByteArray Database::randomSalt() {
    QByteArray salt(16, Qt::Uninitialized);
    for (qsizetype i = 0; i < salt.size(); i += 4) {
        const quint32 value = QRandomGenerator::system()->generate();
        for (int j = 0; j < 4 && i + j < salt.size(); ++j)
            salt[i + j] = static_cast<char>((value >> (j * 8)) & 0xff);
    }
    return salt;
}

bool Database::registerUser(const QString& username, const QString& password, QString* error) {
    const QString normalized = username.trimmed().normalized(QString::NormalizationForm_C);
    if (normalized.size() < 3 || normalized.size() > 32 || password.size() < 8) {
        if (error) *error = QStringLiteral("Имя должно содержать 3–32 символа, пароль — не менее 8 символов.");
        return false;
    }
    const QByteArray salt = randomSalt();
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("INSERT INTO users(username, salt, password_hash, password_scheme) VALUES(?, ?, ?, 1)"));
    query.addBindValue(normalized);
    query.addBindValue(salt);
    query.addBindValue(passwordDigest(password, salt));
    if (!query.exec()) {
        if (error) *error = query.lastError().text().contains(QStringLiteral("UNIQUE"), Qt::CaseInsensitive)
            ? QStringLiteral("Пользователь с таким именем уже существует.") : query.lastError().text();
        return false;
    }
    return true;
}

qint64 Database::authenticate(const QString& username, const QString& password, QString* error) {
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("SELECT id, salt, password_hash, password_scheme FROM users WHERE username = ? COLLATE NOCASE"));
    query.addBindValue(username.trimmed().normalized(QString::NormalizationForm_C));
    if (!query.exec()) { if (error) *error = query.lastError().text(); return -1; }
    if (!query.next()) { if (error) *error = QStringLiteral("Неверное имя пользователя или пароль."); return -1; }

    const qint64 id = query.value(0).toLongLong();
    const QByteArray oldSalt = query.value(1).toByteArray();
    const QByteArray expected = query.value(2).toByteArray();
    const int scheme = query.value(3).toInt();
    query.finish();
    // Verify using the recorded scheme first. Also try the previous scheme as
    // a compatibility fallback for accounts created by intermediate builds.
    const QByteArray primary = scheme == 1 ? passwordDigest(password, oldSalt)
                                           : legacyPasswordDigest(password, oldSalt);
    const bool primaryMatches = constantTimeEqual(primary, expected);
    bool fallbackMatches = false;
    if (!primaryMatches) {
        const QByteArray fallback = scheme == 1 ? legacyPasswordDigest(password, oldSalt)
                                                : passwordDigest(password, oldSalt);
        fallbackMatches = constantTimeEqual(fallback, expected);
    }
    if (!primaryMatches && !fallbackMatches) {
        if (error) *error = QStringLiteral("Неверное имя пользователя или пароль.");
        return -1;
    }

    // Transparent one-time migration: all accepted older records are upgraded
    // to the current PBKDF2 scheme after successful authentication.
    if (scheme != 1 || fallbackMatches) {
        const QByteArray newSalt = randomSalt();
        QSqlQuery upgrade(db_);
        upgrade.prepare(QStringLiteral("UPDATE users SET salt = ?, password_hash = ?, password_scheme = 1 WHERE id = ?"));
        upgrade.addBindValue(newSalt);
        upgrade.addBindValue(passwordDigest(password, newSalt));
        upgrade.addBindValue(id);
        if (!upgrade.exec()) {
            if (error) *error = QStringLiteral("Вход выполнен, но обновить защиту пароля не удалось: ") + upgrade.lastError().text();
        }
    }
    return id;
}

qint64 Database::createGame(qint64 userId, int humanSide, int difficulty, QString* error) {
    if (userId <= 0 || (humanSide != 0 && humanSide != 1) ||
        (difficulty != 2 && difficulty != 4 && difficulty != 6 && difficulty != 8)) {
        if (error) *error = QStringLiteral("Некорректные параметры новой партии.");
        return -1;
    }
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("INSERT INTO games(user_id, human_side, difficulty) VALUES(?, ?, ?)"));
    query.addBindValue(userId); query.addBindValue(humanSide); query.addBindValue(difficulty);
    if (!query.exec()) { if (error) *error = query.lastError().text(); return -1; }
    return query.lastInsertId().toLongLong();
}

bool Database::appendMove(qint64 gameId, int ply, const QString& notation, QString* error) {
    if (gameId <= 0 || ply <= 0 || notation.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Некорректные данные хода.");
        return false;
    }
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("INSERT INTO moves(game_id, ply, notation) VALUES(?, ?, ?)"));
    query.addBindValue(gameId); query.addBindValue(ply); query.addBindValue(notation);
    if (!query.exec()) { if (error) *error = query.lastError().text(); return false; }
    return true;
}

bool Database::finishGame(qint64 gameId, const QString& result, QString* error) {
    static const QStringList allowedResults = {
        QStringLiteral("win"), QStringLiteral("loss"), QStringLiteral("draw"),
        QStringLiteral("abandoned")
    };
    if (gameId <= 0 || !allowedResults.contains(result)) {
        if (error) *error = QStringLiteral("Некорректный результат партии.");
        return false;
    }
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("UPDATE games SET result = ?, finished_at = CURRENT_TIMESTAMP WHERE id = ?"));
    query.addBindValue(result); query.addBindValue(gameId);
    if (!query.exec()) { if (error) *error = query.lastError().text(); return false; }
    if (query.numRowsAffected() != 1) {
        if (error) *error = QStringLiteral("Партия не найдена в базе данных.");
        return false;
    }

    // Keep only the 50 most recent finished records per account. Active games
    // are never pruned, and ON DELETE CASCADE removes their move rows as well.
    QSqlQuery prune(db_);
    prune.prepare(QStringLiteral(
        "DELETE FROM games WHERE id IN ("
        " SELECT id FROM games WHERE user_id=(SELECT user_id FROM games WHERE id=?) "
        " AND finished_at IS NOT NULL AND is_favorite=0 "
        " ORDER BY finished_at DESC, id DESC LIMIT -1 OFFSET 50)"));
    prune.addBindValue(gameId);
    if (!prune.exec()) {
        if (error) *error = prune.lastError().text();
        return false;
    }
    return true;
}

bool Database::getUserStatistics(qint64 userId, UserStatistics* statistics, QString* error) const {
    if (userId <= 0 || !statistics) {
        if (error) *error = QStringLiteral("Некорректный пользователь для статистики.");
        return false;
    }

    QSqlQuery query(db_);
    query.prepare(QStringLiteral(
        "SELECT COUNT(*), "
        "COALESCE(SUM(CASE WHEN result='win' THEN 1 ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN result='loss' THEN 1 ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN result='draw' THEN 1 ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN result='abandoned' THEN 1 ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN finished_at IS NULL "
        "THEN MAX(0, CAST((julianday('now') - julianday(started_at)) * 86400 AS INTEGER)) "
        "ELSE MAX(0, CAST((julianday(finished_at) - julianday(started_at)) * 86400 AS INTEGER)) END), 0), "
        "COALESCE(SUM(CASE WHEN finished_at IS NOT NULL AND result IN ('win','loss','draw') "
        "THEN MAX(0, CAST((julianday(finished_at) - julianday(started_at)) * 86400 AS INTEGER)) "
        "ELSE 0 END), 0), "
        "(SELECT COUNT(*) FROM moves m INNER JOIN games g ON g.id=m.game_id WHERE g.user_id=?) "
        "FROM games WHERE user_id=?"));
    query.addBindValue(userId);
    query.addBindValue(userId);
    if (!query.exec() || !query.next()) {
        if (error) *error = query.lastError().text();
        return false;
    }

    statistics->totalGames = query.value(0).toLongLong();
    statistics->wins = query.value(1).toLongLong();
    statistics->losses = query.value(2).toLongLong();
    statistics->draws = query.value(3).toLongLong();
    statistics->abandoned = query.value(4).toLongLong();
    statistics->totalPlaySeconds = qMax<qint64>(0, query.value(5).toLongLong());
    const qint64 completedSeconds = qMax<qint64>(0, query.value(6).toLongLong());
    statistics->totalMoves = query.value(7).toLongLong();

    const qint64 completedGames = statistics->wins + statistics->losses + statistics->draws;
    statistics->winRatePercent = completedGames > 0
        ? qBound(0, qRound(100.0 * double(statistics->wins) / double(completedGames)), 100)
        : 0;
    statistics->averageGameSeconds = completedGames > 0 ? completedSeconds / completedGames : 0;

    // Count all recorded games, including unfinished ones, so this reflects
    // the difficulty the player actually selects most often.
    QSqlQuery difficultyQuery(db_);
    difficultyQuery.prepare(QStringLiteral(
        "SELECT difficulty, COUNT(*) AS games_count FROM games "
        "WHERE user_id=? GROUP BY difficulty "
        "ORDER BY games_count DESC, difficulty DESC LIMIT 1"));
    difficultyQuery.addBindValue(userId);
    if (!difficultyQuery.exec()) {
        if (error) *error = difficultyQuery.lastError().text();
        return false;
    }
    if (difficultyQuery.next()) {
        statistics->mostPlayedDifficulty = difficultyQuery.value(0).toInt();
        statistics->mostPlayedDifficultyGames = difficultyQuery.value(1).toLongLong();
    } else {
        statistics->mostPlayedDifficulty = 0;
        statistics->mostPlayedDifficultyGames = 0;
    }
    return true;
}

bool Database::getRecentGames(qint64 userId, QList<RecentGameRecord>* games, QString* error) const {
    if (userId <= 0 || !games) {
        if (error) *error = QStringLiteral("Некорректный пользователь для истории партий.");
        return false;
    }
    games->clear();
    QSqlQuery query(db_);
    query.prepare(QStringLiteral(
        "SELECT g.id, g.started_at, COALESCE(g.finished_at, ''), g.human_side, "
        "g.difficulty, g.result, COUNT(m.id), g.is_favorite "
        "FROM games g LEFT JOIN moves m ON m.game_id=g.id "
        "WHERE g.user_id=? AND (g.is_favorite=1 OR g.id IN ("
        " SELECT recent.id FROM games recent WHERE recent.user_id=? "
        " ORDER BY recent.started_at DESC, recent.id DESC LIMIT 50)) "
        "GROUP BY g.id ORDER BY g.is_favorite DESC, g.started_at DESC, g.id DESC"));
    query.addBindValue(userId);
    query.addBindValue(userId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    while (query.next()) {
        RecentGameRecord record;
        record.id = query.value(0).toLongLong();
        record.startedAt = query.value(1).toString();
        record.finishedAt = query.value(2).toString();
        record.humanSide = query.value(3).toInt();
        record.difficulty = query.value(4).toInt();
        record.result = query.value(5).toString();
        record.moveCount = query.value(6).toLongLong();
        record.isFavorite = query.value(7).toBool();
        games->push_back(record);
    }
    return true;
}

bool Database::setGameFavorite(qint64 userId, qint64 gameId, bool favorite, QString* error) {
    if (userId <= 0 || gameId <= 0) {
        if (error) *error = QStringLiteral("Некорректная партия для изменения избранного.");
        return false;
    }
    QSqlQuery query(db_);
    query.prepare(QStringLiteral(
        "UPDATE games SET is_favorite=? WHERE id=? AND user_id=?"));
    query.addBindValue(favorite ? 1 : 0);
    query.addBindValue(gameId);
    query.addBindValue(userId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    if (query.numRowsAffected() != 1) {
        if (error) *error = QStringLiteral("Партия не найдена.");
        return false;
    }
    if (!favorite) {
        // Once a game leaves favorites, immediately restore the 50-record
        // rolling limit and let the foreign-key cascade remove old move rows.
        QSqlQuery prune(db_);
        prune.prepare(QStringLiteral(
            "DELETE FROM games WHERE user_id=? AND finished_at IS NOT NULL AND is_favorite=0 "
            "AND id IN (SELECT id FROM games WHERE user_id=? AND finished_at IS NOT NULL "
            "AND is_favorite=0 ORDER BY finished_at DESC, id DESC LIMIT -1 OFFSET 50)"));
        prune.addBindValue(userId);
        prune.addBindValue(userId);
        if (!prune.exec()) {
            if (error) *error = prune.lastError().text();
            return false;
        }
    }
    return true;
}

bool Database::getGameMoves(qint64 userId, qint64 gameId, QStringList* moves, QString* error) const {
    if (userId <= 0 || gameId <= 0 || !moves) {
        if (error) *error = QStringLiteral("Некорректные параметры просмотра партии.");
        return false;
    }
    moves->clear();
    QSqlQuery query(db_);
    query.prepare(QStringLiteral(
        "SELECT m.notation FROM moves m "
        "INNER JOIN games g ON g.id=m.game_id "
        "WHERE g.id=? AND g.user_id=? ORDER BY m.ply"));
    query.addBindValue(gameId);
    query.addBindValue(userId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    while (query.next()) moves->push_back(query.value(0).toString());
    return true;
}

QString Database::userStatistics(qint64 userId, QString* error) const {
    UserStatistics statistics;
    if (!getUserStatistics(userId, &statistics, error))
        return QStringLiteral("Не удалось загрузить статистику.");
    return QStringLiteral("Всего партий: %1\nПобеды: %2\nПоражения: %3\nНичьи: %4\nПрервано: %5\nПроцент побед: %6%\nВремя в игре: %7 сек.\nСредняя партия: %8 сек.\nХодов: %9")
        .arg(statistics.totalGames).arg(statistics.wins).arg(statistics.losses)
        .arg(statistics.draws).arg(statistics.abandoned).arg(statistics.winRatePercent)
        .arg(statistics.totalPlaySeconds).arg(statistics.averageGameSeconds).arg(statistics.totalMoves);
}

bool Database::saveProtocol(const QString& filePath, const QString& username,
                            const QStringList& moves, const QString& result, QString* error) const {
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    QTextStream out(&file);
    out << "Протокол партии в русские шашки\nИгрок: " << username
        << "\nДата: " << QDateTime::currentDateTime().toString(Qt::ISODate)
        << "\n\nХоды:\n";
    for (const QString& move : moves) out << move << '\n';
    out << "\nРезультат: " << result << '\n';
    out.flush();
    if (out.status() != QTextStream::Ok || !file.commit()) {
        if (error) *error = file.errorString().isEmpty() ? QStringLiteral("Не удалось записать файл протокола.") : file.errorString();
        return false;
    }
    return true;
}
