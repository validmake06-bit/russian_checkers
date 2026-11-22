# Russian Checkers — Stage 6 (audit and hardening)

Настольная игра в русские шашки на **C++17 + Qt 6**. Qt 6 теперь обязательная зависимость: CMake не создаёт «урезанную» сборку без GUI.

## Возможности этого этапа

- Qt 6 Widgets: главное окно, игровая доска, выбор стороны и сложности, история ходов, светлая/тёмная тема.
- Qt Concurrent: поиск хода компьютером в отдельной задаче, чтобы интерфейс не зависал.
- Qt SQL + SQLite: локальная база пользователей, партий и ходов.
- Регистрация и вход. Новые пароли защищаются PBKDF2-HMAC-SHA256 с уникальной 128-битной солью и 210 000 итерациями; старые хеши автоматически обновляются после успешного входа.
- Сохранение ходов по мере игры и запись результата партии; незавершённая партия помечается как прерванная при начале новой.
- Экспорт протокола в TXT.
- Игровое ядро и AI остаются независимыми от Qt.

## Зависимости

- CMake 3.21+
- C++17-компилятор (MSVC 2019/2022, GCC или Clang)
- Qt 6.2+ с модулями **Widgets, Sql, Concurrent, Multimedia**
- Драйвер Qt SQL SQLite (`QSQLITE`).

## Установка Qt 6

### Windows (рекомендуемый вариант)

1. Установить Qt через официальный Qt Online Installer.
2. Выбрать Qt 6.x для используемого комплекта компилятора (например, `MinGW 64-bit` или `MSVC 2022 64-bit`). Важно: компилятор должен совпадать с выбранным Qt Kit.
3. В компонентах отметить `Qt Widgets`, `Qt SQL`, `Qt Multimedia`, а также SQLite driver / `QSQLite` (если он предлагается отдельным пунктом). Установить CMake и Ninja либо использовать CMake из Visual Studio.
4. Добавить каталог `.../Qt/6.x.x/<kit>/bin` в `PATH` либо указать `CMAKE_PREFIX_PATH`.

Пример для PowerShell (пути замените на свои):

```powershell
$env:CMAKE_PREFIX_PATH = 'C:\Qt\6.8.3\mingw_64'
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Для запуска вне Qt Creator рядом с приложением должны быть Qt DLL и плагины платформы/SQL. Для MinGW это можно подготовить утилитой `windeployqt` из соответствующего Qt Kit:

```powershell
windeployqt --release --compiler-runtime build\russian_checkers.exe
```

Проверьте, что `sqldrivers\qsqlite.dll` попал в развёрнутый каталог. Без него приложение не сможет открыть SQLite.

### Ubuntu / Debian

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-multimedia-dev libqt6sql6-sqlite
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

### macOS

Установите Qt 6 через Qt Online Installer или Homebrew, включая Widgets, Sql и Multimedia. Затем задайте путь к Qt:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Запуск

```bash
./build/russian_checkers
```

Windows: `build\\russian_checkers.exe`.

При первом запуске в главном окне откроется страница входа. Можно войти, перейти на отдельную страницу регистрации или продолжить как гость. Имя пользователя — 3–32 символа, пароль — от 8 символов. База `checkers.sqlite` располагается в стандартной пользовательской папке данных приложения Qt.

## Структура

```text
core/   — доска, генератор ходов, AI
ui/     — Qt Widgets, главное окно, встроенные страницы авторизации и игровая доска
 db/    — SQLite: users, games, moves
 tests/ — тесты игрового ядра
