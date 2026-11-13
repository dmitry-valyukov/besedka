// Настройки туда и обратно -- строками, без файла: файл проверяют тесты
// рабочего места (workspace_test.cpp), а формат -- вот это.

#include <gtest/gtest.h>

#include <string>

import besedka.app;

using namespace besedka::app;

namespace {

/// Настройки, прочитанные из текста, который обязан разобраться.
Settings readable(const std::string& xml) {
    Settings read;
    EXPECT_TRUE(readSettings(xml, read)) << xml;
    return read;
}

}  // namespace

TEST(settings, defaults_when_there_is_nothing_to_read) {
    const Settings none = readable("");

    EXPECT_TRUE(none.windowPlacement.empty());
    EXPECT_DOUBLE_EQ(none.splitFraction, 0.5);
}

TEST(settings, what_is_written_is_what_is_read) {
    Settings written;

    written.windowPlacement = L"2,3,-1,-1,-1,-1,100,100,1380,960";
    written.splitFraction = 0.375;
    written.zoom = 1.25;

    const Settings read = readable(formatSettings(written));

    EXPECT_EQ(read.windowPlacement, written.windowPlacement);
    EXPECT_DOUBLE_EQ(read.splitFraction, written.splitFraction);
    EXPECT_DOUBLE_EQ(read.zoom, written.zoom);
}

TEST(settings, zoom_defaults_to_one_and_survives_a_missing_or_unreadable_value) {
    EXPECT_DOUBLE_EQ(readable("").zoom, 1.0);
    EXPECT_DOUBLE_EQ(readable("<settings version=\"1\"><layout split=\"0.5\"/></settings>").zoom, 1.0);
    EXPECT_DOUBLE_EQ(readable("<settings version=\"1\"><view zoom=\"big\"/></settings>").zoom, 1.0);
}

TEST(settings, the_fraction_is_written_with_a_dot_and_three_digits) {
    Settings settings;

    settings.splitFraction = 1.0 / 3.0;

    const std::string xml = formatSettings(settings);

    EXPECT_NE(xml.find("split=\"0.333\""), std::string::npos) << xml;
}

TEST(settings, a_broken_file_is_not_read_and_leaves_the_settings_alone) {
    Settings settings;

    settings.windowPlacement = L"x";
    settings.splitFraction = 0.25;

    EXPECT_FALSE(readSettings("<settings version=\"1\"><window placement=\"", settings));

    // Ни одно поле не тронуто: битый файл не оставляет настроек, прочитанных
    // наполовину.
    EXPECT_EQ(settings.windowPlacement, L"x");
    EXPECT_DOUBLE_EQ(settings.splitFraction, 0.25);
}

TEST(settings, an_unknown_field_and_a_missing_one_are_both_fine) {
    const Settings read = readable(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<settings version=\"7\">\n"
        "  <window placement=\"x\" colour=\"red\"/>\n"
        "  <future thing=\"1\"/>\n"
        "</settings>\n");

    EXPECT_EQ(read.windowPlacement, L"x");
    EXPECT_DOUBLE_EQ(read.splitFraction, 0.5);
}

TEST(settings, an_unreadable_fraction_keeps_the_default) {
    const Settings read = readable("<settings version=\"1\"><layout split=\"half\"/></settings>");

    EXPECT_DOUBLE_EQ(read.splitFraction, 0.5);
}

TEST(settings, a_lone_surrogate_from_windows_is_repaired_rather_than_poisoning_the_file) {
    Settings written;

    written.windowPlacement = L"a\xD800z";

    const Settings read = readable(formatSettings(written));

    // U+FFFD вместо испорченной единицы, а остальное -- как было. Без этого
    // при следующем запуске wxl::xml отвергла бы файл целиком.
    EXPECT_EQ(read.windowPlacement, L"a\xFFFDz");
}

TEST(settings, the_value_is_escaped_for_xml) {
    Settings written;

    written.windowPlacement = L"<a & \"b\">";

    const Settings read = readable(formatSettings(written));

    EXPECT_EQ(read.windowPlacement, written.windowPlacement);
}
