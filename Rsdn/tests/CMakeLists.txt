wxl_add_gtest(besedka.rsdn.tests
    SRC
        test_initializer.cpp
        rsdn_test.cpp
    DEPS
        besedka::rsdn
        wxl::core
)
