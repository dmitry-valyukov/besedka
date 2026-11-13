// Сбой сценария словами: операция с файлом -- причиной системы, исключение с
// текстом -- его текстом, если он UTF-8, всё прочее -- фактом.

#include <gtest/gtest.h>

#include <windows.h>

#include <exception>
#include <stdexcept>
#include <string>

import besedka.app;
import wxl.core;

using namespace besedka::app;
using wxl::async::last_error;

TEST(notice, a_file_operation_is_told_in_the_systems_words) {
    const last_error failure("ReadFile", static_cast<int>(ERROR_FILE_NOT_FOUND));
    const Notice onFile = noticeOf(std::make_exception_ptr(failure));

    // Заголовок о файле, а не «Ошибка».
    EXPECT_EQ(onFile.headline, L"Не удалось выполнить операцию с файлом");
    EXPECT_EQ(onFile.details, reasonOf(failure));

    // Причина -- слова системы на языке читателя, без имени вызова и кода.
    EXPECT_FALSE(onFile.details.empty());
    EXPECT_EQ(onFile.details, failure.explanation());
}

TEST(notice, a_code_the_system_cannot_name_leaves_the_diagnostic) {
    const last_error nonsense("ReadFile", 0x1FFFFFFF);
    const std::wstring diagnostic = reasonOf(nonsense);

    // Что делали и код.
    EXPECT_NE(diagnostic.find(L"ReadFile"), std::wstring::npos);
    EXPECT_NE(diagnostic.find(L"536870911"), std::wstring::npos);
}

TEST(notice, a_failure_with_a_file_names_it_and_the_reason_after_an_empty_line) {
    const last_error failure("ReadFile", static_cast<int>(ERROR_FILE_NOT_FOUND));

    const Notice withPath = noticeOf(L"Не удалось прочитать настройки", L"C:\\данные\\settings.xml", failure);
    EXPECT_EQ(withPath.headline, L"Не удалось прочитать настройки");
    EXPECT_EQ(withPath.details, L"C:\\данные\\settings.xml\n\n" + reasonOf(failure));

    // Без пути -- одна причина, без пустых строк впереди.
    EXPECT_EQ(noticeOf(L"Не удалось", L"", failure).details, reasonOf(failure));
}

TEST(notice, an_exception_with_utf8_text_is_told_by_its_text) {
    const Notice said = noticeOf(std::make_exception_ptr(std::runtime_error("Файл испорчен")));

    EXPECT_EQ(said.headline, L"Ошибка");
    EXPECT_EQ(said.details, L"Файл испорчен");
}

TEST(notice, text_that_is_not_utf8_is_said_to_be_so) {
    const Notice garbled = noticeOf(std::make_exception_ptr(std::runtime_error("\xFF\xFE broken")));

    EXPECT_EQ(garbled.headline, L"Ошибка");
    EXPECT_EQ(garbled.details, L"(сообщение не в UTF-8)");
}

TEST(notice, something_that_is_not_an_exception_is_just_a_fact) {
    const Notice unknown = noticeOf(std::make_exception_ptr(42));

    EXPECT_EQ(unknown.headline, L"Неизвестная ошибка");
    EXPECT_EQ(unknown.details, L"Сценарий прерван.");
}
