#include "Database.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <cstdlib>
#include <iostream>

#define CHECK(expr) do { if (!(expr)) { std::cerr << "CHECK failed: " #expr << " at " << __FILE__ << ":" << __LINE__ << '\n'; std::abort(); } } while (false)


static QString credentialSnapshot(const QString& path) {
    const QString connectionName = QStringLiteral("credential_snapshot");
    QString snapshot;
    {
        QSqlDatabase connection = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        connection.setDatabaseName(path);
        if (connection.open()) {
            {
                QSqlQuery query(connection);
                if (query.exec(QStringLiteral(
                        "SELECT hex(salt), hex(password_hash), password_scheme "
                        "FROM users WHERE username='Tester'")) && query.next()) {
                    snapshot = query.value(0).toString() + QLatin1Char(':')
                        + query.value(1).toString() + QLatin1Char(':')
                        + query.value(2).toString();
                }
            }
            connection.close();
        }
        connection = QSqlDatabase();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return snapshot;
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    CHECK(temporary.isValid());

    const QString databasePath = temporary.filePath(QStringLiteral("checkers.sqlite"));
    qint64 userId = -1;
    QString credentialBeforeClose;
    {
        Database database;
        QString error;
        CHECK(database.open(databasePath, &error));
        CHECK(database.registerUser(QStringLiteral("Tester"), QStringLiteral("correct-horse-battery"), &error));
        userId = database.authenticate(QStringLiteral("tester"), QStringLiteral("correct-horse-battery"), &error);
        CHECK(userId > 0);
        CHECK(database.authenticate(QStringLiteral("Tester"), QStringLiteral("wrong-password"), &error) < 0);
        CHECK(!database.registerUser(QStringLiteral("tester"), QStringLiteral("another-password"), &error));

        const qint64 gameId = database.createGame(userId, 0, 6, &error);
        CHECK(gameId > 0);
        CHECK(database.appendMove(gameId, 1, QStringLiteral("a3-b4"), &error));
        CHECK(database.finishGame(gameId, QStringLiteral("win"), &error));

        const QString statistics = database.userStatistics(userId, &error);
        CHECK(statistics.contains(QStringLiteral("Всего партий: 1")));
        CHECK(statistics.contains(QStringLiteral("Победы: 1")));

        const QString protocolPath = temporary.filePath(QStringLiteral("game.txt"));
        CHECK(database.saveProtocol(protocolPath, QStringLiteral("Tester"),
                                    {QStringLiteral("a3-b4")}, QStringLiteral("win"), &error));
        QFile protocol(protocolPath);
        CHECK(protocol.open(QIODevice::ReadOnly | QIODevice::Text));
        CHECK(QString::fromUtf8(protocol.readAll()).contains(QStringLiteral("a3-b4")));
        credentialBeforeClose = credentialSnapshot(databasePath);
        CHECK(!credentialBeforeClose.isEmpty());
        CHECK(database.authenticate(QStringLiteral("Tester"),
                                    QStringLiteral("correct-horse-battery"), &error) == userId);
    }

    // Verify that user records and completed game history survive reopening.
    {
        Database reopened;
        QString error;
        CHECK(reopened.open(databasePath, &error));
        const QString credentialAfterReopen = credentialSnapshot(databasePath);
        if (credentialAfterReopen != credentialBeforeClose)
            std::cerr << "Credential record changed across reopen. before="
                      << credentialBeforeClose.toStdString() << ", after="
                      << credentialAfterReopen.toStdString() << '\n';
        CHECK(credentialAfterReopen == credentialBeforeClose);
        const qint64 reopenedUserId = reopened.authenticate(
            QStringLiteral("Tester"), QStringLiteral("correct-horse-battery"), &error);
        if (reopenedUserId != userId)
            std::cerr << "Reopened login failed: id=" << reopenedUserId
                      << ", expected=" << userId << ", error=" << error.toStdString() << '\n';
        CHECK(reopenedUserId == userId);
        const QString statistics = reopened.userStatistics(userId, &error);
        CHECK(statistics.contains(QStringLiteral("Всего партий: 1")));
        CHECK(statistics.contains(QStringLiteral("Победы: 1")));
    }

    std::cout << "All database tests passed.\n";
    return 0;
}
