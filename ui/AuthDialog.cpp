#include "AuthDialog.h"
#include "../db/Database.h"

#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QColor>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

namespace {
QFrame* makeCard(QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setObjectName("authCard");
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(30);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 30));
    card->setGraphicsEffect(shadow);
    return card;
}
QLineEdit* makeInput(QWidget* parent, const QString& placeholder, bool password = false) {
    auto* edit = new QLineEdit(parent);
    edit->setObjectName("authInput");
    edit->setMinimumHeight(46);
    edit->setPlaceholderText(placeholder);
    edit->setClearButtonEnabled(!password);
    if (password) edit->setEchoMode(QLineEdit::Password);
    return edit;
}
QPushButton* makeButton(QWidget* parent, const QString& text, const QString& objectName) {
    auto* button = new QPushButton(text, parent);
    button->setObjectName(objectName);
    button->setMinimumHeight(objectName == QStringLiteral("authPrimary") ? 48 : 44);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}
}

AuthDialog::AuthDialog(Database& database, QWidget* parent)
    : QWidget(parent), database_(database) {
    setObjectName("authPage");
    setMinimumWidth(420);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(30, 26, 30, 26);
    root->setSpacing(0);

    auto* brand = new QLabel(QStringLiteral("♟  RUSSIAN CHECKERS"), this);
    brand->setObjectName("brand");
    brand->setAlignment(Qt::AlignCenter);
    auto* title = new QLabel(QStringLiteral("Добро пожаловать"), this);
    title->setObjectName("heroTitle");
    title->setAlignment(Qt::AlignCenter);
    auto* subtitle = new QLabel(QStringLiteral("Играйте в своём аккаунте или без регистрации."), this);
    subtitle->setObjectName("heroSubtitle");
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setWordWrap(true);

    pages_ = new QStackedWidget(this);

    // Login page
    auto* loginPage = new QWidget(pages_);
    auto* loginPageLayout = new QVBoxLayout(loginPage);
    loginPageLayout->setContentsMargins(0, 0, 0, 0);
    loginPageLayout->addStretch(1);
    auto* loginCard = makeCard(loginPage);
    loginCard->setMaximumWidth(480);
    auto* loginLayout = new QVBoxLayout(loginCard);
    loginLayout->setContentsMargins(24, 24, 24, 22);
    loginLayout->setSpacing(13);
    auto* loginTitle = new QLabel(QStringLiteral("Вход в аккаунт"), loginCard);
    loginTitle->setObjectName("formTitle");
    usernameEdit_ = makeInput(loginCard, QStringLiteral("Имя пользователя"));
    passwordEdit_ = makeInput(loginCard, QStringLiteral("Пароль"), true);
    loginMessage_ = new QLabel(loginCard);
    loginMessage_->setObjectName("authMessage");
    loginMessage_->setWordWrap(true);
    loginMessage_->setMinimumHeight(32);
    auto* loginButton = makeButton(loginCard, QStringLiteral("Войти"), QStringLiteral("authPrimary"));
    auto* toRegisterButton = makeButton(loginCard, QStringLiteral("Создать аккаунт"), QStringLiteral("authSecondary"));
    auto* guestButton = makeButton(loginCard, QStringLiteral("Продолжить как гость"), QStringLiteral("authGuest"));
    loginLayout->addWidget(loginTitle);
    loginLayout->addWidget(usernameEdit_);
    loginLayout->addWidget(passwordEdit_);
    loginLayout->addWidget(loginMessage_);
    loginLayout->addWidget(loginButton);
    loginLayout->addWidget(toRegisterButton);
    loginLayout->addWidget(guestButton);
    loginPageLayout->addWidget(loginCard, 0, Qt::AlignHCenter);
    loginPageLayout->addStretch(1);
    pages_->addWidget(loginPage);

    // Registration page is intentionally separate from the login form.
    auto* registerPage = new QWidget(pages_);
    auto* registerPageLayout = new QVBoxLayout(registerPage);
    registerPageLayout->setContentsMargins(0, 0, 0, 0);
    registerPageLayout->addStretch(1);
    auto* registerCard = makeCard(registerPage);
    registerCard->setMaximumWidth(480);
    auto* registerLayout = new QVBoxLayout(registerCard);
    registerLayout->setContentsMargins(24, 24, 24, 22);
    registerLayout->setSpacing(13);
    auto* registerTitle = new QLabel(QStringLiteral("Новый аккаунт"), registerCard);
    registerTitle->setObjectName("formTitle");
    registerUsernameEdit_ = makeInput(registerCard, QStringLiteral("Придумайте имя пользователя"));
    registerPasswordEdit_ = makeInput(registerCard, QStringLiteral("Придумайте пароль (от 8 символов)"), true);
    confirmPasswordEdit_ = makeInput(registerCard, QStringLiteral("Повторите пароль"), true);
    registerMessage_ = new QLabel(registerCard);
    registerMessage_->setObjectName("authMessage");
    registerMessage_->setWordWrap(true);
    registerMessage_->setMinimumHeight(34);
    auto* createButton = makeButton(registerCard, QStringLiteral("Зарегистрироваться"), QStringLiteral("authPrimary"));
    auto* backButton = makeButton(registerCard, QStringLiteral("← Назад ко входу"), QStringLiteral("authSecondary"));
    registerLayout->addWidget(registerTitle);
    registerLayout->addWidget(registerUsernameEdit_);
    registerLayout->addWidget(registerPasswordEdit_);
    registerLayout->addWidget(confirmPasswordEdit_);
    registerLayout->addWidget(registerMessage_);
    registerLayout->addWidget(createButton);
    registerLayout->addWidget(backButton);
    registerPageLayout->addWidget(registerCard, 0, Qt::AlignHCenter);
    registerPageLayout->addStretch(1);
    pages_->addWidget(registerPage);

    auto* footer = new QLabel(QStringLiteral("Гостевой режим не требует аккаунта. Прогресс гостя не привязывается к профилю."), this);
    footer->setObjectName("authFooter");
    footer->setAlignment(Qt::AlignCenter);
    footer->setWordWrap(true);

    root->addWidget(brand);
    root->addSpacing(16);
    root->addWidget(title);
    root->addWidget(subtitle);
    root->addSpacing(22);
    root->addWidget(pages_, 1);
    root->addSpacing(10);
    root->addWidget(footer);

    const bool dark = QSettings().value(QStringLiteral("appearance/darkTheme"), false).toBool();
    if (dark) {
        setStyleSheet(R"(
            QWidget#authPage { background: #241812; color: #f4eadc; }
            QLabel#brand { color: #b58c68; font-size: 11px; font-weight: 700; letter-spacing: 2px; }
            QLabel#heroTitle { color: #fff4e4; font-size: 27px; font-weight: 700; }
            QLabel#heroSubtitle { color: #d7c0a7; font-size: 13px; }
            QFrame#authCard { background: #202630; border: 1px solid #303946; border-radius: 18px; }
            QLabel#formTitle { color: #f4eadc; font-size: 17px; font-weight: 650; }
            QLineEdit#authInput { background: #1a2029; color: #f4eadc; border: 1px solid #d1b99d; border-radius: 10px; padding: 0 14px; font-size: 14px; selection-background-color: #53675b; }
            QLineEdit#authInput:focus { border: 1px solid #91a494; }
            QLabel#authMessage { color: #d69a91; font-size: 12px; }
            QPushButton { border: 0; border-radius: 10px; font-size: 14px; font-weight: 650; }
            QPushButton#authPrimary { background: #a8b9aa; color: #18201b; }
            QPushButton#authPrimary:hover { background: #bac9bc; }
            QPushButton#authSecondary, QPushButton#authGuest { background: #2b3540; color: #e5e9e7; border: 1px solid #3a4651; }
            QPushButton#authSecondary:hover, QPushButton#authGuest:hover { background: #35424c; }
            QLabel#authFooter { color: #89958d; font-size: 11px; }
        )");
    } else {
        setStyleSheet(R"(
            QWidget#authPage { background: #f4eadc; color: #3c281d; }
            QLabel#brand { color: #946b49; font-size: 11px; font-weight: 700; letter-spacing: 2px; }
            QLabel#heroTitle { color: #3b271d; font-size: 27px; font-weight: 700; }
            QLabel#heroSubtitle { color: #806653; font-size: 13px; }
            QFrame#authCard { background: #fff9ef; border: 1px solid #e2e6e1; border-radius: 18px; }
            QLabel#formTitle { color: #3c281d; font-size: 17px; font-weight: 650; }
            QLineEdit#authInput { background: #fbfcfa; color: #3c281d; border: 1px solid #d6bea2; border-radius: 10px; padding: 0 14px; font-size: 14px; selection-background-color: #dce7dd; }
            QLineEdit#authInput:focus { border: 1px solid #9aaf9d; }
            QLabel#authMessage { color: #a15f56; font-size: 12px; }
            QPushButton { border: 0; border-radius: 10px; font-size: 14px; font-weight: 650; }
            QPushButton#authPrimary { background: #657d6b; color: #fff9ef; }
            QPushButton#authPrimary:hover { background: #748d7a; }
            QPushButton#authSecondary, QPushButton#authGuest { background: #f1e3d0; color: #684a32; border: 1px solid #d9c3a7; }
            QPushButton#authSecondary:hover, QPushButton#authGuest:hover { background: #ead8c2; }
            QLabel#authFooter { color: #879188; font-size: 11px; }
        )");
    }

    connect(loginButton, &QPushButton::clicked, this, &AuthDialog::login);
    connect(toRegisterButton, &QPushButton::clicked, this, &AuthDialog::showRegistration);
    connect(guestButton, &QPushButton::clicked, this, &AuthDialog::continueAsGuest);
    connect(createButton, &QPushButton::clicked, this, &AuthDialog::registerAccount);
    connect(backButton, &QPushButton::clicked, this, &AuthDialog::showLogin);
    connect(passwordEdit_, &QLineEdit::returnPressed, this, &AuthDialog::login);
    connect(confirmPasswordEdit_, &QLineEdit::returnPressed, this, &AuthDialog::registerAccount);
}

