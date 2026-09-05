# Forum — всё, что касается сервера RSDN: запросы, разбор ответов, модель
# сообщений. Об интерфейсе не знает и знать не будет.
#
# Пока здесь одна проба: она ходит на api.rsdn.org и печатает список форумов.
# Смысл её не в списке, а в доказательстве, что дорога есть — HTTPS без единой
# своей строчки TLS и без чужой библиотеки.

find_package(cppwinrt CONFIG REQUIRED)

add_executable(forum-probe src/probe.cpp)
besedka_consumes_wxl(forum-probe)

# Microsoft::CppWinRT даёт заголовки winrt/Windows.*, WindowsApp.lib — их
# реализацию. Проекция WinUI 3 здесь не нужна: сеть живёт в пространствах
# Windows.*, которые приезжают с самим пакетом cppwinrt.
target_link_libraries(forum-probe PRIVATE
    wxl::text
    Microsoft::CppWinRT
    WindowsApp.lib
)
