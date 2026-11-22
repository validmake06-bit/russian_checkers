#include "Database.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <cstdlib>
#include <iostream>

#define CHECK(expr) do { if (!(expr)) { std::cerr << "CHECK failed: " #expr << " at " << __FILE__ << ":" << __LINE__ << '\n'; std::abort(); } } while (false)


int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    CHECK(temporary.isValid());

    const QString databasePath = temporary.filePath(QStringLiteral("checkers.sqlite"));
    qint64 userId = -1;
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

        UserStatistics metrics;
        CHECK(database.getUserStatistics(userId, &metrics, &error));
        CHECK(metrics.totalGames == 1);
        CHECK(metrics.wins == 1);
        CHECK(metrics.losses == 0);
        CHECK(metrics.draws == 0);
        CHECK(metrics.totalMoves == 1);
        CHECK(metrics.winRatePercent == 100);
        CHECK(metrics.totalPlaySeconds >= 0);
        CHECK(metrics.averageGameSeconds >= 0);

        const QString protocolPath = temporary.filePath(QStringLiteral("game.txt"));
        CHECK(database.saveProtocol(protocolPath, QStringLiteral("Tester"),
                                    {QStringLiteral("a3-b4")}, QStringLiteral("win"), &error));
        QFile protocol(protocolPath);
        CHECK(protocol.open(QIODevice::ReadOnly | QIODevice::Text));
        CHECK(QString::fromUtf8(protocol.readAll()).contains(QStringLiteral("a3-b4")));
        CHECK(database.authenticate(QStringLiteral("Tester"),
                                    QStringLiteral("correct-horse-battery"), &error) == userId);

        // Retention is bounded: only the 50 newest finished games and their
        // move rows remain available for the history browser.
        qint64 newestGameId = -1;
        for (int index = 0; index < 51; ++index) {
            const qint64 recentId = database.createGame(userId, index % 2, 4, &error);
            CHECK(recentId > 0);
            CHECK(database.appendMove(recentId, 1, QStringLiteral("move-%1").arg(index), &error));
            CHECK(database.finishGame(recentId, index % 2 == 0
                ? QStringLiteral("win") : QStringLiteral("draw"), &error));
            newestGameId = recentId;
        }
        QList<RecentGameRecord> recentGames;
        CHECK(database.getRecentGames(userId, &recentGames, &error));
        CHECK(recentGames.size() == 50);
        CHECK(recentGames.front().id == newestGameId);
        QStringList recentMoves;
        CHECK(database.getGameMoves(userId, newestGameId, &recentMoves, &error));
        CHECK(recentMoves.size() == 1);
        CHECK(recentMoves.front() == QStringLiteral("move-50"));
        CHECK(database.getGameMoves(userId, gameId, &recentMoves, &error));
        CHECK(recentMoves.isEmpty());
        CHECK(database.getUserStatistics(userId, &metrics, &error));
        CHECK(metrics.totalGames == 50);
        CHECK(metrics.wins == 25);
        CHECK(metrics.draws == 25);
    }

    // Verify that user records and completed game history survive reopening.
    {
        Database reopened;
        QString error;
        CHECK(reopened.open(databasePath, &error));
        CHECK(reopened.authenticate(QStringLiteral("Tester"),
                                    QStringLiteral("correct-horse-battery"), &error) == userId);
        const QString statistics = reopened.userStatistics(userId, &error);
        CHECK(statistics.contains(QStringLiteral("Всего партий: 50")));
        CHECK(statistics.contains(QStringLiteral("Победы: 25")));
    }

    std::cout << "All database tests passed.\n";
    return 0;
}
