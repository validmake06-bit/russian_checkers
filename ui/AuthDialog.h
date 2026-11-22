#pragma once

#include <QDialog>

class Database;
class QLineEdit;
class QLabel;
class QStackedWidget;

class AuthDialog final : public QDialog {
    Q_OBJECT
public:
    explicit AuthDialog(Database& database, QWidget* parent = nullptr);
    qint64 userId() const noexcept { return userId_; }
    QString username() const { return username_; }
private slots:
    void login();
    void registerAccount();
    void showRegistration();
    void showLogin();
    void continueAsGuest();
private:
    void setMessage(QLabel* label, const QString& text, bool success = false);
    Database& database_;
    QStackedWidget* pages_{};
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
