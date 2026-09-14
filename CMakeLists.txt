cmake_minimum_required(VERSION 3.28)

# ---- wxl: базовая библиотека Беседки, соседнее дерево ----
#
# Задаётся до project() по той же причине, что и у Буквицы: wxl собирается с
# `import std;`, а экспериментальный шлюз CMake для него открывается только в
# самом верхнем CMakeLists и только до project(). Скрипт шлюза живёт в wxl и
# знает UUID, привязанный к версии CMake, — включаем его оттуда, чтобы не
# заводить второе место, где этот UUID придётся обновлять.
set(BESEDKA_WXL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../wxl" CACHE PATH
    "Корень дерева wxl (даёт wxl::unicode, wxl::rsdn, wxl::winui)")

if(NOT EXISTS "${BESEDKA_WXL_ROOT}/cmake/wxl_import_std_gate.cmake")
    message(FATAL_ERROR
        "Не найден wxl: ожидался в ${BESEDKA_WXL_ROOT}. "
        "Задайте -DBESEDKA_WXL_ROOT=<путь к дереву wxl>.")
endif()

set(BESEDKA_WXL_CMAKE_DIR "${BESEDKA_WXL_ROOT}/cmake")

# vcpkg — ради cppwinrt: из него и проекция WinUI 3 под wxl.winui, и заголовки
# пространств Windows.*, которыми Беседка ходит в сеть. До project(), потому
# что ставит CMAKE_TOOLCHAIN_FILE, а его после первого project() уже никто не
# читает.
include("${BESEDKA_WXL_CMAKE_DIR}/vcpkg_bootstrap.cmake")
include("${BESEDKA_WXL_CMAKE_DIR}/wxl_import_std_gate.cmake")

project(Besedka
    VERSION 0.1.0
    DESCRIPTION "Besedka: RSDN forum client"
    LANGUAGES CXX)

# C++23 требует wxl от своих потребителей: разойдясь стандартом, они дают
# C5050 «mismatched C++ versions» на каждый импорт и падение на компоновке.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ключи языка — до подключения wxl и для всего дерева сразу. CMake считает BMI
# годным только там, где язык настроен так же: цель с другим набором получает
# свою копию модуля в синтетической цели `<цель>@synth_N`, а две копии одного
# модуля в одной цели роняют сборку. /W4 и определения стоят ниже: они —
# дело Беседки, а не wxl.
if(MSVC)
    add_compile_options(/utf-8 /permissive- /Zc:__cplusplus)
endif()

# ---- Подключение wxl ----
#
# Берутся подкаталоги с исходниками, а не корневые CMakeLists модулей: те
# устроены как самостоятельные проекты и тянут Boost, GTest и генератор
# проекции — всё это нужно самой wxl и не нужно клиенту форума.
include("${BESEDKA_WXL_CMAKE_DIR}/wxl_helpers.cmake")

if(NOT TARGET wxl::core)
    add_subdirectory("${BESEDKA_WXL_ROOT}/wxl.core/src" "${CMAKE_BINARY_DIR}/wxl.core" EXCLUDE_FROM_ALL)
endif()
if(NOT TARGET wxl::unicode)
    add_subdirectory("${BESEDKA_WXL_ROOT}/wxl.unicode/src" "${CMAKE_BINARY_DIR}/wxl.unicode" EXCLUDE_FROM_ALL)
endif()

# Будущее (wxl::future), которым транспорт отдаёт ответ, и всё, на чём оно
# стоит.
if(NOT TARGET wxl::async)
    add_subdirectory("${BESEDKA_WXL_ROOT}/wxl.async/src" "${CMAKE_BINARY_DIR}/wxl.async" EXCLUDE_FROM_ALL)
endif()

# Читатель JSON, а с ним и wxl.xml: арену -- схему хранения дерева -- он
# берёт оттуда, а не заводит свою.
if(NOT TARGET wxl::xml)
    add_subdirectory("${BESEDKA_WXL_ROOT}/wxl.xml/src" "${CMAKE_BINARY_DIR}/wxl.xml" EXCLUDE_FROM_ALL)
endif()
if(NOT TARGET wxl::json)
    add_subdirectory("${BESEDKA_WXL_ROOT}/wxl.json/src" "${CMAKE_BINARY_DIR}/wxl.json" EXCLUDE_FROM_ALL)
endif()

# Семейство разметок -- их требует wxl.winui: контролы HtmlBlock, BbBlock и
# RsdnBlock линкуют свои парсеры. Показ сообщения стоит на последнем: ради
# него Беседка и заводится.
if(NOT TARGET wxl::html)
    add_subdirectory("${BESEDKA_WXL_ROOT}/wxl.html/src" "${CMAKE_BINARY_DIR}/wxl.html" EXCLUDE_FROM_ALL)
endif()
if(NOT TARGET wxl::bb)
    add_subdirectory("${BESEDKA_WXL_ROOT}/wxl.bb/src" "${CMAKE_BINARY_DIR}/wxl.bb" EXCLUDE_FROM_ALL)
endif()
if(NOT TARGET wxl::highlight)
    add_subdirectory("${BESEDKA_WXL_ROOT}/wxl.highlight/src" "${CMAKE_BINARY_DIR}/wxl.highlight" EXCLUDE_FROM_ALL)
endif()
if(NOT TARGET wxl::rsdn)
    add_subdirectory("${BESEDKA_WXL_ROOT}/wxl.rsdn/src" "${CMAKE_BINARY_DIR}/wxl.rsdn" EXCLUDE_FROM_ALL)
endif()

# wxl.winui -- проекция WinUI 3, на которой стоит приложение. Здесь, в
# отличие от модулей выше, берётся корневой CMakeLists модуля: он и есть весь
# модуль, а генератор с его winmd живёт отдельной целью в дереве wxl и сюда
# не приезжает.
if(WIN32 AND NOT TARGET wxl::winui)
    add_subdirectory("${BESEDKA_WXL_ROOT}/wxl.winui" "${CMAKE_BINARY_DIR}/wxl.winui" EXCLUDE_FROM_ALL)
endif()

# Цель, чьи исходники видят заголовки wxl, компилирует их `import std;` сама:
# без этих двух свойств MSVC не найдёт модуль std, а CMake не станет сканировать
# обычные .cpp на импорты.
function(besedka_consumes_wxl target)
    set_target_properties(${target} PROPERTIES
        CXX_MODULE_STD ON
        CXX_SCAN_FOR_MODULES ON
    )
endfunction()

# Своё Беседки — строго после подключения wxl: у wxl свои NOMINMAX и
# WIN32_LEAN_AND_MEAN в platform.h, и наши дали бы C4005 на каждом её файле.
if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

option(BESEDKA_BUILD_TESTS "Собирать тесты" ON)
if(BESEDKA_BUILD_TESTS)
    enable_testing()

    # Тот же gtest, что у wxl, и та же обвязка (wxl_add_gtest): у Беседки
    # нет причин заводить второй способ писать тесты.
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)
endif()

add_subdirectory(Rsdn)
add_subdirectory(Forum)

# Само приложение -- только под Windows: оно и есть WinUI.
if(WIN32)
    add_subdirectory(App)
endif()
