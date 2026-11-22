#include "MainWindow.h"
#include "AuthDialog.h"
#include "../db/Database.h"
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QGridLayout>
#include <QScrollArea>

#include <QtConcurrent/QtConcurrentRun>
#include <QStatusBar>
#include <QGraphicsDropShadowEffect>
#include <QColor>
#include <QDataStream>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QVector>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QtMath>
#include <QSettings>
#include <QSoundEffect>
#include <QCheckBox>
#include <QSlider>
#include <QFormLayout>
#include <QUrl>
#include <QFileDialog>
#include <QStackedWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <algorithm>
#include <cmath>

using namespace checkers;

namespace {
QByteArray makeMonoWav(const QVector<qint16>& samples, int sampleRate) {
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("RIFF", 4);
    stream << quint32(36 + samples.size() * 2);
    stream.writeRawData("WAVEfmt ", 8);
    stream << quint32(16) << quint16(1) << quint16(1)
           << quint32(sampleRate) << quint32(sampleRate * 2)
           << quint16(2) << quint16(16);
    stream.writeRawData("data", 4);
    stream << quint32(samples.size() * 2);
    for (qint16 sample : samples) stream << sample;
    return bytes;
}

QString createSoundPack() {
    constexpr int rate = 22050;
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                              + QStringLiteral("/audio");
    QDir().mkpath(directory);

    auto save = [&](const QString& name, const QVector<qint16>& samples) {
        QFile file(directory + QLatin1Char('/') + name);
        if (file.open(QIODevice::WriteOnly))
            file.write(makeMonoWav(samples, rate));
    };
    auto clampSample = [](double value) {
        return static_cast<qint16>(qBound(-32767, qRound(value * 32767.0), 32767));
    };

    QVector<qint16> move;
    move.reserve(rate / 11);
    quint32 noise = 0x6d2b79f5u;
    for (int i = 0; i < rate / 11; ++i) {
        const double t = double(i) / rate;
        noise = noise * 1664525u + 1013904223u;
        const double random = (double((noise >> 8) & 0xffffu) / 32767.5) - 1.0;
        // A fingertip-sized wooden tap: a tiny broadband attack and a short,
        // quiet body resonance. Avoid the long, bass-heavy envelope of a drum.
        const double attack = random * std::exp(-t * 210.0) * 0.105;
        const double body = std::sin(2.0 * 3.14159265358979323846 * 205.0 * t)
                          * std::exp(-t * 92.0) * 0.105;
        const double softLow = std::sin(2.0 * 3.14159265358979323846 * 142.0 * t)
                             * std::exp(-t * 118.0) * 0.035;
        move.push_back(clampSample(attack + body + softLow));
    }
    save(QStringLiteral("wood_move.wav"), move);

    QVector<qint16> capture;
    capture.reserve(rate / 9);
    for (int i = 0; i < rate / 9; ++i) {
        const double t = double(i) / rate;
        const double attack = std::sin(2.0 * 3.14159265358979323846 * 165.0 * t)
                            * std::exp(-t * 82.0) * 0.13;
        const double texture = std::sin(2.0 * 3.14159265358979323846 * 265.0 * t)
                             * std::exp(-t * 115.0) * 0.035;
        capture.push_back(clampSample(attack + texture));
    }
    save(QStringLiteral("wood_capture.wav"), capture);

    QVector<qint16> click;
    click.reserve(rate / 16);
    for (int i = 0; i < rate / 16; ++i) {
        const double t = double(i) / rate;
        const double envelope = std::exp(-t * 125.0);
        const double tone = std::sin(2.0 * 3.14159265358979323846 * 920.0 * t) * 0.10
                          + std::sin(2.0 * 3.14159265358979323846 * 1480.0 * t) * 0.035;
        click.push_back(clampSample(tone * envelope));
    }
    save(QStringLiteral("button_click.wav"), click);

    // A quiet, slow instrumental pad. Four warm seventh/suspended chords form
    // a seamless 16-second loop without percussion or sharp transients.
    const double chords[4][4] = {
        {261.63, 329.63, 392.00, 493.88},
        {220.00, 261.63, 329.63, 392.00},
        {174.61, 220.00, 261.63, 329.63},
        {196.00, 220.00, 293.66, 392.00}
    };
    QVector<qint16> ambient;
    const int musicLength = rate * 16;
    ambient.reserve(musicLength);
    for (int i = 0; i < musicLength; ++i) {
        const double t = double(i) / rate;
        const double section = t / 4.0;
        const int chord = int(section) % 4;
        const double local = section - std::floor(section);
        const double blend = qBound(0.0, (local - 0.82) / 0.18, 1.0);
        double sample = 0.0;
        for (int n = 0; n < 4; ++n) {
            const double a = std::sin(2.0 * 3.14159265358979323846 * chords[chord][n] * t + n * 0.31);
            const double b = std::sin(2.0 * 3.14159265358979323846 * chords[(chord + 1) % 4][n] * t + n * 0.31);
            sample += ((1.0 - blend) * a + blend * b) * (n == 0 ? 0.115 : 0.072);
        }
        const double slowBreath = 0.88 + 0.12 * std::sin(2.0 * 3.14159265358979323846 * t / 11.0);
        const double edge = qMin(1.0, qMin(t / 0.45, (16.0 - t) / 0.45));
        ambient.push_back(clampSample(sample * slowBreath * qBound(0.0, edge, 1.0) * 0.48));
    }
    save(QStringLiteral("calm_ambient.wav"), ambient);
    return directory;
}

void addButtonFeedback(QPushButton* button, QSoundEffect* clickSound) {
    if (!button || button->property("feedbackInstalled").toBool()) return;
    button->setProperty("feedbackInstalled", true);
    button->setCursor(Qt::PointingHandCursor);
    auto* opacity = new QGraphicsOpacityEffect(button);
    button->setGraphicsEffect(opacity);
    auto* animation = new QPropertyAnimation(opacity, "opacity", button);
    animation->setDuration(105);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(button, &QPushButton::pressed, button, [opacity, animation] {
        animation->stop();
        animation->setStartValue(opacity->opacity());
        animation->setEndValue(0.78);
        animation->start();
    });
    QObject::connect(button, &QPushButton::released, button, [opacity, animation] {
        animation->stop();
        animation->setStartValue(opacity->opacity());
        animation->setEndValue(1.0);
        animation->start();
    });
    QObject::connect(button, &QPushButton::clicked, button, [clickSound] {
        if (clickSound) clickSound->play();
    });
}
} // namespace

