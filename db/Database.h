#pragma once

#include <QDateTime>
#include <QByteArray>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

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
