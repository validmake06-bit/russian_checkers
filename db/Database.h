#pragma once

#include <QDateTime>
#include <QByteArray>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

struct UserStatistics {
    qint64 totalGames{0};
    qint64 wins{0};
    qint64 losses{0};
    qint64 draws{0};
    qint64 abandoned{0};
    qint64 totalPlaySeconds{0};
    qint64 averageGameSeconds{0};
    qint64 totalMoves{0};
    int winRatePercent{0};
};

class Database final {
public:
    Database();
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool open(const QString& filePath, QString* error = nullptr);
    bool registerUser(const QString& username, const QString& password, QString* error = nullptr);
    qint64 authenticate(const QString& username, const QString& password, QString* error = nullptr);

    qint64 createGame(qint64 userId, int humanSide, int difficulty, QString* error = nullptr);
    bool appendMove(qint64 gameId, int ply, const QString& notation, QString* error = nullptr);
    bool finishGame(qint64 gameId, const QString& result, QString* error = nullptr);
    bool getUserStatistics(qint64 userId, UserStatistics* statistics, QString* error = nullptr) const;
    QString userStatistics(qint64 userId, QString* error = nullptr) const;
    bool saveProtocol(const QString& filePath, const QString& username,
                      const QStringList& moves, const QString& result, QString* error = nullptr) const;

private:
    bool initializeSchema(QString* error);
    static QByteArray passwordDigest(const QString& password, const QByteArray& salt, int iterations = 210000);
    static QByteArray legacyPasswordDigest(const QString& password, const QByteArray& salt);
    static bool constantTimeEqual(const QByteArray& a, const QByteArray& b) noexcept;
    static QByteArray randomSalt();
    QSqlDatabase db_;
    QString connectionName_;
};
