# Выпуск версии FM RX

## Как выпустить
1. Поднять версию в `CMakeLists.txt` (`project(FmRx VERSION X.Y.Z ...)`) и обновить «Что нового» в
   `packaging/RELEASE_NOTES.md` (`@VERSION@` подставляется сам; там же ссылки «Скачать» и шаги установки).
2. Если менялся интерфейс или функции - обновить инструкцию:
   - снимки: собрать `-DFMRX_BUILD_CHECK=ON`, запустить `FmRxCheck <station.wav> <out>`, скопировать
     `manual_*.png` в `docs/manual/img/*.jpg`;
   - PDF: `python3 docs/manual/make_manual.py --version X.Y.Z` (нужны reportlab, Pillow, шрифты DejaVu).
3. Закоммитить и сделать push в `main`.
4. GitHub Actions соберёт установщики, проверит их (pluginval, тестовая установка и удаление) и создаст или
   обновит **черновик** релиза `vX.Y.Z` с файлами:
   `FM-RX-X.Y.Z-Windows.zip` и `FM-RX-X.Y.Z-macOS.zip` (установщик + обе PDF-инструкции + INSTALL.txt),
   а также отдельно `FM-RX-X.Y.Z-Windows-Setup.exe`, `FM-RX-X.Y.Z-macOS.pkg` и PDF.
   Ссылки «Скачать» в описании релиза начинают работать после публикации.
   Черновик видят только участники репозитория: скачайте файлы и загрузите их в магазин. Пока черновик не
   опубликован, каждый push в `main` обновляет его файлы. Когда опубликуете (Releases → Edit → Publish release;
   при этом создаётся тег `vX.Y.Z`), следующие push его не трогают - для новой версии поднимите номер.

## Что делают установщики
| | Windows (.exe, Inno Setup) | macOS (.pkg) |
|---|---|---|
| Плагин | `C:\Program Files\Common Files\VST3\FM RX.vst3` | `/Library/Audio/Plug-Ins/VST3/FM RX.vst3` |
| Инструкции | `C:\Program Files\NUWMUN\FM RX\` + ярлык в «Пуск» | `/Library/Application Support/NUWMUN/FM RX/` |
| Языки | русский, английский | русский, английский |
| Удаление | «Приложения» → FM RX | удалить две папки выше |

Плагин ищет PDF в этих папках (кнопка «Полная инструкция» в окне помощи).
Windows-сборка со статической средой MSVC - Visual C++ Redistributable не нужен.
macOS-сборка универсальная (arm64 + x86_64), минимум macOS 10.13.

## Подпись (рекомендуется перед продажей)
Без подписи установщики работают, но:
- **Windows** показывает SmartScreen «Неизвестный издатель» → «Подробнее» → «Выполнить в любом случае».
- **macOS** не даёт открыть пакет двойным кликом («неустановленный разработчик») → правый клик → «Открыть».

Чтобы этого не было, добавьте секреты в репозиторий (Settings → Secrets and variables → Actions):

| Секрет | Что это |
|---|---|
| `WIN_CERT_PFX_BASE64`, `WIN_CERT_PASSWORD` | сертификат подписи кода Windows (.pfx в base64) и пароль |
| `MAC_CERTS_P12_BASE64`, `MAC_CERTS_PASSWORD` | экспорт из «Связки ключей» сертификатов *Developer ID Application* и *Developer ID Installer* в один .p12 (base64) и пароль |
| `MAC_APP_SIGN_ID` | например `Developer ID Application: Ivan Ivanov (ABCDE12345)` |
| `MAC_INSTALLER_SIGN_ID` | например `Developer ID Installer: Ivan Ivanov (ABCDE12345)` |
| `NOTARY_APPLE_ID`, `NOTARY_PASSWORD`, `NOTARY_TEAM_ID` | Apple ID, пароль приложения (appleid.apple.com) и Team ID - для нотаризации |

Сертификаты macOS выдаёт Apple Developer Program (99 $ в год). Для Windows - любой центр сертификации подписи
кода (OV/EV) или Azure Trusted Signing. Шаги подписи уже есть в `.github/workflows/build.yml` и
`packaging/macos/build_pkg.sh`; они включаются сами, когда секреты заданы.

## Лицензии перед продажей
- **JUCE 8**: бесплатная лицензия Starter - при годовом доходе до 20 000 $; выше - Indie (до 300 000 $) или Pro.
  Заставка JUCE в JUCE 8 не требуется (`JUCE_DISPLAY_SPLASH_SCREEN=0`).
- **VST3**: сборка идёт с VST3 SDK, который поставляется вместе с JUCE. Логотип и слово VST - по правилам Steinberg.
- **radiofm** (MIT, © Henrik Forssell) - упоминание есть в инструкции и `THIRD_PARTY_NOTICES.md`.