void AuthDialog::setMessage(QLabel* label, const QString& text, bool success) {
    label->setStyleSheet(success ? QStringLiteral("color:#4c9a67") : QStringLiteral("color:#d16f65"));
    label->setText(text);
}

void AuthDialog::switchFormPage(int index) {
    if (!pages_ || index < 0 || index >= pages_->count()) return;
    if (pageAnimation_) {
        pageAnimation_->stop();
        if (transitionPage_ && transitionPage_->graphicsEffect() == pageEffect_)
            transitionPage_->setGraphicsEffect(nullptr);
        pageAnimation_->deleteLater();
        pageAnimation_ = nullptr;
        transitionPage_ = nullptr;
        pageEffect_ = nullptr;
    }
    if (pages_->currentIndex() == index) return;

    QWidget* target = pages_->widget(index);
    auto* effect = new QGraphicsOpacityEffect(target);
    effect->setOpacity(0.0);
    target->setGraphicsEffect(effect);
    pages_->setCurrentIndex(index);

    auto* animation = new QPropertyAnimation(effect, "opacity", this);
    animation->setDuration(180);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    pageAnimation_ = animation;
    transitionPage_ = target;
    pageEffect_ = effect;
    connect(animation, &QPropertyAnimation::finished, this, [this, animation, target, effect] {
        if (target && target->graphicsEffect() == effect)
            target->setGraphicsEffect(nullptr);
        if (pageAnimation_ == animation) {
            pageAnimation_ = nullptr;
            transitionPage_ = nullptr;
            pageEffect_ = nullptr;
        }
        animation->deleteLater();
    });
    animation->start();
}

