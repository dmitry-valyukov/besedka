# Беседка / Besedka

Клиент форума [RSDN](https://rsdn.org) для Windows 10/11 на `wxl.ui` (WinUI 3
без XAML), C++ / WinRT. Сообщение показывается своим деревом разметки, а не
серверным HTML.

## Состав

| Модуль | Что это |
|---|---|
| **Rsdn/** | Разметка форума RSDN: токенизатор, который превращает текст сообщения в дерево семейства `wxl.html`. |
| **Forum/** | Всё, что касается сервера RSDN: запросы, разбор ответов, модель сообщений. Об интерфейсе не знает. |
| **App/** | Сама Беседка: модуль `besedka.app` — то, что приложение решает без окон (маршруты, история переходов, план страниц, настройки, даты), и исполняемый файл с окном, каркасом и экранами. |

## Сборка

Нужны:

- библиотека [wxl](https://github.com/wxl-project/wxl) одним из двух способов,
  описанных ниже. Пакеты NuGet, проекцию cppwinrt и обёртки wxl готовит сама
  при настройке и сборке, поэтому первая настройка ходит в сеть;
- Visual Studio с тулчейном, умеющим `import std;`, и компонентом C++ WinUI,
  CMake 4.5 или новее, как и самой wxl: с этого выпуска он даёт `import std;`
  без экспериментального ключа, и .NET SDK 10.0.200 или новее;
- для запуска — экспериментальная среда Windows App Runtime: её ставит утилита
  `winxl` — из `bin` пакета `WinXL` (`winxl runtime install --packages
  build\x64\wxl.packages\restored-packages.json`), а с соседним деревом — та, что
  оно собирает с `-DWXL_WINXL=ON`.

**Пакет NuGet `WinXL`** — пресет `x64`, основной путь. Поставщик
`cmake/winxl-provider.cmake` (копия файла wxl) восстанавливает пакет для
`find_package(wxl)`: наименьшую версию называет корневой `CMakeLists.txt`,
точную — файл блокировки `packages.lock.json`, источники — `nuget.config`.
`WinXL` берётся только из локального каталога пакетов `feed/` в корне
дерева. Пакет собирается из дерева wxl на коммите той версии, что записана в
файле блокировки, так, как его собирает `package.yml` wxl (раздел «Пакет
WinXL» её `docs/building.md`), и публикуется в `feed/` её сценарием; затем
`tools\winxl-lock.ps1` записывает в файл блокировки хеш этого пакета —
упаковка не воспроизводится побайтно, а NuGet сверяет хеш при каждом
восстановлении:

```
cmake -DWXL_PUBLISH_PACKAGE=<каталог>\WinXL.<версия>.nupkg -DWXL_PUBLISH_FEED=feed -P <wxl>\tools\package\publish.cmake
tools\winxl-lock.ps1
tools\build.ps1                 # настроить (если нужно) и собрать Debug
tools\build.ps1 -Config Release
ctest --preset x64-debug
run-besedka.cmd                 # собрать Debug и запустить
```

Пакету нужны обе сборки wxl, `x64-dynamic` и `x64-static`, каждая в Debug и
Release: Debug Беседки берёт библиотеки с CRT в DLL, Release — со статической,
а конфигурация пакета требует обе конфигурации в каждом виде CRT.

**Соседнее дерево wxl** — пресет `x64-wxl-tree`: корень дерева
`BESEDKA_WXL_ROOT` (по умолчанию `../wxl`) подключается подкаталогом. Так
Беседка собирается с ещё не влитой правкой wxl:

```
tools\build.ps1 -Preset x64-wxl-tree
tools\build.ps1 -Preset x64-wxl-tree -Configure -Define BESEDKA_WXL_ROOT=M:/wxl   # только перенастроить
ctest --preset x64-wxl-tree-debug
```

`tools\build.ps1` и `run-besedka.cmd` идут через PowerShell 7 (`pwsh`). Скрипт
сам поднимает окружение Visual Studio, так что годится обычная оболочка.
Генератор — Ninja Multi-Config: генераторы Visual Studio не собирают
`import std;`. Debug собирается с CRT в DLL, Release — со статической CRT,
и вид CRT задаёт пресет (`CMAKE_MSVC_RUNTIME_LIBRARY`) для всего дерева
вместе с wxl. googletest для тестов настройка собирает из исходников сама,
тем же способом, что и wxl, — её сценарием `cmake/wxl_gtest.cmake`: из пакета
(`WXL_GTEST_SCRIPT`) или из соседнего дерева.

Пул-реквесты проверяет GitHub Actions (`.github/workflows/ci.yml`): на
Windows-машине дерево собирается в Debug и Release из пакета `WinXL`,
проходит тесты, и клиент форума должен открыть своё окно. Пакет той версии,
что записана в файле блокировки, собирает сценарий
`.github/workflows/winxl.yml` действиями самой wxl из её дерева на коммите
`main` с этой версией; версия, которой ещё нет в `main` wxl, оставляет
проверку красной, пока нужная правка wxl не влита. Порядок внесения правок и правила для коммитов — в [CONTRIBUTING](CONTRIBUTING.md); историю пул-реквеста проверяет `.github/workflows/commits.yml`.
