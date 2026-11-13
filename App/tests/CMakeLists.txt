# Тесты besedka.app: истории, плана страниц, настроек, дат, списков, окна,
# слов о сбоях и рабочего места -- всего, что приложение решает без XAML. Ни
# окна, ни сети здесь нет; файлы рабочее место пишет во временный каталог.
wxl_add_gtest(besedka.app.tests
    SRC
        navigation_test.cpp
        settings_test.cpp
        format_test.cpp
        lists_test.cpp
        geometry_test.cpp
        notice_test.cpp
        workspace_test.cpp
    DEPS
        besedka::app
        besedka::forum
        wxl::core
)
