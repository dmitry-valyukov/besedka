# App -- сама Беседка на wxl.winui.
#
# Линкуется с wxl::winui и besedka::forum: интерфейс и сервер, и больше
# ничего. Доступ к декларативной поверхности не должен стоить приложению
# разбора проекции cppwinrt, и отсутствие её include-пути здесь -- то, что
# это гарантирует.
#
# WIN32 делает исполняемый файл оконным, а оконным его точкой входа
# становится wWinMain, который живёт в самой wxl.

add_executable(Besedka WIN32
    src/main.cpp
    src/settings.cpp
    src/shell.cpp
    src/splash_screen.cpp
    src/stub_screens.cpp
    src/about_dialog.cpp
    src/forum_screen.cpp
    src/topic_screen.cpp
    src/message_screen.cpp
    src/settings.h
    src/shell.h
    src/splash_screen.h
    src/stub_screens.h
    src/about_dialog.h
    src/forum_screen.h
    src/topic_screen.h
    src/message_screen.h
    src/pch.h
    src/app.manifest
)

target_link_libraries(Besedka PRIVATE
    wxl::winui
    wxl::text
    besedka::forum
)

besedka_consumes_wxl(Besedka)

# Картинки окна: заставка и задник чтения. Путь без схемы XAML разрешает
# рядом с исполняемым файлом, поэтому обе едут туда -- копируются на каждой
# сборке, потому что правка картинки не повод перелинковывать.
add_custom_command(TARGET Besedka POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Besedka>/Assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/art/splash-screen.png"
            "$<TARGET_FILE_DIR:Besedka>/Assets/splash-screen.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/art/forum.png"
            "$<TARGET_FILE_DIR:Besedka>/Assets/forum.png"
    COMMENT "Копирование картинок окна рядом с Беседкой")

# Смайлы форума: токенизатор wxl.rsdn выдаёт <img src="smiles/...">, и путь
# этот разрешается от каталога, названного baseDirectory. Картинки живут в
# самой wxl -- у неё же лежит и таблица смайлов.
add_custom_command(TARGET Besedka POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${BESEDKA_WXL_ROOT}/wxl.rsdn/smiles" "$<TARGET_FILE_DIR:Besedka>/Assets/smiles"
    COMMENT "Копирование смайлов форума рядом с Беседкой")

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
