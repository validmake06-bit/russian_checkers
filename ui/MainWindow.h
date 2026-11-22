#pragma once
#include "BoardWidget.h"
#include "AI.h"
#include <QMainWindow>
#include <QToolButton>
#include <QPropertyAnimation>
class QFrame;
class QScrollArea;
class QGraphicsOpacityEffect;
class QCheckBox;
class QSlider;
#include <QFutureWatcher>
#include <QStringList>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <functional>

class QLabel;
class QListWidget;
class QComboBox;
class QPushButton;
class QSoundEffect;
class QStackedWidget;
class QMediaPlayer;
class QAudioOutput;

class Database;
class AuthDialog;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(Database& database, qint64 userId, QString username, QWidget* parent = nullptr);
    ~MainWindow() override;
private slots:
    void onSquareClicked(int square);
    void startNewGame();
    void onAiFinished();
    void toggleTheme();
    void openSettings();
    void exportProtocol();
    void showHome();
    void showStatistics();
    void showReplay(qint64 gameId, const QString& gameTitle, QWidget* returnPage);
    void toggleGameDrawer();
    void toggleGameDrawer(bool open);
private:
    void refresh();
    void navigateToPage(QWidget* page);
    void applyTheme(bool dark);
    void applySoundSettings(bool enabled, qreal volume);
    void playMoveSound(bool capture);
    void beginComputerTurn();
    void finishMove(const checkers::Move& move, bool animate);
    QString moveText(const checkers::Move& move) const;
    bool gameOver() const;
    bool drawByRepetition() const;
    void showInlineMessage(const QString& title, const QString& message);
    void showInlinePrompt(const QString& title, const QString& message,
                          const QString& acceptText, const QString& cancelText,
                          std::function<void(bool)> callback);

    QStackedWidget* pages_{};
    QWidget* homePage_{};
    AuthDialog* authPage_{};
    QLabel* userLabel_{};
    QLabel* homeUserLabel_{};
    QWidget* gamePage_{};
    QFrame* gameDrawer_{};
    QScrollArea* gameDrawerScroll_{};
    QToolButton* drawerToggle_{};
    QPropertyAnimation* drawerAnimation_{};
    bool drawerOpen_{false};
    QWidget* settingsPage_{};
    QWidget* messagePage_{};
    QWidget* messageReturnPage_{};
    QWidget* pageBeforeSettings_{};
    QWidget* statisticsPage_{};
    QComboBox* themeSetting_{};
    QCheckBox* soundSetting_{};
    QSlider* soundVolumeSetting_{};
    QCheckBox* musicSetting_{};
    QSlider* musicVolumeSetting_{};
    QCheckBox* animationSetting_{};
    QSlider* animationSpeedSetting_{};
    bool animationsEnabled_{true};
    int animationSpeedPercent_{100};
    BoardWidget* boardView_{};
    QLabel* status_{};
    QLabel* stats_{};
    QListWidget* history_{};
    QComboBox* sideChoice_{};
    QComboBox* difficulty_{};
    QPushButton* exportButton_{};
    QPushButton* newGame_{};
    QPushButton* themeButton_{};
    QPushButton* soundButton_{};
    QPushButton* settingsButton_{};
    QPushButton* homeButton_{};
    QPushButton* homeStart_{};
    QPushButton* homeLogin_{};
    QMediaPlayer* musicPlayer_{};
    QAudioOutput* musicOutput_{};
    bool musicEnabled_{false};
    qreal musicVolume_{0.12};
    QSoundEffect* moveSound_{};
    QSoundEffect* captureSound_{};
    QSoundEffect* victorySound_{};
    QSoundEffect* defeatSound_{};
    QSoundEffect* buttonSound_{};
    bool soundsEnabled_{true};
    qreal soundVolume_{0.55};
    checkers::Board board_;
    checkers::Side sideToMove_{checkers::Side::White};
    checkers::Side humanSide_{checkers::Side::White};
    std::vector<checkers::Move> legalMoves_;
    std::vector<checkers::Move> historyMoves_;
    checkers::Square selected_{-1};
    bool darkTheme_{true};
    bool aiThinking_{false};
    bool abandonConfirmed_{false};
    QFutureWatcher<checkers::SearchResult> aiWatcher_;
    Database& database_;
    qint64 userId_{-1};
    qint64 gameId_{-1};
    QString username_;
    QStringList protocol_;
    QString gameResult_{QStringLiteral("in_progress")};
    std::unordered_map<std::uint64_t, int> positionCounts_;
};
