#pragma once
// Прокрутка с масштабом, при котором текст переливается, а не растягивается.
//
// У ScrollViewer масштаб есть свой (ZoomMode), но он увеличивает содержимое
// как картинку: строки уходят за правый край, и появляется горизонтальная
// прокрутка. Здесь содержимому даётся ширина viewport / масштаб -- оно
// верстается уже, а показанное в масштабе ровно заполняет окно; так делает
// браузер. Щипок и Ctrl+колесо ScrollViewer обрабатывает сам, после них
// ширина пересчитывается по новому множителю, а устоявшийся множитель
// сообщается наружу -- его запоминают.

#include <functional>

#include "pch.h"

namespace besedka::app {

class ZoomView {
public:
    /// Содержимое -- список, которому и дают ширину. Место в сетке ставит
    /// тот, кто кладёт `root()` в неё.
    explicit ZoomView(const wxl::FrameworkElement& content);

    ZoomView(const ZoomView&) = delete;
    ZoomView& operator=(const ZoomView&) = delete;

    const wxl::ScrollViewer& root() const { return root_.value(); }

    void zoom(double factor);
    double zoom() const noexcept { return zoom_; }

    /// Масштаб сменился руками читателя -- щипком или Ctrl+колесом -- и
    /// устоялся.
    std::function<void(double)> onZoomChanged;

private:
    /// Ширина содержимого под масштаб, и сам масштаб, если ScrollViewer ещё
    /// не в нём. До первой разметки viewport пуст, и всё это ждёт SizeChanged.
    void apply();

    wxl::FrameworkElement content_;
    double zoom_ = 1;

    /// Что уже сообщили наружу: ViewChanged приходит и на прокрутку, и
    /// сообщать о масштабе надо только когда он и правда сменился.
    double reported_ = 1;

    /// Сколько раз ChangeView отказал на этом масштабе.
    int retries_ = 0;

    wxl::Nullable<wxl::ScrollViewer> root_ = nullptr;
};

}  // namespace besedka::app
