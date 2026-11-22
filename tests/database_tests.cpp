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

        const QString protocolPath = temporary.filePath(QStringLiteral("game.txt"));
        CHECK(database.saveProtocol(protocolPath, QStringLiteral("Tester"),
                                    {QStringLiteral("a3-b4")}, QStringLiteral("win"), &error));
        QFile protocol(protocolPath);
        CHECK(protocol.open(QIODevice::ReadOnly | QIODevice::Text));
        CHECK(QString::fromUtf8(protocol.readAll()).contains(QStringLiteral("a3-b4")));
    }

    // Verify that user records and completed game history survive reopening.
    {
        Database reopened;
        QString error;
        CHECK(reopened.open(databasePath, &error));
        const qint64 reopenedUserId = reopened.authenticate(
            QStringLiteral("Tester"), QStringLiteral("correct-horse-battery"), &error);
        if (reopenedUserId != userId)
            std::cerr << "Reopened login failed: id=" << reopenedUserId
                      << ", expected=" << userId << ", error=" << error.toStdString() << '\\n';
        CHECK(reopenedUserId == userId);
        const QString statistics = reopened.userStatistics(userId, &error);
        CHECK(statistics.contains(QStringLiteral("Всего партий: 1")));
        CHECK(statistics.contains(QStringLiteral("Победы: 1")));
    }

    std::cout << "All database tests passed.\n";
    return 0;
}
