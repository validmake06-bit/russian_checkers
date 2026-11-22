#include "MainWindow.h"
#include "AuthDialog.h"
#include "../db/Database.h"
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QTextEdit>
#include <QDateTime>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QPushButton>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QGridLayout>
#include <QScrollArea>
#include <QScrollBar>

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
#include <QPainter>
#include <QTimer>
#include <QProgressBar>
#include <QVariantAnimation>
#include <QFontDatabase>
#include <QApplication>
#include <QGraphicsOpacityEffect>
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
    // Avoid QGraphicsOpacityEffect on interactive controls: page transitions and
    // overlapping pressed/released animations could leave a button transparent.
    // The widget stylesheet already provides hover/pressed visual feedback.
    QObject::connect(button, &QPushButton::clicked, button, [clickSound] {
        if (clickSound) clickSound->play();
    });
}

class CheckersBackdrop final : public QWidget {
public:
    explicit CheckersBackdrop(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName("homeBackground");
        setAttribute(Qt::WA_OpaquePaintEvent);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const bool dark = property("darkTheme").toBool();
        QLinearGradient base(0, 0, width(), height());
        if (dark) {
            base.setColorAt(0.0, QColor("#111820"));
            base.setColorAt(0.55, QColor("#202a2b"));
            base.setColorAt(1.0, QColor("#34251d"));
        } else {
            base.setColorAt(0.0, QColor("#f5eee3"));
            base.setColorAt(0.55, QColor("#eee0cc"));
            base.setColorAt(1.0, QColor("#e4d0b7"));
        }
        p.fillRect(rect(), base);
        QRadialGradient ambient(QPointF(width() * 0.76, height() * 0.44), qMax(width(), height()) * 0.62);
        ambient.setColorAt(0.0, dark ? QColor(198, 151, 105, 38) : QColor(166, 126, 87, 32));
        ambient.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.fillRect(rect(), ambient);

        // A quiet, oversized board motif gives the menu depth without competing
        // with the controls or creating a high-frequency visual distraction.
        const qreal boardSide = qMin(width(), height()) * 0.88;
        const QPointF boardCenter(width() * 0.78, height() * 0.48);
        const qreal cell = boardSide / 8.0;
        p.save();
        p.translate(boardCenter);
        p.rotate(-17.0);
        p.setPen(Qt::NoPen);
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                if (((row + col) & 1) == 0) continue;
                const int alpha = dark ? 12 : 19;
                QColor squareColor(dark ? "#d6b38c" : "#80583b");
                squareColor.setAlpha(alpha);
                p.setBrush(squareColor);
                p.drawRect(QRectF(-boardSide / 2 + col * cell,
                                  -boardSide / 2 + row * cell, cell, cell));
            }
        }
        p.restore();

        static constexpr qreal xFactors[] = {0.08, 0.22, 0.36, 0.50, 0.64, 0.78, 0.92};
        static constexpr qreal yFactors[] = {0.18, 0.285, 0.39, 0.495, 0.60, 0.705, 0.81};
        for (int i = 0; i < 7; ++i) {
            const qreal x = width() * xFactors[i];
            const qreal y = height() * yFactors[i];
            const qreal radius = 13.0 + (i % 3) * 3.0;
            QColor pieceColor(dark ? "#d6b38c" : "#80583b");
            pieceColor.setAlpha(dark ? 27 : 32);
            QRadialGradient piece(QPointF(x - radius * .3, y - radius * .35), radius * 1.5);
            QColor highlight = dark ? QColor("#f0d9bb") : QColor("#fffaf1");
            highlight.setAlpha(dark ? 35 : 70);
            QColor edge = pieceColor.darker(145);
            edge.setAlpha(dark ? 34 : 42);
            piece.setColorAt(0.0, highlight);
            piece.setColorAt(0.72, pieceColor);
            piece.setColorAt(1.0, edge);
            p.setBrush(piece);
            p.setPen(QPen(QColor(dark ? 224 : 117, dark ? 195 : 84,
                                 dark ? 157 : 54, dark ? 50 : 65), 1.1));
            p.drawEllipse(QPointF(x, y), radius, radius);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(dark ? 255 : 255, dark ? 239 : 255,
                                 dark ? 220 : 255, dark ? 38 : 75), 1.0));
            p.drawEllipse(QPointF(x, y), radius * 0.68, radius * 0.68);
        }
    }

};