MainWindow::MainWindow(Database& database, qint64 userId, QString username, QWidget* parent)
    : QMainWindow(parent), database_(database), userId_(userId), username_(std::move(username)) {
    setWindowTitle(QStringLiteral("Русские шашки — против компьютера"));
    resize(1240, 820);
    setMinimumSize(980, 680);

    auto* central = new QWidget(this);
    central->setObjectName("appBackground");
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(26, 24, 26, 24);
    root->setSpacing(24);

    auto* boardContainer = new QWidget(central);
    auto* boardGrid = new QGridLayout(boardContainer);
    boardGrid->setContentsMargins(0, 0, 0, 0);
    boardGrid->setSpacing(0);
    boardView_ = new BoardWidget(boardContainer);
    boardView_->setObjectName("boardView");
    boardGrid->addWidget(boardView_, 0, 0);

    drawerToggle_ = new QPushButton(QStringLiteral("☰"), boardContainer);
    drawerToggle_->setObjectName("drawerToggle");
    drawerToggle_->setFixedSize(48, 44);
    drawerToggle_->setToolTip(QStringLiteral("Открыть меню партии"));
    boardGrid->addWidget(drawerToggle_, 0, 0, Qt::AlignTop | Qt::AlignLeft);
    root->addWidget(boardContainer, 1);

    auto* panel = new QFrame(central);
    panel->setObjectName("sidePanel");
    panel->setMinimumWidth(0);
    panel->setMaximumWidth(0);
    panel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    gameDrawer_ = panel;
    auto* panelShadow = new QGraphicsDropShadowEffect(panel);
    panelShadow->setBlurRadius(18);
    panelShadow->setOffset(0, 4);
    panelShadow->setColor(QColor(0, 0, 0, 20));
    panel->setGraphicsEffect(panelShadow);

    auto* side = new QVBoxLayout(panel);
    side->setContentsMargins(20, 18, 20, 18);
    side->setSpacing(8);

    auto* brand = new QLabel(QStringLiteral("♟  RUSSIAN CHECKERS"), panel);
    brand->setObjectName("brand");
    auto* title = new QLabel(QStringLiteral("Русские шашки"), panel);
    title->setObjectName("title");
    auto* subtitle = new QLabel(QStringLiteral("Партия против компьютера"), panel);
    subtitle->setObjectName("muted");
    userLabel_ = new QLabel(QStringLiteral("Игрок: %1").arg(username_), panel);
    userLabel_->setObjectName("userLabel");

    auto* divider1 = new QFrame(panel);
    divider1->setObjectName("separator");
    divider1->setFrameShape(QFrame::HLine);
    status_ = new QLabel(panel);
    status_->setWordWrap(true);
    status_->setObjectName("status");

    auto* sideLabel = new QLabel(QStringLiteral("ВАША СТОРОНА"), panel);
    sideLabel->setObjectName("fieldLabel");
    sideChoice_ = new QComboBox(panel);
    sideChoice_->addItem(QStringLiteral("Играть белыми"), int(Side::White));
    sideChoice_->addItem(QStringLiteral("Играть чёрными"), int(Side::Black));

    auto* difficultyLabel = new QLabel(QStringLiteral("СЛОЖНОСТЬ"), panel);
    difficultyLabel->setObjectName("fieldLabel");
    difficulty_ = new QComboBox(panel);
    difficulty_->addItem(QStringLiteral("Лёгкий — глубина 2"), 2);
    difficulty_->addItem(QStringLiteral("Средний — глубина 4"), 4);
    difficulty_->addItem(QStringLiteral("Сложный — глубина 6"), 6);
    difficulty_->addItem(QStringLiteral("Максимальный — глубина 8"), 8);
    difficulty_->setCurrentIndex(2);

    newGame_ = new QPushButton(QStringLiteral("＋  Новая партия"), panel);
    newGame_->setObjectName("primary");
    themeButton_ = new QPushButton(panel);
    themeButton_->setObjectName("secondary");
    soundButton_ = new QPushButton(panel);
    soundButton_->setObjectName("secondary");
    settingsButton_ = new QPushButton(QStringLiteral("Настройки"), panel);
    settingsButton_->setObjectName("textButton");
    exportButton_ = new QPushButton(QStringLiteral("Экспорт протокола"), panel);
    exportButton_->setObjectName("secondary");
    homeButton_ = new QPushButton(QStringLiteral("←  Главное меню"), panel);
    homeButton_->setObjectName("textButton");

    auto* divider2 = new QFrame(panel);
    divider2->setObjectName("separator");
    divider2->setFrameShape(QFrame::HLine);
    auto* historyTitle = new QLabel(QStringLiteral("ИСТОРИЯ ХОДОВ"), panel);
    historyTitle->setObjectName("sectionTitle");
    history_ = new QListWidget(panel);
    history_->setObjectName("historyList");
    history_->setMinimumHeight(90);
    stats_ = new QLabel(QStringLiteral("Поиск ещё не запускался"), panel);
    stats_->setObjectName("stats");
    stats_->setWordWrap(true);

    side->addWidget(brand);
    side->addWidget(title);
    side->addWidget(subtitle);
    side->addWidget(userLabel_);
    side->addSpacing(5);
    side->addWidget(divider1);
    side->addWidget(status_);
    side->addSpacing(3);
    side->addWidget(sideLabel);
    side->addWidget(sideChoice_);
    side->addWidget(difficultyLabel);
    side->addWidget(difficulty_);
    side->addSpacing(4);
    side->addWidget(newGame_);
    side->addWidget(themeButton_);
    side->addWidget(soundButton_);
    side->addWidget(settingsButton_);
    side->addWidget(exportButton_);
    side->addWidget(homeButton_);
    side->addSpacing(4);
    side->addWidget(divider2);
    side->addWidget(historyTitle);
    side->addWidget(history_, 1);
    side->addWidget(stats_);

    auto* drawerScroll = new QScrollArea(central);
    drawerScroll->setObjectName("drawerScroll");
    drawerScroll->setWidget(panel);
    drawerScroll->setWidgetResizable(true);
    drawerScroll->setFrameShape(QFrame::NoFrame);
    drawerScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    drawerScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    drawerScroll->setFixedWidth(0);
    gameDrawerScroll_ = drawerScroll;
    root->addWidget(drawerScroll);
    drawerAnimation_ = new QPropertyAnimation(drawerScroll, "maximumWidth", this);
    drawerAnimation_->setDuration(260);
    drawerAnimation_->setEasingCurve(QEasingCurve::OutCubic);
    connect(drawerToggle_, &QPushButton::clicked, this, [this] { toggleGameDrawer(); });
    gamePage_ = central;

    // A calmer, more deliberate start screen with a warm visual focal point.
    homePage_ = new QWidget(this);
    homePage_->setObjectName("homeBackground");
    auto* homeLayout = new QVBoxLayout(homePage_);
    homeLayout->setContentsMargins(34, 24, 34, 24);
    homeLayout->addStretch(1);

    auto* homeCard = new QFrame(homePage_);
    homeCard->setObjectName("homeCard");
    homeCard->setMaximumWidth(560);
    auto* homeCardLayout = new QVBoxLayout(homeCard);
    homeCardLayout->setContentsMargins(48, 36, 48, 34);
    homeCardLayout->setSpacing(13);

    auto* homeBrand = new QLabel(QStringLiteral("RUSSIAN CHECKERS  /  CLASSIC"), homeCard);
    homeBrand->setObjectName("brand");
    homeBrand->setAlignment(Qt::AlignCenter);
    auto* homeEmblem = new QLabel(QStringLiteral("♟"), homeCard);
    homeEmblem->setObjectName("homeEmblem");
    homeEmblem->setAlignment(Qt::AlignCenter);
    auto* homeTitle = new QLabel(QStringLiteral("Русские шашки"), homeCard);
    homeTitle->setObjectName("homeTitle");
    homeTitle->setAlignment(Qt::AlignCenter);
    auto* homeSubtitle = new QLabel(QStringLiteral("Спокойная партия. Чистая стратегия."), homeCard);
    homeSubtitle->setObjectName("homeSubtitle");
    homeSubtitle->setAlignment(Qt::AlignCenter);
    homeUserLabel_ = new QLabel(QStringLiteral("Вы вошли как  ·  %1").arg(username_), homeCard);
    homeUserLabel_->setObjectName("userLabel");
    homeUserLabel_->setAlignment(Qt::AlignCenter);

    homeStart_ = new QPushButton(QStringLiteral("Начать партию"), homeCard);
    homeStart_->setObjectName("primary");
    homeStart_->setMinimumHeight(56);
    auto* homeStats = new QPushButton(QStringLiteral("Моя статистика"), homeCard);
    homeStats->setObjectName("secondary");
    homeStats->setMinimumHeight(48);
    auto* homeSettings = new QPushButton(QStringLiteral("Настройки"), homeCard);
    homeSettings->setObjectName("secondary");
    homeSettings->setMinimumHeight(48);
    auto* homeTheme = new QPushButton(homeCard);
    homeTheme->setObjectName("textButton");
    homeTheme->setText(darkTheme_ ? QStringLiteral("Светлая тема") : QStringLiteral("Тёмная тема"));
    auto* homeFooter = new QLabel(QStringLiteral("РУССКИЕ ШАШКИ  ·  ВЕРСИЯ 0.7"), homePage_);
    homeFooter->setObjectName("homeFooter");
    homeFooter->setAlignment(Qt::AlignCenter);

    homeCardLayout->addWidget(homeBrand);
    homeCardLayout->addSpacing(2);
    homeCardLayout->addWidget(homeEmblem);
    homeCardLayout->addWidget(homeTitle);
    homeCardLayout->addWidget(homeSubtitle);
    homeCardLayout->addSpacing(7);
    homeCardLayout->addWidget(homeUserLabel_);
    homeCardLayout->addSpacing(9);
    homeCardLayout->addWidget(homeStart_);
    homeCardLayout->addWidget(homeStats);
    homeCardLayout->addWidget(homeSettings);
    homeCardLayout->addWidget(homeTheme);
    homeLayout->addWidget(homeCard, 0, Qt::AlignHCenter);
    homeLayout->addStretch(1);
    homeLayout->addWidget(homeFooter);

    pages_ = new QStackedWidget(this);
    authPage_ = new AuthDialog(database_, pages_);
    pages_->addWidget(authPage_);
    pages_->addWidget(homePage_);
    pages_->addWidget(gamePage_);
    pages_->setCurrentWidget(userId_ >= 0 ? homePage_ : static_cast<QWidget*>(authPage_));
    connect(authPage_, &AuthDialog::authenticated, this, [this](qint64 id, const QString& name) {
        userId_ = id;
        username_ = name;
        userLabel_->setText(QStringLiteral("Игрок: %1").arg(username_));
        homeUserLabel_->setText(QStringLiteral("Вы вошли как  ·  %1").arg(username_));
        pages_->setCurrentWidget(homePage_);
        statusBar()->showMessage(QStringLiteral("Добро пожаловать, %1").arg(username_), 3000);
    });
    setCentralWidget(pages_);
    statusBar()->setSizeGripEnabled(false);
    statusBar()->showMessage(QStringLiteral("Готовы к игре · Русские шашки"));

    connect(boardView_, &BoardWidget::squareClicked, this, &MainWindow::onSquareClicked);
    connect(newGame_, &QPushButton::clicked, this, &MainWindow::startNewGame);
    connect(themeButton_, &QPushButton::clicked, this, &MainWindow::toggleTheme);
    connect(soundButton_, &QPushButton::clicked, this, [this] { applySoundSettings(!soundsEnabled_, soundVolume_); });
    connect(settingsButton_, &QPushButton::clicked, this, &MainWindow::openSettings);
    connect(exportButton_, &QPushButton::clicked, this, &MainWindow::exportProtocol);
    connect(homeButton_, &QPushButton::clicked, this, [this] {
        toggleGameDrawer(false);
        showHome();
    });
    connect(homeStart_, &QPushButton::clicked, this, [this] {
        pages_->setCurrentWidget(gamePage_);
        if (gameId_ < 0 || gameResult_ != QStringLiteral("in_progress")) startNewGame();
        else refresh();
    });
    connect(homeStats, &QPushButton::clicked, this, &MainWindow::showStatistics);
    connect(homeSettings, &QPushButton::clicked, this, &MainWindow::openSettings);
    connect(homeTheme, &QPushButton::clicked, this, &MainWindow::toggleTheme);
    connect(&aiWatcher_, &QFutureWatcher<SearchResult>::finished, this, &MainWindow::onAiFinished);
    connect(sideChoice_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { if (!aiThinking_) startNewGame(); });

    QSettings settings;
    darkTheme_ = settings.value(QStringLiteral("appearance/darkTheme"), false).toBool();
    soundsEnabled_ = settings.value(QStringLiteral("audio/enabled"), true).toBool();
    soundVolume_ = settings.value(QStringLiteral("audio/volume"), 0.55).toReal();
    musicEnabled_ = settings.value(QStringLiteral("audio/musicEnabled"), false).toBool();
    musicVolume_ = settings.value(QStringLiteral("audio/musicVolume"), 0.10).toReal();
    animationsEnabled_ = settings.value(QStringLiteral("game/animationsEnabled"), true).toBool();
    animationSpeedPercent_ = std::clamp(settings.value(QStringLiteral("game/animationSpeedPercent"), 100).toInt(), 50, 150);
    boardView_->setAnimationSettings(animationsEnabled_, animationSpeedPercent_);
    moveSound_ = new QSoundEffect(this);
    captureSound_ = new QSoundEffect(this);
    victorySound_ = new QSoundEffect(this);
    defeatSound_ = new QSoundEffect(this);
    buttonSound_ = new QSoundEffect(this);
    const QString audioDirectory = createSoundPack();
    moveSound_->setSource(QUrl::fromLocalFile(audioDirectory + QStringLiteral("/wood_move.wav")));
    captureSound_->setSource(QUrl::fromLocalFile(audioDirectory + QStringLiteral("/wood_capture.wav")));
    buttonSound_->setSource(QUrl::fromLocalFile(audioDirectory + QStringLiteral("/button_click.wav")));
    victorySound_->setSource(QUrl(QStringLiteral("qrc:/sounds/victory.wav")));
    defeatSound_->setSource(QUrl(QStringLiteral("qrc:/sounds/defeat.wav")));
    applySoundSettings(soundsEnabled_, soundVolume_);
    musicOutput_ = new QAudioOutput(this);
    musicPlayer_ = new QMediaPlayer(this);
    musicPlayer_->setAudioOutput(musicOutput_);
    musicPlayer_->setSource(QUrl::fromLocalFile(audioDirectory + QStringLiteral("/calm_ambient.wav")));
    musicPlayer_->setLoops(QMediaPlayer::Infinite);
    musicOutput_->setVolume(musicVolume_);
    if (musicEnabled_) musicPlayer_->play();
    const auto buttons = findChildren<QPushButton*>();
    for (QPushButton* button : buttons) addButtonFeedback(button, buttonSound_);
    applyTheme(darkTheme_);
    for (QPushButton* button : {drawerToggle_}) addButtonFeedback(button, buttonSound_);
    connect(boardView_, &BoardWidget::animationFinished, this, [this] {
        if (aiThinking_ || gameOver()) return;
        if (sideToMove_ == humanSide_) refresh();
        else beginComputerTurn();
    });
}