void AuthDialog::showRegistration() {
    registerUsernameEdit_->setText(usernameEdit_->text().trimmed());
    registerPasswordEdit_->clear();
    confirmPasswordEdit_->clear();
    registerMessage_->clear();
    switchFormPage(1);
    registerUsernameEdit_->setFocus();
}

void AuthDialog::openLoginPage() {
    showLogin();
}

void AuthDialog::showLogin() {
    usernameEdit_->setText(registerUsernameEdit_->text().trimmed());
    passwordEdit_->clear();
    loginMessage_->clear();
    switchFormPage(0);
    usernameEdit_->setFocus();
}

void AuthDialog::login() {
    const QString name = usernameEdit_->text().trimmed();
    if (name.isEmpty() || passwordEdit_->text().isEmpty()) {
        setMessage(loginMessage_, QStringLiteral("Введите имя пользователя и пароль."));
        return;
    }
    QString error;
    const qint64 id = database_.authenticate(name, passwordEdit_->text(), &error);
    if (id < 0) { setMessage(loginMessage_, error.isEmpty() ? QStringLiteral("Не удалось выполнить вход.") : error); return; }
    userId_ = id;
    username_ = name;
    emit authenticated(userId_, username_);
}

void AuthDialog::registerAccount() {
    const QString name = registerUsernameEdit_->text().trimmed();
    const QString password = registerPasswordEdit_->text();
    if (password != confirmPasswordEdit_->text()) {
        setMessage(registerMessage_, QStringLiteral("Пароли не совпадают."));
        return;
    }
    QString error;
    if (!database_.registerUser(name, password, &error)) {
        setMessage(registerMessage_, error.isEmpty() ? QStringLiteral("Не удалось создать аккаунт.") : error);
        return;
    }

    // Verify the just-created record using the exact same path as a normal login.
    // If the database/hash layer is inconsistent, show the actual error instead of
    // claiming that registration completed successfully.
    const qint64 id = database_.authenticate(name, password, &error);
    if (id < 0) {
        setMessage(registerMessage_, QStringLiteral("Аккаунт записан, но проверка входа не прошла: %1").arg(error));
        return;
    }
    userId_ = id;
    username_ = name;
    emit authenticated(userId_, username_);
}

void AuthDialog::continueAsGuest() {
    userId_ = -1;
    username_ = QStringLiteral("Гость");
    emit authenticated(userId_, username_);
}
