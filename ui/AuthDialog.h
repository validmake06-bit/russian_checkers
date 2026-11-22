#pragma once

#include <QWidget>
class QPropertyAnimation;
class QGraphicsOpacityEffect;

class Database;
class QLineEdit;
class QLabel;
class QStackedWidget;

class AuthDialog final : public QWidget {
    Q_OBJECT
public:
    explicit AuthDialog(Database& database, QWidget* parent = nullptr);
    qint64 userId() const noexcept { return userId_; }
    QString username() const { return username_; }
    void openLoginPage();
signals:
    void authenticated(qint64 userId, const QString& username);
private slots:
    void login();
    void registerAccount();
    void showRegistration();
    void showLogin();
    void continueAsGuest();
private:
    void setMessage(QLabel* label, const QString& text, bool success = false);
    void switchFormPage(int index);
    Database& database_;
    QStackedWidget* pages_{};
    QPropertyAnimation* pageAnimation_{};
    QWidget* transitionPage_{};
    QGraphicsOpacityEffect* pageEffect_{};
    QLineEdit* usernameEdit_{};
    QLineEdit* passwordEdit_{};
    QLabel* loginMessage_{};
    QLineEdit* registerUsernameEdit_{};
    QLineEdit* registerPasswordEdit_{};
    QLineEdit* confirmPasswordEdit_{};
    QLabel* registerMessage_{};
    qint64 userId_{-1};
    QString username_;
};