MainWindow::~MainWindow() {
    if (gameId_ >= 0 && gameResult_ == QStringLiteral("in_progress")) {
        QString ignored;
        database_.finishGame(gameId_, QStringLiteral("abandoned"), &ignored);
    }
}

void MainWindow::startNewGame() {
    if (aiThinking_) return;
    if (gameResult_ == QStringLiteral("in_progress") && !historyMoves_.empty() && !abandonConfirmed_) {
        showInlinePrompt(QStringLiteral("Начать новую партию?"),
            QStringLiteral("Текущая партия ещё не завершена. Если продолжить, она будет отмечена как прерванная."),
            QStringLiteral("Начать новую"), QStringLiteral("Остаться в партии"),
            [this](bool accepted) { if (accepted) { abandonConfirmed_ = true; startNewGame(); } });
        return;
    }
    abandonConfirmed_ = false;
    if (gameId_ >= 0 && gameResult_ == QStringLiteral("in_progress")) {
        QString ignored;
        database_.finishGame(gameId_, QStringLiteral("abandoned"), &ignored);
    }
    board_.reset(); sideToMove_=Side::White; humanSide_=static_cast<Side>(sideChoice_->currentData().toInt());
    historyMoves_.clear(); history_->clear(); protocol_.clear(); selected_=-1; stats_->setText("Поиск ещё не запускался");
    gameResult_ = QStringLiteral("in_progress");
    positionCounts_.clear();
    ++positionCounts_[board_.positionKey(sideToMove_)];
    QString dbError;
    gameId_ = -1;
    if (userId_ >= 0) {
        gameId_ = database_.createGame(userId_, static_cast<int>(humanSide_), difficulty_->currentData().toInt(), &dbError);
        if (gameId_ < 0) showInlineMessage(QStringLiteral("База данных"), dbError);
    }
    refresh(); if(sideToMove_!=humanSide_) beginComputerTurn();
}

