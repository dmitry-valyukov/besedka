#pragma once
// «О программе»: кто мы, какой версии и с каким сервером разговариваем.
//
// Порт `ui/components/AboutDialog.kt` из jana. Там это карточка с заставкой
// во весь фон, крестиком в углу и подвалом, где ссылка на сайт, версия
// клиента и версия сервера; здесь -- ContentDialog, потому что модальное
// окно поверх содержимого в WinUI называется так и умеет всё то же само.
//
// Версия сервера приезжает отдельно и позже: её спрашивают у `/service_info`,
// и до ответа строчки просто нет -- ровно как у jana, где `serviceInfo?.let`.

#include <string>
#include <string_view>

#include "pch.h"

namespace besedka::app {

class AboutDialog {
public:
    AboutDialog();

    /// Показывает поверх содержимого окна. Элемент, а не окно: XAML приходит в
    /// окно островом, и диалог вешается над тем островом, в котором стоит
    /// `host`.
    ///
    /// Возврат сразу: диалог живёт, пока читатель не закроет его сам.
    void show(const wxl::UIElement& host);

    /// Строчка про сервер: «API v3.1 [2026-08-30]». Пустая -- значит, сервер
    /// ещё не ответил, и её не видно.
    void setServerLine(std::wstring_view said);

private:
    wxl::Nullable<wxl::ContentDialog> dialog_ = nullptr;
    wxl::Nullable<wxl::TextBlock> server_ = nullptr;
};

}  // namespace besedka::app
