# 4.5 — тот же минимум, что у wxl: с этого выпуска CMake даёт `import std;`
# каждой цели C++20 и новее, чьи исходники сканируются на импорты, без
# экспериментального ключа.
cmake_minimum_required(VERSION 4.5)

# ---- wxl: базовая библиотека Беседки, пакет WinXL или соседнее дерево ----
#
# Откуда берётся wxl, решает пресет. Пресет x64 берёт пакет NuGet WinXL:
# подключает поставщика cmake/winxl-provider.cmake (копия файла wxl), и
# find_package(wxl) ниже восстанавливает пакет по nuget.config и файлу
# блокировки packages.lock.json в корне. Пресет x64-wxl-tree задаёт
# BESEDKA_WXL_ROOT — корень соседнего дерева wxl, которое подключается
# подкаталогом: так Беседка собирается с ещё не влитой парной веткой wxl.
#
# Путь проверяется до project(), чтобы дерево без wxl останавливалось сразу и
# с понятным сообщением, а не на add_subdirectory ниже.
set(BESEDKA_WXL_ROOT "" CACHE PATH
    "Корень соседнего дерева wxl (пресет x64-wxl-tree); пусто — wxl из пакета WinXL (пресет x64)")

if(BESEDKA_WXL_ROOT AND NOT EXISTS "${BESEDKA_WXL_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "Не найден wxl: ожидался в ${BESEDKA_WXL_ROOT}. "
        "Задайте -DBESEDKA_WXL_ROOT=<путь к дереву wxl>.")
endif()

project(Besedka
    VERSION 0.1.0
    DESCRIPTION "Besedka: RSDN forum client"
    LANGUAGES CXX)

# Поставщик пакета регистрируется на первом project(). Настройка без пресета —
# без поставщика и без соседнего дерева — остановлена здесь: find_package(wxl)
# искал бы wxl по путям машины и мог найти чужую.
if(NOT BESEDKA_WXL_ROOT AND NOT COMMAND winxl_provide_dependency)
    message(FATAL_ERROR
        "wxl не из чего взять: BESEDKA_WXL_ROOT пуст, а поставщик пакета WinXL не подключён. "
        "Настройте пресетом x64 (пакет) или x64-wxl-tree (соседнее дерево).")
endif()

# ---- Язык: до подключения wxl и для всего дерева сразу ----
#
# Тот же набор, что у самой wxl (`cmake/wxl_language.cmake` в её дереве): CMake
# считает BMI годным только там, где язык настроен так же, а цель с другим
# набором получает не общий BMI, а свой — собственную копию модуля `std` и
# интерфейса wxl.core. Подключаемая wxl набор не задаёт, а проверяет и
# останавливает настройку, называя недостающие строки.
#
# /Zc:__cplusplus — Беседки: настоящее значение __cplusplus в её заголовках. Он
# стоит на каталоге рядом с остальными по той же причине: на цели он разделил бы
# отпечаток.
#
# /W4 сюда не входит намеренно: уровень предупреждений на годность BMI не
# влияет, а предупреждения wxl — её дело. Он стоит ниже, вместе с определениями,
# и накрывает только код Беседки.
#
# Модуль std цель получает от сканирования исходников на импорты
# (CMAKE_CXX_SCAN_FOR_MODULES): отдельного ключа для него в CMake 4.5 нет.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
if(MSVC)
    add_compile_options(/permissive- /utf-8 /Zc:throwingNew /Zc:__cplusplus)
endif()

