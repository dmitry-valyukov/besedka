# App -- сама Беседка на wxl.winui, в двух частях.
#
# core/ -- модуль besedka.app: то, что приложение решает без окон -- маршруты,
# история переходов, план страниц, настройки, даты, группировка списков. Ни
# XAML, ни сети; отсюда тесты в tests/.
#
# src/ -- исполняемый файл: окно, каркас, экраны и переходы между ними. Он
# линкуется с wxl::winui, besedka::forum и besedka::app, и больше ничем.
# Доступ к декларативной поверхности не должен стоить приложению разбора
# проекции cppwinrt, и отсутствие её include-пути здесь -- то, что это
# гарантирует.
#
# WIN32 делает исполняемый файл оконным, а оконным его точкой входа
# становится wWinMain, который живёт в самой wxl.

# ---- besedka.app: приложение без окон ----

wxl_add_module_library(besedka.app STATIC
    PUBLIC
        core/app.ixx

        core/route.ixx
        core/history.ixx
        core/pages.ixx
        core/settings.ixx
        core/format.ixx
        core/lists.ixx
        core/geometry.ixx
    SRC
        core/route.cpp
        core/history.cpp
        core/pages.cpp
        core/settings.cpp
        core/format.cpp
        core/lists.cpp
        core/geometry.cpp
)

add_library(besedka::app ALIAS besedka.app)

# PUBLIC: модель форума стоит в интерфейсе (маршрут несёт форум и тему,
# списки принимают страницу сообщений), а с ней приезжает wxl::core.
# wxl::xml -- только настройкам, но модуль один, и его читатель
# XML линкуется вместе с остальным.
target_link_libraries(besedka.app PUBLIC
    besedka::forum
    wxl::core
    wxl::xml
)

# Каталог данных спрашивается у оболочки (SHGetKnownFolderPath).
target_link_libraries(besedka.app PRIVATE shell32)

# ---- Besedka: окно, каркас, экраны ----

add_executable(Besedka WIN32
    src/main.cpp
    src/navigator.cpp
    src/settings_writer.cpp
    src/shell.cpp
    src/page_host.cpp
    src/zoom_view.cpp
    src/splash_screen.cpp
    src/stub_screens.cpp
    src/about_dialog.cpp
    src/forum_screen.cpp
    src/topic_screen.cpp
    src/message_screen.cpp
    src/navigator.h
    src/settings_writer.h
    src/shell.h
    src/page_host.h
    src/zoom_view.h
    src/splash_screen.h
    src/stub_screens.h
    src/about_dialog.h
    src/forum_screen.h
    src/topic_screen.h
    src/message_screen.h
    src/palette.h
    src/pch.h
    src/app.manifest
)

target_link_libraries(Besedka PRIVATE
    wxl::winui
    besedka::forum
    besedka::app
)

besedka_consumes_wxl(Besedka)

# Всё, что приложение показывает, но не компилирует, лежит в одном каталоге --
# src/Assets: картинки окна (заставка и задник чтения) и смайлы форума. Оттуда
# каталог целиком едет рядом с исполняемым файлом, потому что путь без схемы
# XAML разрешает именно оттуда, а смайлы токенизатор wxl.rsdn называет как
# <img src="smiles/...">, разрешая их от baseDirectory.
#
# Один каталог, а не три источника: добавленная картинка едет с остальными,
# ничего не дописывая сюда. Копируется на каждой сборке -- правка картинки не
# повод перелинковывать, -- и copy_directory сам не трогает то, что не менялось.
#
# Смайлы -- копия набора wxl.rsdn: там же лежит и таблица кодов, по которой
# токенизатор выбирает файл. Разойдутся -- в сообщении появится пустой квадрат
# вместо смайла, и лечится это переносом файла оттуда сюда.
add_custom_command(TARGET Besedka POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/src/Assets" "$<TARGET_FILE_DIR:Besedka>/Assets"
    COMMENT "Копирование ресурсов рядом с Беседкой")

# Загрузчик Windows App Runtime находит пакет платформы при старте, поэтому
# его DLL обязана лежать рядом.
set(_besedka_bootstrap_dll
    "${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/runtimes/win-x64/native/Microsoft.WindowsAppRuntime.Bootstrap.dll")

if(EXISTS "${_besedka_bootstrap_dll}")
    add_custom_command(TARGET Besedka POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_besedka_bootstrap_dll}" "$<TARGET_FILE_DIR:Besedka>"
        COMMENT "Копирование загрузчика Windows App Runtime рядом с Беседкой")
else()
    message(WARNING
        "Нет Microsoft.WindowsAppRuntime.Bootstrap.dll по пути ${_besedka_bootstrap_dll}; "
        "Беседка не запустится, пока она не окажется рядом с исполняемым файлом.")
endif()

# Win2D (Microsoft.Graphics.Canvas) активируется неупакованно, из своей нативной
# DLL рядом с exe: ею CompositionWindow грузит картинки задника (TextureCache),
# и без неё окно падает при создании. Путь объявляет wxl.winui.
set(_besedka_win2d_dll
    "${WXL_WIN2D_DIR}/runtimes/win-x64/native/Microsoft.Graphics.Canvas.dll")

if(EXISTS "${_besedka_win2d_dll}")
    add_custom_command(TARGET Besedka POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_besedka_win2d_dll}" "$<TARGET_FILE_DIR:Besedka>"
        COMMENT "Копирование Microsoft.Graphics.Canvas.dll (Win2D) рядом с Беседкой")
else()
    message(WARNING
        "Нет Microsoft.Graphics.Canvas.dll по пути ${_besedka_win2d_dll}; "
        "Беседка упадёт при создании окна, пока эта DLL не окажется рядом с ней.")
endif()

if(BESEDKA_BUILD_TESTS)
    add_subdirectory(tests)
endif()
