cmake_minimum_required(VERSION 4.3)

# ---- wxl: базовая библиотека Беседки, соседнее дерево ----
#
# Задаётся до project() по той же причине, что и у Буквицы: wxl собирается с
# `import std;`, а экспериментальный шлюз CMake для него открывается только в
# самом верхнем CMakeLists и только до project(). Скрипт шлюза живёт в wxl и
# знает UUID, привязанный к версии CMake, — включаем его оттуда, чтобы не
# заводить второе место, где этот UUID придётся обновлять.
set(BESEDKA_WXL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../wxl" CACHE PATH
    "Корень дерева wxl (даёт wxl::core, wxl::highlight и wxl::ui.full)")

if(NOT EXISTS "${BESEDKA_WXL_ROOT}/cmake/wxl_import_std_gate.cmake")
    message(FATAL_ERROR
        "Не найден wxl: ожидался в ${BESEDKA_WXL_ROOT}. "
        "Задайте -DBESEDKA_WXL_ROOT=<путь к дереву wxl>.")
endif()

include("${BESEDKA_WXL_ROOT}/cmake/wxl_import_std_gate.cmake")

project(Besedka
    VERSION 0.1.0
    DESCRIPTION "Besedka: RSDN forum client"
    LANGUAGES CXX)

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
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_MODULE_STD 1)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
if(MSVC)
    add_compile_options(/permissive- /utf-8 /Zc:throwingNew /Zc:__cplusplus)
endif()

# Тесты — дело проекта: enable_testing() стоит выше подключения wxl, как она
# того требует, а googletest (find_package(GTest)) отвечает её сценарий,
# `cmake/wxl_gtest.cmake`: он собирает его из исходников на закреплённой метке,
# тем же способом, что для собственных тестов wxl. Ставить пакет не нужно, а
# машина и CI получают один и тот же gtest.
option(BESEDKA_BUILD_TESTS "Собирать тесты" ON)
if(BESEDKA_BUILD_TESTS)
    enable_testing()
    include("${BESEDKA_WXL_ROOT}/cmake/wxl_gtest.cmake")
    find_package(GTest CONFIG REQUIRED)
endif()

# ---- Подключение wxl ----
#
# Корень дерева целиком, как велит её раздел «Подключение wxl» (docs/building.md):
# модули по отдельности не подключаются. Беседка получает бэкенд WinUI вместе с
# генератором, которым собираются обёртки (WXL_UI, WXL_TOOLS), и не получает ни
# примеров, ни песочницы, ни сайта, ни тестов wxl. Из-за генератора первая
# настройка восстанавливает пакеты NuGet и строит проекцию cppwinrt (MSBuild
# окружения разработчика, компонент C++ WinUI), поэтому ходит в сеть.
#
# Цели, которые линкует Беседка: wxl::core — ядро одним модулем `wxl.core` со
# всеми областями (fmt, async, xml, json, html), wxl::highlight, wxl::ui.full —
# окно и контролы, Microsoft::CppWinRT — проекция для сети.
add_subdirectory("${BESEDKA_WXL_ROOT}" wxl)

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