# Тесты — дело проекта: enable_testing() стоит выше подключения wxl, как она
# того требует, а googletest (find_package(GTest)) отвечает её сценарий,
# `cmake/wxl_gtest.cmake`: он собирает его из исходников на закреплённой метке,
# тем же способом, что для собственных тестов wxl. Ставить пакет не нужно, а
# машина и CI получают один и тот же gtest. Пресет x64-wxl-tree включает файл
# соседнего дерева здесь, до add_subdirectory, как велит сам сценарий; пресет
# x64 — сценарий из пакета WinXL, который называет WXL_GTEST_SCRIPT, после
# find_package(wxl) ниже.
option(BESEDKA_BUILD_TESTS "Собирать тесты" ON)
if(BESEDKA_BUILD_TESTS)
    enable_testing()
    if(BESEDKA_WXL_ROOT)
        include("${BESEDKA_WXL_ROOT}/cmake/wxl_gtest.cmake")
    endif()
endif()

# ---- Подключение wxl ----
#
# Пакет: find_package с наименьшей версией WinXL, которую Беседка принимает.
# С 0.1.0.83 в пакете есть wxl.highlight, которую линкует besedka.rsdn, и
# wxl.bb с wxl.rsdn, которые линкуют компоненты профиля forum, а профиль из
# `extends` ищется и среди профилей пакета; с 0.1.0.84 у wxl есть действия,
# которыми CI Беседки собирает пакет (.github/workflows/winxl.yml); с 0.1.0.87
# пакет несёт сценарий googletest wxl_gtest.cmake (WXL_GTEST_SCRIPT), а BMI
# интерфейсов его модулей собираются без отладочной информации и не спорят за
# общий vc140.pdb (C1041 в Debug). Точную версию держит файл блокировки
# packages.lock.json; NuGet берёт наименьшую версию не ниже названной, так что
# переход на новую WinXL поднимает и это число. Пакет несёт готовые
# библиотеки, генератор и профили; интерфейсы модулей, обёртки профиля forum и
# его библиотеку сборка строит сама.
#
# Соседнее дерево: корень целиком, как велит её раздел «Подключение wxl»
# (docs/building.md): модули по отдельности не подключаются. Беседка получает
# бэкенд WinUI вместе с генератором, которым собираются обёртки (WXL_UI,
# WXL_TOOLS), и не получает ни примеров, ни песочницы, ни сайта, ни тестов wxl.
#
# В обоих случаях первая настройка восстанавливает пакеты NuGet и строит
# проекцию cppwinrt (MSBuild окружения разработчика, компонент C++ WinUI),
# поэтому ходит в сеть.
#
# Цели, которые линкует Беседка: wxl::core — ядро одним модулем `wxl.core` со
# всеми областями (fmt, async, xml, json, html), wxl::highlight, wxl::ui.forum —
# окно и контролы профиля Беседки (App/profile), Microsoft::CppWinRT — проекция
# для сети.
if(BESEDKA_WXL_ROOT)
    add_subdirectory("${BESEDKA_WXL_ROOT}" wxl)
else()
    find_package(wxl 0.1.0.87 CONFIG REQUIRED)
    if(BESEDKA_BUILD_TESTS)
        include("${WXL_GTEST_SCRIPT}")
    endif()
endif()
if(BESEDKA_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
endif()

# Библиотека профиля Беседки, wxl::ui.forum: обёртки профиля
# App/profile/forum.winxl-profile, которые пишет генератор wxl, и компоненты,
# которые профиль выбирает. Профиль продолжает rich wxl; rich лежит не рядом
# с ним, и генератор с настройкой находят его среди профилей wxl -- пакета или
# соседнего дерева. Это код wxl, поэтому библиотека объявлена здесь, до своих
# ключей Беседки ниже, как прежде wxl::ui.full в дереве wxl.
wxl_add_ui_library(forum PROFILE "${CMAKE_CURRENT_SOURCE_DIR}/App/profile/forum.winxl-profile")

# Своё Беседки — строго после подключения wxl: у wxl свои NOMINMAX и
# WIN32_LEAN_AND_MEAN в platform.h, и наши дали бы C4005 на каждом её файле.
if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(UNICODE _UNICODE)
endif()

add_subdirectory(Rsdn)
add_subdirectory(Forum)

# Само приложение -- только под Windows: оно и есть WinUI.
if(WIN32)
    add_subdirectory(App)
endif()
