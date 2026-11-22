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
        CHECK(statistics.contains(QStringLiteral("Победы: 1")));

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
    }

    // Verify that user records and completed game history survive reopening.
    {
        Database reopened;
        QString error;
        CHECK(reopened.open(databasePath, &error));
        CHECK(reopened.authenticate(QStringLiteral("Tester"),
                                    QStringLiteral("correct-horse-battery"), &error) == userId);
        const QString statistics = reopened.userStatistics(userId, &error);
        CHECK(statistics.contains(QStringLiteral("Всего партий: 1")));
        CHECK(statistics.contains(QStringLiteral("Победы: 1")));
    }

    std::cout << "All database tests passed.\n";
    return 0;
}
