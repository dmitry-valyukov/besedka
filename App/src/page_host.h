#pragma once
// Середина каркаса: одна или две страницы и граница между ними.
//
// Хост не знает ни истории, ни маршрутов. Ему говорят план (planPages из
// besedka.app) -- кого снять, кого поставить, кому какая сторона, -- и он
// делает ровно это, не трогая тех, кто остаётся. Так щёлкнутая страница, чьё
// поддерево XAML в этот момент разбирает, никогда не покидает дерева: план
// считается в модуле и проверен тестами, а здесь только руки.
//
// Граница -- обычный Border с захватом указателя, а не контрол библиотеки: в
// WinUI 3 разделителя нет (см. docs/decisions.md), и пока он нужен одному
// приложению, тридцать строк здесь честнее новой машинерии в wxl.

#include <functional>
#include <span>
#include <utility>
#include <vector>

#include "pch.h"

import besedka.app;

namespace besedka::app {

class PageHost {
public:
    PageHost();

    const wxl::UIElement& root() const { return root_.value(); }

    /// Корень экрана каждого рода. Ставится раз, при сборке: экраны живут
    /// столько же, сколько приложение, и хост лишь ставит их в дерево и снимает.
    void setScreen(Screen screen, const wxl::UIElement& root);

    /// Что стоит в дереве сейчас -- вход для planPages.
    std::span<const Screen> inTree() const noexcept { return inTree_; }

    /// Одна колонка или две с границей между ними.
    void setWide(bool wide);

    /// Сделать с деревом ровно то, что сказано.
    void apply(const PagePlan& plan);

    /// Доля левой страницы в ширине, 0,2…0,8. Ставит запуск из настроек и
    /// двигает читатель, потянув границу.
    void splitFraction(double value);
    double splitFraction() const noexcept { return fraction_; }

    /// Читатель отпустил границу на новом месте -- долю пора запомнить.
    std::function<void(double)> onSplitChanged;

private:
    const wxl::UIElement& elementOf(Screen screen) const;

    std::vector<std::pair<Screen, wxl::UIElement>> screens_;

    /// Страницы в дереве, в порядке добавления: страница с номером `at` --
    /// ребёнок сетки `kFirstPage + at`, потому что нулевым стоит граница.
    std::vector<Screen> inTree_;

    bool wide_ = false;
    double fraction_ = 0.5;

    // Тяга границы. Захват указателя делает её тягой, а не «пока мышь над
    // полоской»: полоска шириной в шесть пикселей теряет мышь на первом же
    // быстром движении.
    bool dragging_ = false;

    wxl::core::nullable<wxl::Grid> root_ = nullptr;
    wxl::core::nullable<wxl::Border> splitter_ = nullptr;
};

}  // namespace besedka::app
