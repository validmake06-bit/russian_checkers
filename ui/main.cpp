#include "AuthDialog.h"
#include "MainWindow.h"
#include "../db/Database.h"

#include <QApplication>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QPropertyAnimation>
#include <QEasingCurve>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("RussianCheckers"));
    QApplication::setOrganizationName(QStringLiteral("RussianCheckers"));

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(dataDir)) {
        QMessageBox::critical(nullptr, QStringLiteral("Ошибка"), QStringLiteral("Не удалось создать папку данных приложения."));
        return 1;
    }

    Database database;
    QString error;
    if (!database.open(dataDir + QStringLiteral("/checkers.sqlite"), &error)) {
        QMessageBox::critical(nullptr, QStringLiteral("Ошибка SQLite"), error);
        return 1;
    }

    AuthDialog auth(database);
    if (auth.exec() != QDialog::Accepted) return 0;

    MainWindow window(database, auth.userId(), auth.username());
    window.setWindowOpacity(0.0);
    window.show();
    auto* intro = new QPropertyAnimation(&window, "windowOpacity", &window);
    intro->setDuration(420);
    intro->setStartValue(0.0);
    intro->setEndValue(1.0);
    intro->setEasingCurve(QEasingCurve::OutCubic);
    intro->start(QAbstractAnimation::DeleteWhenStopped);
    return app.exec();
}
