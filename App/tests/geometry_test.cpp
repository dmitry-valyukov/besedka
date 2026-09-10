// Окно под картинку заставки.

#include <gtest/gtest.h>

import besedka.app;

using namespace besedka::app;

TEST(geometry, a_square_picture_on_a_wide_screen_takes_the_whole_height) {
    // Экран 1920x1040 (без панели задач), картинка квадратная: квадрат во
    // всю высоту.
    EXPECT_EQ(fitToPicture({1920, 1040}, {1254, 1254}), (Extent{1040, 1040}));
}

TEST(geometry, a_picture_wider_than_the_screen_is_limited_by_the_width) {
    EXPECT_EQ(fitToPicture({1000, 1200}, {1254, 1254}), (Extent{1000, 1000}));
    EXPECT_EQ(fitToPicture({800, 1000}, {1600, 800}), (Extent{800, 400}));
}

TEST(geometry, the_pictures_proportions_are_kept) {
    const Extent client = fitToPicture({1920, 1040}, {2000, 1000});

    EXPECT_EQ(client.height, 960);
    EXPECT_EQ(client.width, 1920);
}