void MainWindow::refresh() {
    board_.generateMoves(sideToMove_,legalMoves_);
    boardView_->setPosition(board_,sideToMove_,legalMoves_); boardView_->setLastMove(historyMoves_.empty()?Move{}:historyMoves_.back());
    const QString whose=sideToMove_==humanSide_?"Ваш ход":"Ход компьютера";
    status_->setText(drawByRepetition() ? QStringLiteral("Ничья: троекратное повторение позиции")
                                      : (legalMoves_.empty() ? QStringLiteral("Партия окончена") : whose));
    boardView_->setInteractionEnabled(!aiThinking_ && sideToMove_==humanSide_ && !gameOver());
}

bool MainWindow::drawByRepetition() const {
    const auto it = positionCounts_.find(board_.positionKey(sideToMove_));
    return it != positionCounts_.end() && it->second >= 3;
}

bool MainWindow::gameOver() const { return legalMoves_.empty() || drawByRepetition(); }

void MainWindow::onSquareClicked(int square) {
    if(aiThinking_ || sideToMove_!=humanSide_) return;
    const Square clicked=static_cast<Square>(square);
    if(selected_>=0) {
        auto it=std::find_if(legalMoves_.begin(),legalMoves_.end(),[&](const Move& m){return m.from==selected_ && m.to()==clicked;});
        if(it!=legalMoves_.end()) { const Move move=*it; finishMove(move,true); return; }
    }
    if(board_.at(clicked)!=Piece::Empty && belongsTo(board_.at(clicked),humanSide_)) selected_=clicked;
    else selected_=-1;
    std::vector<Move> filtered;
    if(selected_>=0) for(const auto& m:legalMoves_) if(m.from==selected_) filtered.push_back(m);
    boardView_->setPosition(board_,sideToMove_,legalMoves_);
    boardView_->setSelection(selected_);
    // Selected source is represented by adding its legal destinations as target hints.
    boardView_->setLastMove(historyMoves_.empty()?Move{}:historyMoves_.back());
    if(selected_>=0) {
        // Reusing the widget's legal move list keeps destination validation in the game controller.
        boardView_->update();
        status_->setText(filtered.empty()?"У этой шашки нет доступных ходов":"Выберите подсвеченное поле назначения");
    } else refresh();
}

QString MainWindow::moveText(const Move& move) const {
    auto coord=[](Square s){int r,c;Board::toCoordinates(s,r,c);return QString("%1%2").arg(QChar('a'+c)).arg(8-r);};
    QString out=coord(move.from)+(move.isCapture?" : ":" — ");
    for(int i=0;i<move.length;++i){if(i)out+=" : ";out+=coord(move.landings[i]);}
    return out;
}