class WinRateRing final : public QWidget {
public:
    explicit WinRateRing(int percent, QWidget* parent = nullptr)
        : QWidget(parent), percent_(qBound(0, percent, 100)) {
        setMinimumSize(136, 136);
        setMaximumSize(148, 148);
        auto* reveal = new QVariantAnimation(this);
        reveal->setDuration(850);
        reveal->setStartValue(0);
        reveal->setEndValue(percent_);
        reveal->setEasingCurve(QEasingCurve::OutCubic);
        connect(reveal, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            currentPercent_ = value.toInt();
            update();
        });
        reveal->start(QAbstractAnimation::DeleteWhenStopped);
    }
    void setDarkTheme(bool dark) { dark_ = dark; update(); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal side = qMin(width(), height()) - 20.0;
        const QRectF circle((width() - side) / 2.0, (height() - side) / 2.0, side, side);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(dark_ ? QColor("#3a473e") : QColor("#e6dacb"), 10.0, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(circle, 90 * 16, -360 * 16);
        p.setPen(QPen(dark_ ? QColor("#d5a777") : QColor("#8c6444"), 10.0, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(circle, 90 * 16, -currentPercent_ * 360 * 16 / 100);
        p.setPen(dark_ ? QColor("#f4eadc") : QColor("#3c3026"));
        QFont font = p.font();
        font.setPointSize(22);
        font.setWeight(QFont::DemiBold);
        p.setFont(font);
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("%1%").arg(currentPercent_));
    }
private:
    int percent_{0};
    int currentPercent_{0};
    bool dark_{true};
};

QString formatPlayTime(qint64 seconds, bool detailed = false) {
    seconds = qMax<qint64>(0, seconds);
    if (seconds < 60)
        return detailed ? QStringLiteral("%1 сек").arg(seconds)
                        : (seconds == 0 ? QStringLiteral("0 мин") : QStringLiteral("меньше минуты"));
    const qint64 minutes = seconds / 60;
    const qint64 hours = minutes / 60;
    if (hours >= 24)
        return QStringLiteral("%1 д %2 ч").arg(hours / 24).arg(hours % 24);
    if (hours > 0)
        return QStringLiteral("%1 ч %2 мин").arg(hours).arg(minutes % 60);
    if (detailed)
        return QStringLiteral("%1 мин %2 сек").arg(minutes).arg(seconds % 60, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1 мин").arg(minutes);
}
} // namespace

MainWindow::MainWindow(Database& database, qint64 userId, QString username, QWidget* parent)
    : QMainWindow(parent), database_(database), userId_(userId), username_(std::move(username)) {
    setWindowTitle(QStringLiteral("Русские шашки — против компьютера"));
    QFontDatabase fontDatabase;
    const QStringList installedFonts = fontDatabase.families();
    QString selectedFont = QStringLiteral("Segoe UI");
    for (const QString& candidate : {QStringLiteral("Inter"), QStringLiteral("Segoe UI Variable"),
                                     QStringLiteral("Aptos"), QStringLiteral("Segoe UI"),
                                     QStringLiteral("Noto Sans")}) {
        if (installedFonts.contains(candidate, Qt::CaseInsensitive)) { selectedFont = candidate; break; }
    }
    QFont applicationFont(selectedFont);
    applicationFont.setPointSize(10);
    QApplication::setFont(applicationFont);
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

    drawerToggle_ = new QToolButton(boardContainer);
    drawerToggle_->setObjectName("drawerToggle");
    drawerToggle_->setText(QStringLiteral("☰"));
    drawerToggle_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    drawerToggle_->setFixedSize(48, 48);
    drawerToggle_->setCursor(Qt::PointingHandCursor);
    drawerToggle_->setFocusPolicy(Qt::NoFocus);
    drawerToggle_->setToolTip(QStringLiteral("Открыть меню партии"));
    boardGrid->addWidget(drawerToggle_, 0, 0, Qt::AlignTop | Qt::AlignLeft);
    drawerToggle_->raise();
    root->addWidget(boardContainer, 1);

    auto* panel = new QFrame(central);
    panel->setObjectName("sidePanel");
    panel->setFixedWidth(326);
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
    newGame_->setMinimumHeight(48);
    newGame_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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
    drawerScroll->setStyleSheet("QScrollArea#drawerScroll { border: 0; background: transparent; } QScrollBar:vertical { width: 0px; }");
    drawerScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    drawerScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    drawerScroll->setFixedWidth(0);
    gameDrawerScroll_ = drawerScroll;
    root->addWidget(drawerScroll);
    drawerAnimation_ = new QPropertyAnimation(drawerScroll, "maximumWidth", this);
    drawerAnimation_->setDuration(230);
    drawerAnimation_->setEasingCurve(QEasingCurve::OutCubic);
    connect(drawerToggle_, &QPushButton::clicked, this, [this] { toggleGameDrawer(); });
    gamePage_ = central;

    // A calmer, more deliberate start screen with a warm visual focal point.
    homePage_ = new CheckersBackdrop(this);
    auto* homeLayout = new QVBoxLayout(homePage_);
    homeLayout->setContentsMargins(34, 24, 34, 24);
    homeLayout->addStretch(1);

    auto* homeCard = new QFrame(homePage_);
    homeCard->setObjectName("homeCard");
    homeCard->setMaximumWidth(560);
    auto* homeShadow = new QGraphicsDropShadowEffect(homeCard);
    homeShadow->setBlurRadius(46);
    homeShadow->setOffset(0, 17);
    homeShadow->setColor(QColor(20, 14, 9, 74));
    homeCard->setGraphicsEffect(homeShadow);
    auto* homeCardLayout = new QVBoxLayout(homeCard);
    homeCardLayout->setContentsMargins(48, 36, 48, 34);
    homeCardLayout->setSpacing(13);

    auto* homeBrand = new QLabel(QStringLiteral("RUSSIAN CHECKERS  /  CLASSIC"), homeCard);
    homeBrand->setObjectName("brand");
    homeBrand->setAlignment(Qt::AlignCenter);
    auto* homeAccent = new QFrame(homeCard);
    homeAccent->setObjectName("homeAccent");
    homeAccent->setFixedSize(84, 3);
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
    homeLogin_ = new QPushButton(QStringLiteral("Войти в аккаунт"), homeCard);
    homeLogin_->setObjectName("secondary");
    homeLogin_->setMinimumHeight(48);
    homeLogin_->setVisible(userId_ < 0);
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
    homeCardLayout->addWidget(homeAccent, 0, Qt::AlignHCenter);
    homeCardLayout->addSpacing(5);
    homeCardLayout->addWidget(homeEmblem);
    homeCardLayout->addWidget(homeTitle);
    homeCardLayout->addWidget(homeSubtitle);
    homeCardLayout->addSpacing(7);
    homeCardLayout->addWidget(homeUserLabel_);
    homeCardLayout->addSpacing(9);
    homeCardLayout->addWidget(homeStart_);
    homeCardLayout->addWidget(homeLogin_);
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
        homeUserLabel_->setText(userId_ < 0
            ? QStringLiteral("Вы вошли как  ·  Гость")
            : QStringLiteral("Вы вошли как  ·  %1").arg(username_));
        if (homeLogin_) homeLogin_->setVisible(userId_ < 0);
        navigateToPage(homePage_);

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
        navigateToPage(gamePage_);
        if (gameId_ < 0 || gameResult_ != QStringLiteral("in_progress")) startNewGame();
        else refresh();
    });
    connect(homeLogin_, &QPushButton::clicked, this, [this] {
        authPage_->openLoginPage();
        navigateToPage(authPage_);
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

    connect(boardView_, &BoardWidget::animationFinished, this, [this] {
        if (aiThinking_ || gameOver()) return;
        if (sideToMove_ == humanSide_) refresh();
        else beginComputerTurn();
    });
}

MainWindow::~MainWindow() {
    if (pageTransition_) pageTransition_->stop();
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

void MainWindow::navigateToPage(QWidget* page) {
    if (!page || !pages_) return;
    if (pageTransition_) {
        pageTransition_->stop();
        if (transitionTarget_ && transitionTarget_->graphicsEffect() == transitionEffect_)
            transitionTarget_->setGraphicsEffect(nullptr);
        pageTransition_->deleteLater();
        pageTransition_ = nullptr;
        transitionTarget_ = nullptr;
        transitionEffect_ = nullptr;
    }
    if (pages_->currentWidget() == page) return;

    if (!animationsEnabled_) {
        pages_->setCurrentWidget(page);
        return;
    }

    auto* effect = new QGraphicsOpacityEffect(page);
    effect->setOpacity(0.0);
    page->setGraphicsEffect(effect);
    pages_->setCurrentWidget(page);
    auto* animation = new QPropertyAnimation(effect, "opacity", this);
    animation->setDuration(155);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    pageTransition_ = animation;
    transitionTarget_ = page;
    transitionEffect_ = effect;
    connect(animation, &QPropertyAnimation::finished, this, [this, animation, page, effect] {
        if (page && page->graphicsEffect() == effect) page->setGraphicsEffect(nullptr);
        if (pageTransition_ == animation) {
            pageTransition_ = nullptr;
            transitionTarget_ = nullptr;
            transitionEffect_ = nullptr;
        }
        animation->deleteLater();
    });
    animation->start();
}

void MainWindow::toggleTheme() {
    applyTheme(!darkTheme_);
}

void MainWindow::toggleGameDrawer(bool open) {
    if (!gameDrawerScroll_ || !drawerAnimation_) return;
    if (open == drawerOpen_) return;
    drawerOpen_ = open;
    // The drawer has hidden scrollbars; always reveal its primary action when reopened.
    if (drawerOpen_ && gameDrawerScroll_->verticalScrollBar())
        gameDrawerScroll_->verticalScrollBar()->setValue(0);
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
    if (homePage_) homePage_->setProperty("darkTheme", darkTheme_);
    QSettings().setValue(QStringLiteral("appearance/darkTheme"), darkTheme_);
    boardView_->setDarkTheme(darkTheme_);
    themeButton_->setText(darkTheme_ ? QStringLiteral("Светлая тема") : QStringLiteral("Тёмная тема"));
    if (homePage_) {
        for (auto* button : homePage_->findChildren<QPushButton*>())
            if (button->text().contains(QStringLiteral("тема")))
                button->setText(darkTheme_ ? QStringLiteral("Светлая тема") : QStringLiteral("Тёмная тема"));
    }
    soundButton_->setText(soundsEnabled_ ? QStringLiteral("Звук включён") : QStringLiteral("Звук выключен"));

    if (darkTheme_) {
        setStyleSheet(R"(
            QMainWindow, QWidget#appBackground { background: #141a16; color: #f2eadf; }
            QWidget#homeBackground { background: transparent; }
            QFrame#homeCard { background: rgba(25,34,29,238); border: 1px solid rgba(213,167,119,115); border-radius: 28px; }
            QFrame#homeAccent { background: #d5a777; border: 0; border-radius: 1px; }
            QLabel#brand { color: #d1a77d; font-size: 10px; font-weight: 700; letter-spacing: 2px; }
            QLabel#homeEmblem { color: #d5a777; font-size: 68px; }
            QLabel#homeTitle { color: #fff4e7; font-size: 38px; font-weight: 750; }
            QLabel#homeSubtitle { color: #c4b6a5; font-size: 14px; }
            QLabel#homeFooter { color: #a99a87; font-size: 10px; font-weight: 600; letter-spacing: 2px; }
            QFrame#sidePanel, QFrame#statCard { background: #202a24; border: 1px solid #35443a; border-radius: 20px; }
            QFrame#statHero { background: #202a24; border: 1px solid #3b493f; border-radius: 22px; }
            QLabel#title { color: #f4eadc; font-size: 26px; font-weight: 700; }
            QLabel#muted { color: #d0d9d1; font-size: 12px; }
            QLabel#userLabel { color: #f4eadc; background: #303c33; border-radius: 10px; padding: 8px 11px; font-size: 12px; }
            QLabel#fieldLabel, QLabel#sectionTitle { color: #c4a27e; font-size: 10px; font-weight: 700; letter-spacing: 1.4px; }
            QLabel#status { background: #2b3830; color: #f3e8d8; border: 1px solid #45564a; border-radius: 12px; padding: 13px; font-size: 14px; font-weight: 600; }
            QLabel#stats { color: #c1cdbf; background: #2b3830; border-radius: 10px; padding: 10px; font-size: 11px; }
            QFrame#separator { color: #3a473e; max-height: 1px; }
            QComboBox, QListWidget, QTextEdit { background: #18211c; color: #f1e8dc; border: 1px solid #3b4a40; border-radius: 10px; padding: 9px 11px; min-height: 22px; selection-background-color: #72543b; }
            QComboBox:hover, QListWidget:hover { border-color: #c49a70; }
            QComboBox::drop-down { border: 0; width: 25px; }
            QComboBox QAbstractItemView { background: #202a24; color: #f1e8dc; selection-background-color: #4b5b4f; border: 1px solid #4a594d; }
            QListWidget::item { padding: 7px 5px; border-bottom: 1px solid #303d34; }
            QListWidget::item:selected { background: #35463a; border-radius: 6px; }
            QPushButton { border: 0; border-radius: 11px; padding: 11px 14px; font-weight: 650; min-height: 20px; }
            QPushButton#primary { background: #c18d5d; color: #1d211d; }
            QPushButton#primary:hover { background: #d4a373; }
            QPushButton#primary:pressed { background: #a8754b; }
            QPushButton#secondary { background: #303d34; color: #f0e7da; border: 1px solid #46564a; }
            QPushButton#secondary:hover { background: #3a4b40; border-color: #bd956d; }
            QPushButton#textButton { background: transparent; color: #c3cdbf; }
            QPushButton#textButton:hover { background: #303d34; color: #fff4e7; }
            QCheckBox { spacing: 10px; color: #e8e0d5; }
            QSlider::groove:horizontal { height: 5px; background: #46564a; border-radius: 2px; }
            QSlider::handle:horizontal { background: #c18d5d; width: 15px; margin: -5px 0; border-radius: 7px; }
            QStatusBar { background: #101612; color: #a5b1a7; font-size: 11px; }
            QToolButton#drawerToggle { background: #27342c; color: #f3e5d2; border: 1px solid #596c5d; border-radius: 14px; font-size: 22px; }
            QToolButton#drawerToggle:hover { background: #394a3d; }
            QToolButton#drawerToggle:pressed { background: #1d2821; }
            QLabel#statLabel { color: #aebcaf; font-size: 10px; font-weight: 700; letter-spacing: 1px; }
            QLabel#statValue { color: #f4eadc; font-size: 25px; font-weight: 750; }
            QLabel#statHint { color: #c1ccc2; font-size: 11px; }
            QLabel#statHeroTitle { color: #f5eadb; font-size: 18px; font-weight: 700; }
            QLabel#statHeroText { color: #d0d9d1; font-size: 12px; }
            QToolTip { background: #26342b; color: #f3eadf; border: 1px solid #627365; padding: 6px 9px; border-radius: 7px; }
        )");
    } else {
        setStyleSheet(R"(
            QMainWindow, QWidget#appBackground { background: #f3eee6; color: #352c24; }
            QWidget#homeBackground { background: transparent; }
            QFrame#homeCard { background: rgba(255,251,245,242); border: 1px solid rgba(143,104,69,90); border-radius: 28px; }
            QFrame#homeAccent { background: #9b704e; border: 0; border-radius: 1px; }
            QLabel#brand { color: #8f6748; font-size: 10px; font-weight: 700; letter-spacing: 2px; }
            QLabel#homeEmblem { color: #946b49; font-size: 68px; }
            QLabel#homeTitle { color: #35271d; font-size: 38px; font-weight: 750; }
            QLabel#homeSubtitle { color: #806b59; font-size: 14px; }
            QLabel#homeFooter { color: #927b65; font-size: 10px; font-weight: 600; letter-spacing: 2px; }
            QFrame#sidePanel, QFrame#statCard { background: #fffaf3; border: 1px solid #e7ddcf; border-radius: 20px; }
            QFrame#statHero { background: #fffaf3; border: 1px solid #e7ddcf; border-radius: 22px; }
            QLabel#title { color: #352c24; font-size: 26px; font-weight: 700; }
            QLabel#muted { color: #62594f; font-size: 12px; }
            QLabel#userLabel { color: #594633; background: #f0e5d7; border-radius: 10px; padding: 8px 11px; font-size: 12px; }
            QLabel#fieldLabel, QLabel#sectionTitle { color: #8b6b4d; font-size: 10px; font-weight: 700; letter-spacing: 1.4px; }
            QLabel#status { background: #f0f3ed; color: #4d5e50; border: 1px solid #dfe6dc; border-radius: 12px; padding: 13px; font-size: 14px; font-weight: 600; }
            QLabel#stats { color: #617365; background: #edf1eb; border-radius: 10px; padding: 10px; font-size: 11px; }
            QFrame#separator { color: #e9e0d4; max-height: 1px; }
            QComboBox, QListWidget, QTextEdit { background: #fffdf9; color: #3c3026; border: 1px solid #e4d9ca; border-radius: 10px; padding: 9px 11px; min-height: 22px; selection-background-color: #e9d8c2; }
            QComboBox:hover, QListWidget:hover { border-color: #b58b66; }
            QComboBox::drop-down { border: 0; width: 25px; }
            QComboBox QAbstractItemView { background: #fffaf3; color: #3c3026; selection-background-color: #eee0ce; border: 1px solid #e4d9ca; }
            QListWidget::item { padding: 7px 5px; border-bottom: 1px solid #eee4d8; }
            QListWidget::item:selected { background: #f0e3d2; border-radius: 6px; }
            QPushButton { border: 0; border-radius: 11px; padding: 11px 14px; font-weight: 650; min-height: 20px; }
            QPushButton#primary { background: #876044; color: #fffaf3; }
            QPushButton#primary:hover { background: #9a7353; }
            QPushButton#primary:pressed { background: #704d36; }
            QPushButton#secondary { background: #f1e7da; color: #594633; border: 1px solid #e2d1bd; }
            QPushButton#secondary:hover { background: #ead9c4; border-color: #c7a27e; }
            QPushButton#textButton { background: transparent; color: #777c70; }
            QPushButton#textButton:hover { background: #f0e5d7; color: #594633; }
            QCheckBox { spacing: 10px; color: #594633; }
            QSlider::groove:horizontal { height: 5px; background: #e2d1bd; border-radius: 2px; }
            QSlider::handle:horizontal { background: #876044; width: 15px; margin: -5px 0; border-radius: 7px; }
            QStatusBar { background: #e9e5dd; color: #777c70; font-size: 11px; }
            QToolButton#drawerToggle { background: #fffaf3; color: #594633; border: 1px solid #dfcbb4; border-radius: 14px; font-size: 22px; }
            QToolButton#drawerToggle:hover { background: #f0e5d7; }
            QToolButton#drawerToggle:pressed { background: #e5d6c5; }
            QLabel#statLabel { color: #8a7966; font-size: 10px; font-weight: 700; letter-spacing: 1px; }
            QLabel#statValue { color: #352c24; font-size: 25px; font-weight: 750; }
            QLabel#statHint { color: #655d53; font-size: 11px; }
            QLabel#statHeroTitle { color: #352c24; font-size: 18px; font-weight: 700; }
            QLabel#statHeroText { color: #62594f; font-size: 12px; }
            QToolTip { background: #fffaf3; color: #3c3026; border: 1px solid #e1d5c5; padding: 6px 9px; border-radius: 7px; }
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
        animationSetting_ = new QCheckBox(QStringLiteral("Плавные анимации интерфейса и ходов"), card);
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
            navigateToPage(pageBeforeSettings_ ? pageBeforeSettings_ : homePage_);
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
            navigateToPage(pageBeforeSettings_ ? pageBeforeSettings_ : homePage_);
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
    navigateToPage(settingsPage_);

}
void MainWindow::showHome() {
    if (aiThinking_) return;
    if (homeStart_) homeStart_->setText(!historyMoves_.empty() && gameResult_ == QStringLiteral("in_progress")
        ? QStringLiteral("Продолжить игру") : QStringLiteral("Начать игру"));
    navigateToPage(homePage_);

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
        // A prompt can replace another prompt while its fade is still running.
        // Detach the animation before scheduling that page for deletion.
        if (pageTransition_ && transitionTarget_ == messagePage_) {
            pageTransition_->stop();
            if (messagePage_->graphicsEffect() == transitionEffect_)
                messagePage_->setGraphicsEffect(nullptr);
            pageTransition_->deleteLater();
            pageTransition_ = nullptr;
            transitionTarget_ = nullptr;
            transitionEffect_ = nullptr;
        }
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
    navigateToPage(messagePage_);

    auto finish = [this, callback](bool accepted) {
        QWidget* returnPage = messageReturnPage_ ? messageReturnPage_ : homePage_;
        QWidget* page = messagePage_;
        messagePage_ = nullptr;
        messageReturnPage_ = nullptr;
        if (page) {
            navigateToPage(returnPage);
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

    UserStatistics data;
    QString error;
    if (!database_.getUserStatistics(userId_, &data, &error)) {
        showInlineMessage(QStringLiteral("Не удалось загрузить статистику"), error);
        return;
    }

    auto* page = new QWidget(pages_);
    page->setObjectName("appBackground");
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(34, 28, 34, 28);

    auto* card = new QFrame(page);
    card->setObjectName("sidePanel");
    card->setMaximumWidth(980);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(34, 30, 34, 30);
    layout->setSpacing(18);

    auto* eyebrow = new QLabel(QStringLiteral("RUSSIAN CHECKERS  /  ВАШ ПРОГРЕСС"), card);
    eyebrow->setObjectName("brand");
    auto* heading = new QLabel(QStringLiteral("Статистика игрока"), card);
    heading->setObjectName("title");
    auto* identity = new QLabel(QStringLiteral("%1  ·  статистика по последним сохранённым партиям (до 50)").arg(username_), card);
    identity->setObjectName("muted");
    layout->addWidget(eyebrow);
    layout->addWidget(heading);
    layout->addWidget(identity);

    const QString cardStyle = darkTheme_
        ? QStringLiteral("QFrame { background:#202a2b; border:1px solid #394746; border-radius:16px; }")
        : QStringLiteral("QFrame { background:#f7eee2; border:1px solid #e2d1bb; border-radius:16px; }");
    const QString valueColor = darkTheme_ ? QStringLiteral("#f0dfca") : QStringLiteral("#493323");
    const QString labelColor = darkTheme_ ? QStringLiteral("#d0d9d1") : QStringLiteral("#65513f");
    auto metric = [&](const QString& caption, const QString& value, const QString& detail = QString()) {
        auto* tile = new QFrame(card);
        tile->setStyleSheet(cardStyle);
        auto* tileLayout = new QVBoxLayout(tile);
        tileLayout->setContentsMargins(17, 14, 17, 14);
        tileLayout->setSpacing(5);
        auto* valueLabel = new QLabel(value, tile);
        valueLabel->setStyleSheet(QStringLiteral("color:%1; font-size:25px; font-weight:700; background:transparent; border:0;").arg(valueColor));
        auto* captionLabel = new QLabel(caption.toUpper(), tile);
        captionLabel->setStyleSheet(QStringLiteral("color:%1; font-size:10px; font-weight:700; letter-spacing:1px; background:transparent; border:0;").arg(labelColor));
        tileLayout->addWidget(valueLabel);
        tileLayout->addWidget(captionLabel);
        if (!detail.isEmpty()) {
            auto* detailLabel = new QLabel(detail, tile);
            detailLabel->setWordWrap(true);
            detailLabel->setStyleSheet(QStringLiteral("color:%1; font-size:11px; background:transparent; border:0;").arg(labelColor));
            tileLayout->addWidget(detailLabel);
        }
        return tile;
    };

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);
    const qint64 completed = data.wins + data.losses + data.draws;
    grid->addWidget(metric(QStringLiteral("Процент побед"), QStringLiteral("%1%").arg(data.winRatePercent),
                           QStringLiteral("От завершённых партий")), 0, 0);
    grid->addWidget(metric(QStringLiteral("Партии в истории"), QString::number(data.totalGames),
                           QStringLiteral("%1 завершено · %2 прервано").arg(completed).arg(data.abandoned)), 0, 1);
    grid->addWidget(metric(QStringLiteral("Победы"), QString::number(data.wins),
                           QStringLiteral("%1 поражений · %2 ничьих").arg(data.losses).arg(data.draws)), 0, 2);
    grid->addWidget(metric(QStringLiteral("Время в игре"), formatPlayTime(data.totalPlaySeconds),
                           QStringLiteral("Суммарное игровое время")), 1, 0);
    grid->addWidget(metric(QStringLiteral("Средняя партия"), formatPlayTime(data.averageGameSeconds, true),
                           QStringLiteral("По завершённым партиям")), 1, 1);
    grid->addWidget(metric(QStringLiteral("Сделано ходов"), QString::number(data.totalMoves),
                           QStringLiteral("За всю историю аккаунта")), 1, 2);
    const QString difficultyName = [&data]() {
        switch (data.mostPlayedDifficulty) {
        case 2: return QStringLiteral("Лёгкий");
        case 4: return QStringLiteral("Средний");
        case 6: return QStringLiteral("Сложный");
        case 8: return QStringLiteral("Максимальный");
        default: return QStringLiteral("Пока нет данных");
        }
    }();
    grid->addWidget(metric(QStringLiteral("Любимая сложность"), difficultyName,
                           data.mostPlayedDifficultyGames > 0
                               ? QStringLiteral("Выбиралась %1 раз(а)").arg(data.mostPlayedDifficultyGames)
                               : QStringLiteral("Сыграйте первую партию")), 2, 0, 1, 3);
    layout->addLayout(grid);

    auto* ratePanel = new QFrame(card);
    ratePanel->setObjectName("statHero");
    auto* rateLayout = new QHBoxLayout(ratePanel);
    rateLayout->setContentsMargins(18, 12, 22, 12);
    rateLayout->setSpacing(18);
    auto* ring = new WinRateRing(data.winRatePercent, ratePanel);
    ring->setDarkTheme(darkTheme_);
    rateLayout->addWidget(ring, 0, Qt::AlignVCenter);
    auto* rateText = new QVBoxLayout;
    auto* rateTitle = new QLabel(QStringLiteral("Ваш процент побед"), ratePanel);
    rateTitle->setObjectName("title");
    auto* rateDescription = new QLabel(
        completed > 0
            ? QStringLiteral("%1 побед из %2 завершённых партий").arg(data.wins).arg(completed)
            : QStringLiteral("Завершите первую партию, чтобы увидеть свой результат."),
        ratePanel);
    rateDescription->setObjectName("muted");
    rateDescription->setWordWrap(true);
    rateText->addWidget(rateTitle);
    rateText->addWidget(rateDescription);
    rateText->addStretch(1);
    rateLayout->addLayout(rateText, 1);
    layout->addWidget(ratePanel);

    // Recent games are intentionally limited to the latest 50 records.
    auto* historyHeading = new QLabel(QStringLiteral("Избранное и последние партии"), card);
    historyHeading->setObjectName("sectionTitle");
    auto* historyHint = new QLabel(QStringLiteral("В списке последние 50 партий и все избранные. Избранные игры сохраняются, даже когда уходят из обычной истории."), card);
    historyHint->setObjectName("muted");
    historyHint->setWordWrap(true);
    auto* historyLayout = new QHBoxLayout;
    historyLayout->setSpacing(12);
    auto* recentList = new QListWidget(card);
    recentList->setObjectName("recentGamesList");
    recentList->setMinimumWidth(260);
    recentList->setMinimumHeight(190);
    recentList->setMaximumHeight(230);
    auto* moveDetails = new QTextEdit(card);
    moveDetails->setObjectName("recentGameDetails");
    moveDetails->setReadOnly(true);
    moveDetails->setPlaceholderText(QStringLiteral("Выберите партию слева — здесь появятся её ходы."));
    moveDetails->setMinimumHeight(190);
    moveDetails->setMaximumHeight(230);
    historyLayout->addWidget(recentList, 1);
    historyLayout->addWidget(moveDetails, 1);
    auto* replayButton = new QPushButton(QStringLiteral("Открыть партию на доске"), card);
    replayButton->setObjectName("primary");
    replayButton->setMinimumHeight(44);
    replayButton->setEnabled(false);
    auto* favoriteButton = new QPushButton(QStringLiteral("☆  В избранное"), card);
    favoriteButton->setObjectName("secondary");
    favoriteButton->setMinimumHeight(44);
    favoriteButton->setEnabled(false);
    auto* historyActions = new QHBoxLayout;
    historyActions->setSpacing(10);
    historyActions->addWidget(favoriteButton);
    historyActions->addWidget(replayButton, 1);

    QList<RecentGameRecord> recentGames;
    QString historyError;
    if (database_.getRecentGames(userId_, &recentGames, &historyError)) {
        for (const RecentGameRecord& game : recentGames) {
            const QString resultLabel = game.result == QStringLiteral("win") ? QStringLiteral("Победа")
                : game.result == QStringLiteral("loss") ? QStringLiteral("Поражение")
                : game.result == QStringLiteral("draw") ? QStringLiteral("Ничья")
                : game.result == QStringLiteral("abandoned") ? QStringLiteral("Прервана")
                : QStringLiteral("В процессе");
            const QString difficultyLabel = game.difficulty == 2 ? QStringLiteral("Лёгкий")
                : game.difficulty == 4 ? QStringLiteral("Средний")
                : game.difficulty == 6 ? QStringLiteral("Сложный")
                : game.difficulty == 8 ? QStringLiteral("Максимальный")
                : QStringLiteral("—");
            const QString dateLabel = QDateTime::fromString(game.startedAt, QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                .toString(QStringLiteral("dd.MM.yyyy  HH:mm"));
            const QString itemLabel = QStringLiteral("%1   ·   %2\n%3  ·  %4 ходов")
                .arg(dateLabel.isEmpty() ? game.startedAt : dateLabel)
                .arg(resultLabel).arg(difficultyLabel).arg(game.moveCount);
            auto* item = new QListWidgetItem(
                (game.isFavorite ? QStringLiteral("★  ") : QString()) + itemLabel, recentList);
            item->setData(Qt::UserRole, game.id);
            item->setData(Qt::UserRole + 1, game.isFavorite);
            item->setData(Qt::UserRole + 2, itemLabel);
            item->setToolTip(game.isFavorite
                ? QStringLiteral("Избранная партия №%1 — не удаляется при очистке истории").arg(game.id)
                : QStringLiteral("Партия №%1").arg(game.id));
        }
        if (recentList->count() == 0)
            recentList->addItem(QStringLiteral("Сохранённых партий пока нет"));
    } else {
        recentList->addItem(QStringLiteral("Не удалось загрузить историю"));
        recentList->setToolTip(historyError);
    }
    connect(recentList, &QListWidget::currentItemChanged, this,
        [this, moveDetails, recentList, replayButton, favoriteButton](QListWidgetItem* current, QListWidgetItem*) {
            if (!current || !current->data(Qt::UserRole).isValid()) {
                moveDetails->clear();
                replayButton->setEnabled(false);
                favoriteButton->setEnabled(false);
                favoriteButton->setText(QStringLiteral("☆  В избранное"));
                return;
            }
            replayButton->setEnabled(true);
            favoriteButton->setEnabled(true);
            favoriteButton->setText(current->data(Qt::UserRole + 1).toBool()
                ? QStringLiteral("★  Убрать из избранного")
                : QStringLiteral("☆  В избранное"));
            QStringList moves;
            QString error;
            const qint64 selectedGameId = current->data(Qt::UserRole).toLongLong();
            if (!database_.getGameMoves(userId_, selectedGameId, &moves, &error)) {
                moveDetails->setPlainText(QStringLiteral("Не удалось загрузить ходы.\n%1").arg(error));
                return;
            }
            moveDetails->setPlainText(moves.isEmpty()
                ? QStringLiteral("В этой партии ещё нет сохранённых ходов.")
                : moves.join(QLatin1Char('\n')));
        });
    if (recentList->count() > 0 && recentList->item(0)->data(Qt::UserRole).isValid())
        recentList->setCurrentRow(0);
    layout->addWidget(historyHeading);
    layout->addWidget(historyHint);
    layout->addLayout(historyLayout);
    layout->addLayout(historyActions);
    connect(favoriteButton, &QPushButton::clicked, this, [this, recentList, favoriteButton] {
        auto* current = recentList->currentItem();
        if (!current || !current->data(Qt::UserRole).isValid()) return;
        const qint64 selectedGameId = current->data(Qt::UserRole).toLongLong();
        const bool makeFavorite = !current->data(Qt::UserRole + 1).toBool();
        QString error;
        if (!database_.setGameFavorite(userId_, selectedGameId, makeFavorite, &error)) {
            showInlineMessage(QStringLiteral("Не удалось изменить избранное"), error);
            return;
        }
        current->setData(Qt::UserRole + 1, makeFavorite);
        const QString itemLabel = current->data(Qt::UserRole + 2).toString();
        current->setText((makeFavorite ? QStringLiteral("★  ") : QString()) + itemLabel);
        current->setToolTip(makeFavorite
            ? QStringLiteral("Избранная партия №%1 — не удаляется при очистке истории").arg(selectedGameId)
            : QStringLiteral("Партия №%1").arg(selectedGameId));
        favoriteButton->setText(makeFavorite
            ? QStringLiteral("★  Убрать из избранного")
            : QStringLiteral("☆  В избранное"));
    });
    connect(replayButton, &QPushButton::clicked, this, [this, recentList, page] {
        auto* current = recentList->currentItem();
        if (!current || !current->data(Qt::UserRole).isValid()) return;
        showReplay(current->data(Qt::UserRole).toLongLong(), current->text(), page);
    });

    auto* actions = new QHBoxLayout;
    actions->addStretch(1);
    auto* back = new QPushButton(QStringLiteral("Вернуться в меню"), card);
    back->setObjectName("primary");
    back->setMinimumHeight(44);
    actions->addWidget(back);
    layout->addLayout(actions);
    auto* historyScroll = new QScrollArea(page);
    historyScroll->setWidgetResizable(true);
    historyScroll->setFrameShape(QFrame::NoFrame);
    historyScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    historyScroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: 0; }"));
    historyScroll->setWidget(card);
    historyScroll->setMaximumWidth(1020);
    outer->addWidget(historyScroll, 1);

    pages_->addWidget(page);
    navigateToPage(page);

    connect(back, &QPushButton::clicked, this, [this, page] {
        navigateToPage(homePage_);
        pages_->removeWidget(page);
        page->deleteLater();
    });
    addButtonFeedback(back, buttonSound_);
}

void MainWindow::showReplay(qint64 replayGameId, const QString& gameTitle, QWidget* returnPage) {
    QStringList notations;
    QString error;
    if (!database_.getGameMoves(userId_, replayGameId, &notations, &error)) {
        showInlineMessage(QStringLiteral("Не удалось открыть партию"), error);
        return;
    }

    // Rebuild every position from the saved notation using the actual rules
    // engine. This validates the record and keeps replay independent of live play.
    std::vector<Board> positions;
    std::vector<Side> sides;
    std::vector<Move> moves;
    positions.reserve(static_cast<std::size_t>(notations.size()) + 1);
    sides.reserve(static_cast<std::size_t>(notations.size()) + 1);
    moves.reserve(static_cast<std::size_t>(notations.size()));

    Board replayPosition;
    Side replaySide = Side::White;
    positions.push_back(replayPosition);
    sides.push_back(replaySide);
    const QRegularExpression coordinatePattern(QStringLiteral("([a-h][1-8])"));

    for (int ply = 0; ply < notations.size(); ++ply) {
        const QString notation = notations.at(ply);
        QRegularExpressionMatchIterator matches = coordinatePattern.globalMatch(notation);
        std::vector<Square> path;
        while (matches.hasNext()) {
            const QString coordinate = matches.next().captured(1);
            const int column = coordinate.at(0).toLatin1() - 'a';
            const int row = 8 - coordinate.at(1).digitValue();
            const Square square = Board::toSquare(row, column);
            if (square < 0) {
                showInlineMessage(QStringLiteral("Ошибка записи партии"),
                    QStringLiteral("Некорректная координата в ходе %1.").arg(ply + 1));
                return;
            }
            path.push_back(square);
        }
        if (path.size() < 2) {
            showInlineMessage(QStringLiteral("Ошибка записи партии"),
                QStringLiteral("Не удалось разобрать ход %1: %2").arg(ply + 1).arg(notation));
            return;
        }

        const std::vector<Move> legal = replayPosition.generateMoves(replaySide);
        const auto found = std::find_if(legal.begin(), legal.end(), [&path](const Move& candidate) {
            if (candidate.from != path.front() ||
                static_cast<std::size_t>(candidate.length) != path.size() - 1)
                return false;
            for (std::size_t step = 1; step < path.size(); ++step)
                if (candidate.landings[step - 1] != path[step]) return false;
            return true;
        });
        if (found == legal.end()) {
            showInlineMessage(QStringLiteral("Не удалось воспроизвести партию"),
                QStringLiteral("Ход %1 не соответствует правилам или предыдущим ходам:\n%2")
                    .arg(ply + 1).arg(notation));
            return;
        }

        const Move move = *found;
        Undo undo;
        if (!replayPosition.makeMove(move, undo)) {
            showInlineMessage(QStringLiteral("Ошибка воспроизведения"),
                QStringLiteral("Не удалось применить ход %1.").arg(ply + 1));
            return;
        }
        moves.push_back(move);
        replaySide = opposite(replaySide);
        positions.push_back(replayPosition);
        sides.push_back(replaySide);
    }

    auto* page = new QWidget(pages_);
    page->setObjectName("appBackground");
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(14);

    auto* header = new QHBoxLayout;
    auto* back = new QPushButton(QStringLiteral("←  К истории партий"), page);
    back->setObjectName("secondary");
    back->setMinimumHeight(42);
    auto* heading = new QLabel(QStringLiteral("Просмотр партии"), page);
    heading->setObjectName("title");
    header->addWidget(back);
    header->addSpacing(12);
    header->addWidget(heading);
    header->addStretch(1);
    root->addLayout(header);

    auto* content = new QHBoxLayout;
    content->setSpacing(18);
    auto* replayBoard = new BoardWidget(page);
    replayBoard->setObjectName("replayBoard");
    replayBoard->setDarkTheme(darkTheme_);
    replayBoard->setAnimationSettings(animationsEnabled_, animationSpeedPercent_);
    replayBoard->setMinimumSize(420, 420);
    replayBoard->setInteractionEnabled(false);
    content->addWidget(replayBoard, 1);

    auto* panel = new QFrame(page);
    panel->setObjectName("sidePanel");
    panel->setFixedWidth(292);
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(20, 22, 20, 22);
    panelLayout->setSpacing(12);

    auto* gameName = new QLabel(gameTitle, panel);
    gameName->setObjectName("sectionTitle");
    gameName->setWordWrap(true);
    auto* replayStatus = new QLabel(panel);
    replayStatus->setObjectName("muted");
    replayStatus->setWordWrap(true);
    auto* moveHeading = new QLabel(QStringLiteral("Текущий ход"), panel);
    moveHeading->setObjectName("fieldLabel");
    auto* moveDetails = new QLabel(panel);
    moveDetails->setObjectName("status");
    moveDetails->setWordWrap(true);
    moveDetails->setMinimumHeight(70);
    auto* moveSlider = new QSlider(Qt::Horizontal, panel);
    moveSlider->setObjectName("replaySlider");
    moveSlider->setRange(0, static_cast<int>(moves.size()));
    moveSlider->setSingleStep(1);
    moveSlider->setPageStep(5);
    moveSlider->setTracking(true);

    auto* controls = new QGridLayout;
    controls->setSpacing(8);
    auto makeControl = [panel](const QString& caption, const QString& tip) {
        auto* button = new QPushButton(caption, panel);
        button->setObjectName("secondary");
        button->setMinimumHeight(44);
        button->setToolTip(tip);
        return button;
    };
    auto* first = makeControl(QStringLiteral("⏮"), QStringLiteral("В начало"));
    auto* previous = makeControl(QStringLiteral("◀"), QStringLiteral("Предыдущий ход"));
    auto* next = makeControl(QStringLiteral("▶"), QStringLiteral("Следующий ход"));
    auto* last = makeControl(QStringLiteral("⏭"), QStringLiteral("В конец"));
    controls->addWidget(first, 0, 0);
    controls->addWidget(previous, 0, 1);
    controls->addWidget(next, 1, 0);
    controls->addWidget(last, 1, 1);
    auto* playPause = new QPushButton(QStringLiteral("▶  Воспроизвести"), panel);
    playPause->setObjectName("primary");
    playPause->setMinimumHeight(46);
    auto* replayTimer = new QTimer(page);
    replayTimer->setInterval(950);

    panelLayout->addWidget(gameName);
    panelLayout->addWidget(replayStatus);
    panelLayout->addSpacing(8);
    panelLayout->addWidget(moveHeading);
    panelLayout->addWidget(moveDetails);
    panelLayout->addWidget(moveSlider);
    panelLayout->addLayout(controls);
    panelLayout->addWidget(playPause);
    panelLayout->addStretch(1);
    auto* replayHint = new QLabel(
        QStringLiteral("Используйте кнопки или ползунок, чтобы перемещаться по ходам. Текущая партия не изменяется."),
        panel);
    replayHint->setObjectName("muted");
    replayHint->setWordWrap(true);
    panelLayout->addWidget(replayHint);
    content->addWidget(panel);
    root->addLayout(content, 1);

    auto currentPly = std::make_shared<int>(0);
    auto renderPly = [this, replayBoard, replayStatus, moveDetails, moveSlider, currentPly,
                      positions = std::move(positions), sides = std::move(sides),
                      moves = std::move(moves), notations](int requestedPly, bool animateTransition) {
        const int ply = qBound(0, requestedPly, static_cast<int>(moves.size()));
        const int previousPly = *currentPly;
        const QSignalBlocker blocker(moveSlider);
        moveSlider->setValue(ply);
        if (animateTransition && ply == previousPly + 1) {
            const std::size_t previousIndex = static_cast<std::size_t>(previousPly);
            std::vector<Move> previousLegal = positions[previousIndex].generateMoves(sides[previousIndex]);
            replayBoard->setPosition(positions[previousIndex], sides[previousIndex], previousLegal);
            replayBoard->setLastMove(previousPly > 0
                ? moves[static_cast<std::size_t>(previousPly - 1)] : Move{});
            replayBoard->animateMove(moves[static_cast<std::size_t>(ply - 1)],
                                     positions[static_cast<std::size_t>(ply)]);
            replayBoard->setLastMove(moves[static_cast<std::size_t>(ply - 1)]);
        } else {
            std::vector<Move> legal = positions[static_cast<std::size_t>(ply)].generateMoves(
                sides[static_cast<std::size_t>(ply)]);
            replayBoard->setPosition(positions[static_cast<std::size_t>(ply)],
                                     sides[static_cast<std::size_t>(ply)], legal);
            replayBoard->setLastMove(ply > 0 ? moves[static_cast<std::size_t>(ply - 1)] : Move{});
        }
        *currentPly = ply;
        replayBoard->setInteractionEnabled(false);
        replayStatus->setText(ply == 0
            ? QStringLiteral("Начальная позиция · %1 ходов в записи").arg(moves.size())
            : QStringLiteral("Ход %1 из %2 · %3")
                  .arg(ply).arg(moves.size())
                  .arg(sides[static_cast<std::size_t>(ply)] == Side::White
                       ? QStringLiteral("ход белых") : QStringLiteral("ход чёрных")));
        moveDetails->setText(ply == 0
            ? QStringLiteral("Партия ещё не началась")
            : notations.at(ply - 1));
    };

    connect(moveSlider, &QSlider::valueChanged, this, [renderPly](int ply) {
        renderPly(ply, false);
    });
    connect(first, &QPushButton::clicked, this, [moveSlider] { moveSlider->setValue(0); });
    connect(previous, &QPushButton::clicked, this, [moveSlider] {
        moveSlider->setValue(qMax(moveSlider->minimum(), moveSlider->value() - 1));
    });
    connect(next, &QPushButton::clicked, this, [moveSlider, renderPly] {
        if (moveSlider->value() < moveSlider->maximum())
            renderPly(moveSlider->value() + 1, true);
    });
    connect(last, &QPushButton::clicked, this, [moveSlider] { moveSlider->setValue(moveSlider->maximum()); });
    connect(playPause, &QPushButton::clicked, this, [playPause, replayTimer, moveSlider] {
        if (replayTimer->isActive()) {
            replayTimer->stop();
            playPause->setText(QStringLiteral("▶  Продолжить"));
        } else {
            if (moveSlider->value() >= moveSlider->maximum())
                moveSlider->setValue(0);
            if (moveSlider->maximum() > 0) {
                playPause->setText(QStringLiteral("Ⅱ  Пауза"));
                replayTimer->start();
            }
        }
    });
    connect(replayTimer, &QTimer::timeout, this, [replayTimer, moveSlider, playPause, renderPly] {
        if (moveSlider->value() >= moveSlider->maximum()) {
            replayTimer->stop();
            playPause->setText(QStringLiteral("↻  Повторить"));
            return;
        }
        renderPly(moveSlider->value() + 1, true);
        if (moveSlider->value() >= moveSlider->maximum()) {
            replayTimer->stop();
            playPause->setText(QStringLiteral("↻  Повторить"));
        }
    });
    connect(back, &QPushButton::clicked, this, [this, page, returnPage, replayTimer] {
        replayTimer->stop();
        navigateToPage(returnPage);
        pages_->removeWidget(page);
        page->deleteLater();
    });
    addButtonFeedback(back, buttonSound_);
    addButtonFeedback(first, buttonSound_);
    addButtonFeedback(previous, buttonSound_);
    addButtonFeedback(next, buttonSound_);
    addButtonFeedback(last, buttonSound_);
    addButtonFeedback(playPause, buttonSound_);

    pages_->addWidget(page);
    navigateToPage(page);
    renderPly(0, false);
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
