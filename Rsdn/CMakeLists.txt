# Rsdn -- разметка форума RSDN: токенизатор, который превращает текст
# сообщения в дерево семейства wxl.html.
#
# Живёт здесь, а не в wxl, и это переезд, а не рождение: разметка эта
# принадлежит одному форуму, а не библиотеке интерфейса. Общее -- дерево,
# сборщик и подсветка -- осталось в wxl, где ему и место; своё -- запись
# тегов rsdn.org, построчные цитаты с префиксом, смайлы и языковые теги кода
# -- переехало к тому единственному приложению, которому оно нужно.

wxl_add_module_library(besedka.rsdn STATIC
    PUBLIC
        src/besedka.rsdn.ixx
    SRC
        src/rsdn.cpp
)

add_library(besedka::rsdn ALIAS besedka.rsdn)

# PUBLIC: parse() отвечает html::document -- дерево семейства стоит в
# интерфейсе, и с ним приезжают wxl::core и wxl::xml.
target_link_libraries(besedka.rsdn PUBLIC wxl::html)

# PRIVATE: подсветка кода живёт внутри токенизатора -- наружу выходит дерево
# семейства со <span style="kw">, а не куски wxl.highlight.
target_link_libraries(besedka.rsdn PRIVATE wxl::highlight)

# Как в wxl.html: исключением может ответить только аллокация.
if(MSVC AND NOT CMAKE_CXX_FLAGS MATCHES "/EHsc")
    target_compile_options(besedka.rsdn PUBLIC /EHsc)
endif()

if(BESEDKA_BUILD_TESTS)
    add_subdirectory(tests)
endif()
