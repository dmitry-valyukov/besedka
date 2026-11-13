# Forum -- всё, что касается сервера RSDN: запросы, разбор ответов, модель
# сообщений. Об интерфейсе не знает и знать не будет.
#
# Модуль C++20, а не заголовки: заголовкам пришлось бы прятать winrt от
# всех, кто их включает, а модуль прячет его сам -- то, что не
# переэкспортировано, наружу не выходит.

wxl_add_module_library(besedka.forum STATIC
    PUBLIC
        src/forum.ixx

        src/model.ixx
        src/transport.ixx
        src/parse.ixx
        src/api.ixx
    SRC
        src/transport.cpp
        src/parse.cpp
        src/api.cpp
)

add_library(besedka::forum ALIAS besedka.forum)

# PUBLIC: всё, что стоит в интерфейсе модуля, -- будущее из области async,
# дерево из области json (его же :parse принимает параметром), проверенный
# текст, -- приходит одним модулем wxl.core.
target_link_libraries(besedka.forum PUBLIC wxl::core)

# А это -- только реализации: заголовки winrt/Windows.* включает
# transport.cpp, и наружу они не выходят.
target_link_libraries(besedka.forum PRIVATE
    Microsoft::CppWinRT
    WindowsApp.lib
)

# Проба: ходит на api.rsdn.org и печатает, что оттуда пришло. Смысл её не в
# выводе, а в том, что вся дорога проверена на живом сервере.
add_executable(forum-probe src/probe.cpp)

# Проба включает <windows.h> сама, не через platform.h из wxl, который задаёт
# эти определения остальному дереву.
target_compile_definitions(forum-probe PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

# Microsoft::CppWinRT здесь ради одного вызова -- init_apartment(): проба
# заводит апартамент STA сама, потому что дальше в него возвращаются ответы.
target_link_libraries(forum-probe PRIVATE
    besedka::forum
    wxl::core
    Microsoft::CppWinRT
    WindowsApp.lib
)

if(BESEDKA_BUILD_TESTS)
    add_subdirectory(tests)
endif()
