# App -- сама Беседка на wxl.ui, в двух частях.
#
# core/ -- модуль besedka.app: то, что приложение решает без окон -- маршруты,
# история переходов, план страниц, настройки, даты, группировка списков. Ни
# XAML, ни сети; отсюда тесты в tests/.
#
# src/ -- исполняемый файл: окно, каркас, экраны и переходы между ними. Он
# линкуется с wxl::ui.full, besedka::forum и besedka::app, и больше ничем.
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
        core/notice.ixx
        core/workspace.ixx
    SRC
        core/route.cpp
        core/history.cpp
        core/pages.cpp
        core/settings.cpp
        core/format.cpp
        core/lists.cpp
        core/geometry.cpp
        core/notice.cpp
        core/workspace.cpp
)

add_library(besedka::app ALIAS besedka.app)

# PUBLIC: модель форума стоит в интерфейсе (маршрут несёт форум и тему,
# списки принимают страницу сообщений), а с ней приезжает wxl::core -- со
# всем, что от него нужно модулю: задачи (task) в интерфейсе рабочего места,
# сбой операции с файлом (last_error) в словах читателю, построитель текста и
# читатель XML в settings.cpp. Всё это области одного модуля.
target_link_libraries(besedka.app PUBLIC
    besedka::forum
    wxl::core
)

# Каталог данных спрашивается у оболочки (SHGetKnownFolderPath).
target_link_libraries(besedka.app PRIVATE shell32)

# ---- Besedka: окно, каркас, экраны ----

# <windows.h> включается здесь раньше platform.h из wxl, а без этих определений
# он тянет макросы min и max, которые ломают заголовки wxl.ui.
add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)

wxl_add_executable(Besedka
    UI wxl::ui.full
    src/main.cpp
    src/navigator.cpp
    src/shell.cpp
    src/page_host.cpp
    src/splash_screen.cpp
    src/stub_screens.cpp
    src/about_dialog.cpp
    src/forum_screen.cpp
    src/topic_screen.cpp
    src/message_screen.cpp
    src/notices.cpp
    src/deferred_write.h
    src/notices.h
    src/navigator.h
    src/shell.h
    src/page_host.h
    src/splash_screen.h
    src/stub_screens.h
    src/about_dialog.h
    src/forum_screen.h
    src/topic_screen.h
    src/message_screen.h
    src/palette.h
    src/pch.h
)

target_link_libraries(Besedka PRIVATE
    besedka::forum
    besedka::app
)

# Всё, что приложение показывает, но не компилирует, лежит в одном каталоге --
# src/Assets: картинки окна (заставка и задник чтения) и смайлы форума. Оттуда
# каталог целиком едет рядом с исполняемым файлом, потому что путь без схемы
# XAML разрешает именно оттуда, а смайлы токенизатор wxl.rsdn называет как
# <img src="smiles/...">, разрешая их от baseDirectory.
#
# Смайлы -- копия набора wxl.rsdn: там же лежит и таблица кодов, по которой
# токенизатор выбирает файл. Разойдутся -- в сообщении появится пустой квадрат
# вместо смайла, и лечится это переносом файла оттуда сюда.
wxl_target_assets(Besedka src/Assets)

if(BESEDKA_BUILD_TESTS)
    add_subdirectory(tests)
endif()
