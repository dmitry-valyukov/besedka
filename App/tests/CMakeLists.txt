# Тесты besedka.app: истории, плана страниц, настроек, дат, списков, окна --
# всего, что приложение решает без XAML. Ни окна, ни сети здесь нет.
wxl_add_gtest(besedka.app.tests
    SRC
        test_initializer.cpp
        navigation_test.cpp
        settings_test.cpp
        format_test.cpp
        lists_test.cpp
        geometry_test.cpp
    DEPS
        besedka::app
        besedka::forum
        wxl::text
)