```

## Важные замечания

- Ходы и исход партии сохраняются в SQLite; протокол можно экспортировать в TXT. Экспорт выполняется атомарно через временный файл, чтобы не оставить повреждённый протокол при сбое записи.
- Пароли новых аккаунтов защищены PBKDF2-HMAC-SHA256 (210 000 итераций, случайная 128-битная соль). Для уже существующих аккаунтов используется безопасная миграция при успешной авторизации: старый хеш проверяется, затем заменяется новым.
- Добавлено правило троекратного повторения позиции с учётом стороны, которой принадлежит ход.
- `Board::makeMove` предварительно проверяет целостность структуры хода и не изменяет позицию при некорректных данных.
- Поиск ИИ использует лучший ход предыдущей итерации как приоритетный вариант следующего прохода, улучшая эффективность alpha-beta.
- История партий хранится в базе, но отдельный экран просмотра и восстановление незаконченной позиции ещё не реализованы.
- Звуковые эффекты ходов/взятий и фоновая музыка доступны; эффекты и музыка настраиваются независимо.

## UI refresh (Stage 5)

Обновлённый интерфейс Qt Widgets: переработанное окно входа и регистрации, боковая панель, светлая и тёмная темы, деревянная доска, плавное перемещение шашек и затухание снятых шашек. Окно игры появляется с короткой плавной анимацией.

Собери проект тем же Qt 6 MinGW Kit через Qt Creator. Ядро тестируется отдельно; интерфейс необходимо дополнительно проверить на Windows.


## Проверки Stage 6

- Добавлены проверки идентификатора позиции и цикла повторения, а также отказа от некорректного хода без изменения доски.
- Игровое ядро проверено компиляцией GCC и Clang в режиме C++17; обычный набор тестов проходит.
- Дополнительно выполнен прогон с AddressSanitizer и UndefinedBehaviorSanitizer — ошибок памяти и неопределённого поведения в проверенном ядре не обнаружено.
- Qt GUI и SQLite-слой необходимо окончательно собрать и проверить в установленном Qt Creator на Windows: в текущей среде Qt 6 отсутствует, поэтому честно подтвердить сборку интерфейса здесь нельзя.

## Stage 7 — Minimal UI and audio polish

This update keeps the existing Russian draughts rules, AI, SQLite schema and account flow, while refreshing the desktop presentation:

- Light and dark minimalist themes; the selected theme is saved with `QSettings` and restored on the next launch.
- Matching light/dark styling for the sign-in and registration dialog.
- A calmer sage/graphite palette, flatter board squares, softer piece shading and cleaner legal-move highlights.
- Shorter, smoother move and capture animations.
- Built-in WAV effects for a move, capture, victory and defeat. Sound can be switched off, and volume is adjustable in Settings; both preferences are saved.
- Fixed the `checkers::Square` / `checkers::Piece` qualification in `BoardWidget.h` so the GUI header is consistent with the core namespace.

Qt 6 Widgets, Sql, Concurrent and Multimedia are required. Open this folder's `CMakeLists.txt` in Qt Creator, select a Qt 6 MinGW kit, configure and build `russian_checkers`. The core-only tests can also be run with CMake/CTest in an environment where Qt is available.


## Stage 8 — Minimal UI update

This update adds a dedicated start screen and a more complete desktop flow:

- Main menu with Start/Continue, player statistics, preferences and theme switching.
- Returning to the main menu preserves an unfinished game; Continue returns to it.
- Starting another game asks before marking the previous unfinished game as abandoned.
- Statistics are read from SQLite and show total games, wins, losses, draws, abandoned games and win percentage.
- Optional original ambient background loop, disabled by default, with independent volume control.
- Separate controls for sound effects and background music; preferences persist through QSettings.
- Existing board, move history, AI difficulty, protocol export and login/registration are retained.

The application still requires Qt 6.2+ with Widgets, Sql, Concurrent and Multimedia. Open `CMakeLists.txt` in Qt Creator, select the installed Qt 6 MinGW kit, build `russian_checkers`, and run that target (not `checkers_tests`).

The portable core tests can also be built without Qt:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Icore core/Board.cpp core/AI.cpp tests/tests.cpp -o checkers_tests
./checkers_tests
```

### Авторизация (обновление)
- Регистрация открывается на отдельной странице, не смешанной с формой входа.
- Перед созданием аккаунта проверяется совпадение паролей.
- После регистрации приложение сразу проверяет созданную запись тем же механизмом, что и обычный вход, и открывает игру только при успешной проверке.
- Вход допускает разный регистр имени пользователя; для старых записей предусмотрена попытка совместимого чтения прежнего формата хеша с последующим обновлением.
- «Продолжить как гость» запускает игру без создания аккаунта. В гостевом режиме партии не записываются в профиль и статистика недоступна; экспорт текущего протокола в TXT остаётся доступен.


## Stage 9 — Complete in-app UI and animation polish

- Login, registration and guest access are now pages inside the main application window; there is no separate authentication dialog.
- Settings, confirmations, statistics and game notifications are displayed inside the same main window and follow the active light/dark theme.
- Move animations can be disabled or adjusted from Settings. Multi-captures play as separate jumps: the captured checker remains visible until the attacker lands, then fades before the next jump.
- Computer turns wait for the current move animation to finish. Resetting the board cancels any stale animation.
- Theme, sound, music and animation preferences are persisted with QSettings.
- Added SQLite tests for registration, case-insensitive authentication, incorrect passwords, saved game history, statistics and protocol export.
- Added a GitHub Actions workflow that builds the Qt 6 application and runs CTest on Ubuntu.

The Qt GUI and database test suite must still complete a successful CI run and be checked in the Windows Qt Creator / MinGW kit before this build can be considered fully verified. The file export destination picker remains a native operating-system file chooser.