void MainWindow::finishMove(const Move& move, bool animate) {
    const int nextPly = static_cast<int>(historyMoves_.size()) + 1;
    const QString notation = QString("%1. %2%3")
        .arg((nextPly + 1) / 2).arg(moveText(move)).arg(move.isCapture ? QStringLiteral("  ×") : QString());

    // Persist first: never advance the visible game while silently losing a move
    // from the database. A failed write leaves the position and turn unchanged.
    if (gameId_ >= 0) {
        QString dbError;
        if (!database_.appendMove(gameId_, nextPly, notation, &dbError)) {
            showInlineMessage(QStringLiteral("Не удалось сохранить ход"),
                QStringLiteral("Ход не выполнен, потому что его не удалось записать в базу данных.\n\n%1").arg(dbError));
            return;
        }
    }

    Undo undo;
    if (!board_.makeMove(move, undo)) {
        // This should be unreachable for a move generated from the current board.
        // If it happens, surface the consistency problem rather than continuing.
        showInlineMessage(QStringLiteral("Ошибка игровой логики"),
            QStringLiteral("Не удалось применить допустимый ход. Перезапустите партию."));
        return;
    }
    playMoveSound(move.isCapture);
    historyMoves_.push_back(move);
    protocol_.push_back(notation);
    history_->addItem(notation);

    selected_ = -1;
    sideToMove_ = opposite(sideToMove_);
    ++positionCounts_[board_.positionKey(sideToMove_)];
    board_.generateMoves(sideToMove_, legalMoves_);

    if (animate) {
        Board before = board_;
        before.unmakeMove(undo);
        boardView_->setPosition(before, sideToMove_, legalMoves_);
        boardView_->setLastMove(historyMoves_.back());
        boardView_->animateMove(move, board_);
    } else {
        refresh();
    }

    if (gameOver()) {
        if (drawByRepetition()) {
            gameResult_ = QStringLiteral("draw");
            status_->setText(QStringLiteral("Ничья: троекратное повторение позиции"));
        } else {
            gameResult_ = sideToMove_ == humanSide_ ? QStringLiteral("loss") : QStringLiteral("win");
            status_->setText(sideToMove_ == humanSide_ ? QStringLiteral("Вы проиграли") : QStringLiteral("Вы выиграли!"));
        }
        if (gameId_ >= 0) {
            QString error;
            if (!database_.finishGame(gameId_, gameResult_, &error))
                showInlineMessage(QStringLiteral("База данных"),
                    QStringLiteral("Партия завершена, но результат не удалось сохранить:\n%1").arg(error));
        }
        if (gameResult_ == QStringLiteral("win")) { if (soundsEnabled_) victorySound_->play(); }
        else if (gameResult_ == QStringLiteral("loss")) { if (soundsEnabled_) defeatSound_->play(); }
        boardView_->setInteractionEnabled(false);
        const QString resultText = gameResult_ == QStringLiteral("win") ? QStringLiteral("Вы победили!")
            : gameResult_ == QStringLiteral("loss") ? QStringLiteral("Партия завершилась поражением.")
            : QStringLiteral("Партия завершилась вничью.");
        showInlineMessage(QStringLiteral("Партия завершена"), resultText);
    } else if (sideToMove_ != humanSide_ && !animate) {
        beginComputerTurn();
    } else if (sideToMove_ == humanSide_ && !animate) {
        refresh();
    }
}

void MainWindow::beginComputerTurn() {
    if(aiThinking_ || sideToMove_==humanSide_) return;
    aiThinking_=true; boardView_->setInteractionEnabled(false); status_->setText("Компьютер думает…");
    const Board position=board_; const Side side=sideToMove_; const int depth=difficulty_->currentData().toInt();
    aiWatcher_.setFuture(QtConcurrent::run([position,side,depth]() mutable {
        AI ai; SearchLimits limits; limits.maxDepth=depth; limits.timeLimitMs=1800;
        return ai.findBestMove(position,side,limits);
    }));
}

void MainWindow::onAiFinished() {
    aiThinking_=false; const SearchResult result=aiWatcher_.result();
    if(!result.hasMove) {
        refresh(); status_->setText("Компьютер не может ходить. Вы выиграли!");
        gameResult_ = QStringLiteral("win");
        if (gameId_ >= 0) { QString error; database_.finishGame(gameId_, gameResult_, &error); }
        if (soundsEnabled_) victorySound_->play();
        boardView_->setInteractionEnabled(false);
        showInlineMessage(QStringLiteral("Партия завершена"), QStringLiteral("Компьютер не может сделать ход. Вы победили!"));
        return;
    }
    stats_->setText(QString("Глубина: %1\nУзлы: %2\nВремя: %3 мс\nСкорость: %4 уз/с")
        .arg(result.completedDepth).arg(result.nodes+result.qnodes).arg(result.elapsedMs).arg(result.nodesPerSecond));
    finishMove(result.bestMove,true);
}

void MainWindow::toggleTheme() {
    applyTheme(!darkTheme_);
}

void MainWindow::toggleGameDrawer(bool open) {
    if (!gameDrawerScroll_ || !drawerAnimation_) return;
    if (open == drawerOpen_) return;
    drawerOpen_ = open;
    drawerToggle_->setText(drawerOpen_ ? QStringLiteral("×") : QStringLiteral("☰"));
    drawerToggle_->setToolTip(drawerOpen_ ? QStringLiteral("Скрыть меню партии")
                                          : QStringLiteral("Открыть меню партии"));
    drawerAnimation_->stop();
    drawerAnimation_->setStartValue(gameDrawerScroll_->maximumWidth());
    drawerAnimation_->setEndValue(drawerOpen_ ? 326 : 0);
    drawerAnimation_->start();
}
void MainWindow::toggleGameDrawer() {
    toggleGameDrawer(!drawerOpen_);
}

