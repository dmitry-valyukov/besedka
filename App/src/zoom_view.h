#pragma once
// Прокрутка, через которую приходит масштаб от щипка и Ctrl+колеса.
//
// Масштаб у приложения один -- всего окна: заголовок, панели и страницы растут
// вместе (CompositionWindow::zoom). Жесты его ScrollViewer обрабатывает сам и
// увеличивает своё содержимое как картинку. Здесь это увеличение только
// читается: устоявшийся множитель сообщается наружу -- на него умножают
// масштаб окна, -- а собственный зум прокрутки возвращается к единице.

#include <functional>

#include "pch.h"

namespace besedka::app {

class ZoomView {
public:
    /// Содержимое -- список. Место в сетке ставит тот, кто кладёт `root()` в
    /// неё.
    explicit ZoomView(const wxl::FrameworkElement& content);

    ZoomView(const ZoomView&) = delete;
    ZoomView& operator=(const ZoomView&) = delete;

    const wxl::ScrollViewer& root() const { return root_.value(); }

    /// Читатель увеличил или уменьшил список щипком или Ctrl+колесом -- во
    /// столько раз.
    std::function<void(double)> onZoomChanged;

private:
    wxl::core::nullable<wxl::ScrollViewer> root_ = nullptr;
};

}  // namespace besedka::app