void MainWindow::applyTheme(bool dark) {
    darkTheme_ = dark;
    QSettings().setValue(QStringLiteral("appearance/darkTheme"), darkTheme_);
    boardView_->setDarkTheme(darkTheme_);
    themeButton_->setText(darkTheme_ ? QStringLiteral("Светлая тема") : QStringLiteral("Тёмная тема"));
    if (homePage_) { const auto labels = homePage_->findChildren<QPushButton*>(); for (auto* b : labels) if (b->text().contains(QStringLiteral("тема"))) b->setText(darkTheme_ ? QStringLiteral("Светлая тема") : QStringLiteral("Тёмная тема")); }
    soundButton_->setText(soundsEnabled_ ? QStringLiteral("Звук включён") : QStringLiteral("Звук выключен"));

    if (darkTheme_) {
        setStyleSheet(R"(
            QMainWindow, QWidget#appBackground { background: #241812; color: #f0e2cf; font-family: "Segoe UI"; font-size: 13px; }
            QPushButton#drawerToggle { background: #ead8c2; color: #3b271d; border: 1px solid #c5a98b; border-radius: 13px; font-size: 22px; font-weight: 700; margin: 14px; }
            QPushButton#drawerToggle:hover { background: #fff0dc; }
            QScrollArea#drawerScroll { background: transparent; border: 0; }
            QWidget#homeBackground { background: qradialgradient(cx:0.82, cy:0.16, radius:0.95, fx:0.82, fy:0.16, stop:0 #72513b, stop:0.46 #3b281f, stop:1 #1d130f); }
            QFrame#homeCard { background: rgba(27, 36, 31, 238); border: 1px solid #c5a98b; border-radius: 26px; }
            QLabel#homeEmblem { color: #b58c68; font-size: 72px; }
            QLabel#homeTitle { color: #3a2419; font-size: 36px; font-weight: 750; }
            QLabel#homeSubtitle { color: #725b4b; font-size: 14px; }
            QLabel#homeFooter { color: #98765c; font-size: 10px; font-weight: 600; letter-spacing: 2px; }
            QFrame#sidePanel QLabel { color: #f0e2cf; }
            QFrame#sidePanel QLabel#brand { color: #c7a17e; }
            QFrame#sidePanel QLabel#muted { color: #d7c0a7; }
            QFrame#sidePanel QLabel#fieldLabel, QFrame#sidePanel QLabel#sectionTitle { color: #d8c1a8; }
            QFrame#sidePanel QLabel#stats { color: #d8c1a8; }
            QFrame#sidePanel QLabel#userLabel { color: #f0e2cf; }
            QFrame#sidePanel QFormLayout QLabel { color: #f0e2cf; }
            QFrame#sidePanel QPushButton { font-size: 13px; min-height: 24px; padding: 12px 14px; border-radius: 12px; }
            QFrame#sidePanel QPushButton#primary { color: #18231b; }
            QFrame#sidePanel QPushButton#secondary { color: #ecdbc6; }
            QFrame#sidePanel QPushButton#textButton { color: #c5d0c7; }

            QFrame#sidePanel { background: #f5eadb; border: 1px solid #d8c2a8; border-radius: 22px; }
            QLabel#brand { color: #a47a58; font-size: 10px; font-weight: 700; letter-spacing: 2px; }
            QLabel#title { color: #3b271d; font-size: 25px; font-weight: 700; }
            QLabel#muted { color: #806653; font-size: 12px; }
            QLabel#userLabel { color: #4e382a; background: #ead8c2; border-radius: 9px; padding: 8px 10px; font-size: 12px; }
            QLabel#fieldLabel, QLabel#sectionTitle { color: #8a6b52; font-size: 10px; font-weight: 700; letter-spacing: 1.2px; }
            QLabel#status { background: #ead8c2; color: #3c281d; border: 1px solid #d1b99d; border-radius: 11px; padding: 13px; font-size: 14px; font-weight: 600; }
            QLabel#stats { color: #8b7059; background: #e9d7c1; border-radius: 9px; padding: 10px; font-size: 11px; }
            QFrame#separator { color: #303946; max-height: 1px; }
            QComboBox, QListWidget { background: #e9d7c1; color: #edf0f3; border: 1px solid #35404d; border-radius: 9px; padding: 9px 11px; min-height: 22px; selection-background-color: #9b7252; }
            QComboBox:hover, QListWidget:hover { border-color: #68786d; }
            QComboBox::drop-down { border: 0; width: 25px; }
            QComboBox QAbstractItemView { background: #202630; color: #edf0f3; selection-background-color: #7b5237; border: 1px solid #d1b99d; }
            QListWidget::item { padding: 7px 5px; border-bottom: 1px solid #29323d; }
            QListWidget::item:selected { background: #303d39; border-radius: 5px; }
            QPushButton { border: 0; border-radius: 9px; padding: 11px 12px; font-weight: 600; min-height: 20px; }
            QPushButton#primary { background: #8b5e3c; color: #18201b; }
            QPushButton#primary:hover { background: #a87850; }
            QPushButton#primary:pressed { background: #71482f; }
            QPushButton#secondary { background: #ead8c2; color: #4c3528; border: 1px solid #d6bea3; }
            QCheckBox { spacing: 10px; color: #4c3528; }
            QSlider::groove:horizontal { height: 5px; background: #d6bea3; border-radius: 2px; }
            QSlider::handle:horizontal { background: #8b5e3c; width: 15px; margin: -5px 0; border-radius: 7px; }
            QPushButton#secondary:hover { background: #dcc4a9; }
            QPushButton#textButton { background: transparent; color: #8a6b52; }
            QPushButton#textButton:hover { background: #ead8c2; color: #edf0f3; }
            QStatusBar { background: #141920; color: #8f9aa6; font-size: 11px; }
            QToolTip { background: #ead8c2; color: #edf0f3; border: 1px solid #9b7252; padding: 6px 9px; border-radius: 6px; }
        )");
    } else {
        setStyleSheet(R"(
            QMainWindow, QWidget#appBackground { background: #f4eadc; color: #3c281d; font-family: "Segoe UI"; font-size: 13px; }
            QPushButton#drawerToggle { background: #fff9ef; color: #5a3925; border: 1px solid #d6bea2; border-radius: 13px; font-size: 22px; font-weight: 700; margin: 14px; }
            QPushButton#drawerToggle:hover { background: #ead8c2; }
            QScrollArea#drawerScroll { background: transparent; border: 0; }
            QWidget#homeBackground { background: qradialgradient(cx:0.82, cy:0.16, radius:0.95, fx:0.82, fy:0.16, stop:0 #e9d6bd, stop:0.48 #f0e1cc, stop:1 #e6d0b5); }
            QFrame#homeCard { background: rgba(255, 255, 251, 246); border: 1px solid #d6bea2; border-radius: 26px; }
            QLabel#homeEmblem { color: #9b6b47; font-size: 72px; }
            QLabel#homeTitle { color: #3b271d; font-size: 36px; font-weight: 750; }
            QLabel#homeSubtitle { color: #765a45; font-size: 14px; }
            QLabel#homeFooter { color: #8a6b50; font-size: 10px; font-weight: 600; letter-spacing: 2px; }
            QFrame#sidePanel QLabel { color: #3b271d; }
            QFrame#sidePanel QLabel#brand { color: #946b49; }
            QFrame#sidePanel QLabel#muted { color: #586b5d; }
            QFrame#sidePanel QLabel#fieldLabel, QFrame#sidePanel QLabel#sectionTitle { color: #805e43; }
            QFrame#sidePanel QLabel#stats { color: #506253; }
            QFrame#sidePanel QLabel#userLabel { color: #60452f; }
            QFrame#sidePanel QFormLayout QLabel { color: #3b271d; }
            QFrame#sidePanel QPushButton { font-size: 13px; min-height: 24px; padding: 12px 14px; border-radius: 12px; }
            QFrame#sidePanel QPushButton#primary { color: #fff9ef; }
            QFrame#sidePanel QPushButton#secondary { color: #405845; }
            QFrame#sidePanel QPushButton#textButton { color: #536a57; }

            QFrame#sidePanel { background: #fff9ef; border: 1px solid #e1e7df; border-radius: 22px; }
            QLabel#brand { color: #718276; font-size: 10px; font-weight: 700; letter-spacing: 2px; }
            QLabel#title { color: #29332d; font-size: 25px; font-weight: 700; }
            QLabel#muted { color: #7c877f; font-size: 12px; }
            QLabel#userLabel { color: #56645a; background: #ecdbc6; border-radius: 9px; padding: 8px 10px; font-size: 12px; }
            QLabel#fieldLabel, QLabel#sectionTitle { color: #8c7058; font-size: 10px; font-weight: 700; letter-spacing: 1.2px; }
            QLabel#status { background: #f0f4f0; color: #684a32; border: 1px solid #dfc9ae; border-radius: 11px; padding: 13px; font-size: 14px; font-weight: 600; }
            QLabel#stats { color: #748078; background: #f0e2d0; border-radius: 9px; padding: 10px; font-size: 11px; }
            QFrame#separator { color: #e8ece7; max-height: 1px; }
            QComboBox, QListWidget { background: #fff9ef; color: #3c281d; border: 1px solid #e0e6df; border-radius: 9px; padding: 9px 11px; min-height: 22px; selection-background-color: #e8d3b8; }
            QComboBox:hover, QListWidget:hover { border-color: #b58b66; }
            QComboBox::drop-down { border: 0; width: 25px; }
            QComboBox QAbstractItemView { background: #fff9ef; color: #3c281d; selection-background-color: #ecd9c1; border: 1px solid #e0e6df; }
            QListWidget::item { padding: 7px 5px; border-bottom: 1px solid #e7d5bf; }
            QListWidget::item:selected { background: #ead7bf; border-radius: 5px; }
            QPushButton { border: 0; border-radius: 9px; padding: 11px 12px; font-weight: 600; min-height: 20px; }
            QPushButton#primary { background: #8b5a38; color: #fff9ef; }
            QPushButton#primary:hover { background: #a16e47; }
            QPushButton#primary:pressed { background: #70462d; }
            QPushButton#secondary { background: #f1e3d0; color: #536257; border: 1px solid #d9c3a7; }
            QCheckBox { spacing: 10px; color: #684a32; }
            QSlider::groove:horizontal { height: 5px; background: #d9c3a7; border-radius: 2px; }
            QSlider::handle:horizontal { background: #8b5a38; width: 15px; margin: -5px 0; border-radius: 7px; }
            QPushButton#secondary:hover { background: #ead8c2; }
            QPushButton#textButton { background: transparent; color: #78867b; }
            QPushButton#textButton:hover { background: #ecdbc6; color: #684a32; }
            QStatusBar { background: #e9ede8; color: #78847b; font-size: 11px; }
            QToolTip { background: #fff9ef; color: #3c281d; border: 1px solid #dce5dc; padding: 6px 9px; border-radius: 6px; }
        )");
    }
}

void MainWindow::applySoundSettings(bool enabled, qreal volume) {
    soundsEnabled_ = enabled;
    soundVolume_ = std::clamp(volume, qreal(0.0), qreal(1.0));
    for (QSoundEffect* effect : {moveSound_, captureSound_, victorySound_, defeatSound_}) {
        if (effect) effect->setVolume(soundVolume_);
    }
    if (buttonSound_) buttonSound_->setVolume(soundVolume_ * 0.42);
    if (buttonSound_) buttonSound_->setMuted(!soundsEnabled_);
    QSettings settings;
    settings.setValue(QStringLiteral("audio/enabled"), soundsEnabled_);
    settings.setValue(QStringLiteral("audio/volume"), soundVolume_);
    if (soundButton_) soundButton_->setText(soundsEnabled_ ? QStringLiteral("Звук включён") : QStringLiteral("Звук выключен"));
}

void MainWindow::playMoveSound(bool capture) {
    if (!soundsEnabled_) return;
    QSoundEffect* effect = capture ? captureSound_ : moveSound_;
    if (effect) effect->play();
}

void MainWindow::openSettings() {
    pageBeforeSettings_ = pages_->currentWidget();
    if (!settingsPage_) {
        settingsPage_ = new QWidget(pages_);
        settingsPage_->setObjectName("appBackground");
        auto* outer = new QVBoxLayout(settingsPage_);
        outer->setContentsMargins(64, 42, 64, 42);
        outer->addStretch(1);

        auto* card = new QFrame(settingsPage_);
        card->setObjectName("sidePanel");
        card->setMaximumWidth(620);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(34, 30, 34, 30);
        cardLayout->setSpacing(18);

        auto* eyebrow = new QLabel(QStringLiteral("RUSSIAN CHECKERS  /  ПАРАМЕТРЫ"), card);
        eyebrow->setObjectName("brand");
        auto* heading = new QLabel(QStringLiteral("Настройки"), card);
        heading->setObjectName("title");
        auto* description = new QLabel(QStringLiteral("Настройте внешний вид и звуковое сопровождение игры."), card);
        description->setObjectName("muted");
        auto* separator = new QFrame(card);
        separator->setObjectName("separator");
        separator->setFrameShape(QFrame::HLine);

        auto* form = new QFormLayout;
        form->setHorizontalSpacing(24);
        form->setVerticalSpacing(16);
        themeSetting_ = new QComboBox(card);
        themeSetting_->setObjectName("settingsTheme");
        themeSetting_->addItem(QStringLiteral("Светлая тема"), false);
        themeSetting_->addItem(QStringLiteral("Тёмная тема"), true);
        soundSetting_ = new QCheckBox(QStringLiteral("Звуки ходов и взятий"), card);
        soundSetting_->setObjectName("settingsSound");
        soundVolumeSetting_ = new QSlider(Qt::Horizontal, card);
        soundVolumeSetting_->setRange(0, 100);
        musicSetting_ = new QCheckBox(QStringLiteral("Фоновая музыка"), card);
        musicSetting_->setObjectName("settingsMusic");
        musicVolumeSetting_ = new QSlider(Qt::Horizontal, card);
        musicVolumeSetting_->setRange(0, 30);
        animationSetting_ = new QCheckBox(QStringLiteral("Плавные анимации ходов"), card);
        animationSetting_->setObjectName("settingsAnimations");
        animationSpeedSetting_ = new QSlider(Qt::Horizontal, card);
        animationSpeedSetting_->setRange(50, 150);
        animationSpeedSetting_->setToolTip(QStringLiteral("50% — медленнее, 150% — быстрее"));
        form->addRow(QStringLiteral("Оформление"), themeSetting_);
        form->addRow(QStringLiteral("Звуковые эффекты"), soundSetting_);
        form->addRow(QStringLiteral("Громкость эффектов"), soundVolumeSetting_);
        form->addRow(QStringLiteral("Музыка"), musicSetting_);
        form->addRow(QStringLiteral("Громкость музыки"), musicVolumeSetting_);
        form->addRow(QStringLiteral("Анимация"), animationSetting_);
        form->addRow(QStringLiteral("Скорость анимации"), animationSpeedSetting_);

        auto* actions = new QHBoxLayout;
        actions->addStretch(1);
        auto* back = new QPushButton(QStringLiteral("Назад"), card);
        back->setObjectName("secondary");
        auto* save = new QPushButton(QStringLiteral("Сохранить настройки"), card);
        save->setObjectName("primary");
        actions->addWidget(back);
        actions->addWidget(save);

        cardLayout->addWidget(eyebrow);
        cardLayout->addWidget(heading);
        cardLayout->addWidget(description);
        cardLayout->addWidget(separator);
        cardLayout->addLayout(form);
        cardLayout->addSpacing(8);
        cardLayout->addLayout(actions);
        outer->addWidget(card, 0, Qt::AlignHCenter);
        outer->addStretch(1);
        pages_->addWidget(settingsPage_);

        addButtonFeedback(back, buttonSound_);
        addButtonFeedback(save, buttonSound_);
        connect(back, &QPushButton::clicked, this, [this] {
            pages_->setCurrentWidget(pageBeforeSettings_ ? pageBeforeSettings_ : homePage_);
        });
        connect(save, &QPushButton::clicked, this, [this] {
            applyTheme(themeSetting_->currentData().toBool());
            applySoundSettings(soundSetting_->isChecked(), soundVolumeSetting_->value() / 100.0);
            musicEnabled_ = musicSetting_->isChecked();
            musicVolume_ = musicVolumeSetting_->value() / 100.0;
            animationsEnabled_ = animationSetting_->isChecked();
            animationSpeedPercent_ = animationSpeedSetting_->value();
            boardView_->setAnimationSettings(animationsEnabled_, animationSpeedPercent_);
            if (musicOutput_) musicOutput_->setVolume(musicVolume_);
            if (musicPlayer_) { if (musicEnabled_) musicPlayer_->play(); else musicPlayer_->stop(); }
            QSettings().setValue(QStringLiteral("audio/musicEnabled"), musicEnabled_);
            QSettings().setValue(QStringLiteral("audio/musicVolume"), musicVolume_);
            QSettings().setValue(QStringLiteral("game/animationsEnabled"), animationsEnabled_);
            QSettings().setValue(QStringLiteral("game/animationSpeedPercent"), animationSpeedPercent_);
            pages_->setCurrentWidget(pageBeforeSettings_ ? pageBeforeSettings_ : homePage_);
            statusBar()->showMessage(QStringLiteral("Настройки сохранены"), 2600);
        });
    }

    themeSetting_->setCurrentIndex(darkTheme_ ? 1 : 0);
    soundSetting_->setChecked(soundsEnabled_);
    soundVolumeSetting_->setValue(qRound(soundVolume_ * 100.0));
    musicSetting_->setChecked(musicEnabled_);
    musicVolumeSetting_->setValue(qRound(musicVolume_ * 100.0));
    animationSetting_->setChecked(animationsEnabled_);
    animationSpeedSetting_->setValue(animationSpeedPercent_);
    pages_->setCurrentWidget(settingsPage_);
}
void MainWindow::showHome() {
    if (aiThinking_) return;
    if (homeStart_) homeStart_->setText(!historyMoves_.empty() && gameResult_ == QStringLiteral("in_progress")
        ? QStringLiteral("Продолжить игру") : QStringLiteral("Начать игру"));
    pages_->setCurrentWidget(homePage_);
}

void MainWindow::showInlineMessage(const QString& title, const QString& message) {
    showInlinePrompt(title, message, QStringLiteral("Понятно"), QString(), [](bool) {});
}

void MainWindow::showInlinePrompt(const QString& title, const QString& message,
                                  const QString& acceptText, const QString& cancelText,
                                  std::function<void(bool)> callback) {
    QWidget* returnPage = (messagePage_ && messageReturnPage_)
        ? messageReturnPage_ : pages_->currentWidget();
    if (messagePage_) {
        pages_->removeWidget(messagePage_);
        messagePage_->deleteLater();
        messagePage_ = nullptr;
    }
    messageReturnPage_ = returnPage;
    messagePage_ = new QWidget(pages_);
    messagePage_->setObjectName("appBackground");
    auto* outer = new QVBoxLayout(messagePage_);
    outer->setContentsMargins(48, 36, 48, 36);
    outer->addStretch(1);

    auto* card = new QFrame(messagePage_);
    card->setObjectName("sidePanel");
    card->setMaximumWidth(620);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(34, 30, 34, 30);
    layout->setSpacing(18);

    auto* eyebrow = new QLabel(QStringLiteral("RUSSIAN CHECKERS  /  СООБЩЕНИЕ"), card);
    eyebrow->setObjectName("brand");
    auto* heading = new QLabel(title, card);
    heading->setObjectName("title");
    heading->setWordWrap(true);
    auto* body = new QLabel(message, card);
    body->setObjectName("muted");
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->setMinimumWidth(280);

    auto* actions = new QHBoxLayout;
    actions->addStretch(1);
    auto* accept = new QPushButton(acceptText, card);
    accept->setObjectName("primary");
    actions->addWidget(accept);
    QPushButton* cancel = nullptr;
    if (!cancelText.isEmpty()) {
        cancel = new QPushButton(cancelText, card);
        cancel->setObjectName("secondary");
        actions->insertWidget(0, cancel);
    }

    layout->addWidget(eyebrow);
    layout->addWidget(heading);
    layout->addWidget(body);
    layout->addSpacing(8);
    layout->addLayout(actions);
    outer->addWidget(card, 0, Qt::AlignHCenter);
    outer->addStretch(1);
    pages_->addWidget(messagePage_);
    pages_->setCurrentWidget(messagePage_);

    auto finish = [this, callback](bool accepted) {
        QWidget* returnPage = messageReturnPage_ ? messageReturnPage_ : homePage_;
        QWidget* page = messagePage_;
        messagePage_ = nullptr;
        messageReturnPage_ = nullptr;
        if (page) {
            pages_->setCurrentWidget(returnPage);
            pages_->removeWidget(page);
            page->deleteLater();
        }
        if (callback) callback(accepted);
    };
    addButtonFeedback(accept, buttonSound_);
    if (cancel) addButtonFeedback(cancel, buttonSound_);
    connect(accept, &QPushButton::clicked, this, [finish] { finish(true); });
    if (cancel) connect(cancel, &QPushButton::clicked, this, [finish] { finish(false); });
}

void MainWindow::showStatistics() {
    if (userId_ < 0) {
        showInlineMessage(QStringLiteral("Статистика"),
            QStringLiteral("В гостевом режиме статистика не сохраняется. Войдите в аккаунт, чтобы вести историю партий."));
        return;
    }
    QString error;
    const QString report = database_.userStatistics(userId_, &error);
    showInlineMessage(QStringLiteral("Моя статистика · %1").arg(username_),
        error.isEmpty() ? report : error);
}

void MainWindow::exportProtocol() {
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Сохранить протокол"),
        QStringLiteral("checkers_game.txt"), QStringLiteral("Текстовые файлы (*.txt)"));
    if (path.isEmpty()) return;
    QString error;
    if (!database_.saveProtocol(path, username_, protocol_, gameResult_, &error))
        showInlineMessage(QStringLiteral("Экспорт не выполнен"), error);
    else showInlineMessage(QStringLiteral("Экспорт"), QStringLiteral("Протокол сохранён."));
}
