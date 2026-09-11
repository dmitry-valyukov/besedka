#include "zoom_view.h"

#include <cmath>
#include <iterator>
#include <optional>

import besedka.app;

namespace besedka::app {

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Масштаб у ScrollViewer -- float, а у нас double: равенство с допуском.
constexpr double kSame = 1e-4;

// Сколько раз переспрашивать ChangeView, прежде чем оставить как есть.
constexpr int kRetries = 3;

bool same(const double left, const double right) noexcept { return std::abs(left - right) < kSame; }

}  // namespace

ZoomView::ZoomView(const FrameworkElement& content) : content_(content) {
    root_ = ScrollViewer{
        wxl::dsl::content = content_,
        zoomMode = ZoomMode::Enabled,
        // Горизонтали нет: содержимое всегда ровно в ширину окна.
        horizontalScrollMode = ScrollMode::Disabled,
        horizontalScrollBarVisibility = ScrollBarVisibility::Disabled,

        onSizeChanged = [this](Object const&, SizeChangedEventArgs&) { apply(); },

        onViewChanged =
            [this](Object const&, ScrollViewerViewChangedEventArgs& args) {
                const double factor = root_.value().zoomFactor();

                if (!same(factor, zoom_)) {
                    zoom_ = factor;
                    apply();
                }

                if (args.isIntermediate() || same(zoom_, reported_)) return;

                reported_ = zoom_;

                if (onZoomChanged) onZoomChanged(zoom_);
            },
    };

    root_.value().minZoomFactor(static_cast<float>(kZoomSteps[0]));
    root_.value().maxZoomFactor(static_cast<float>(std::end(kZoomSteps)[-1]));
}

void ZoomView::zoom(const double factor) {
    if (same(factor, zoom_)) return;

    zoom_ = factor;
    reported_ = factor;
    retries_ = 0;

    apply();
}

void ZoomView::apply() {
    const ScrollViewer& viewer = root_.value();
    const double viewport = viewer.viewportWidth();

    if (viewport <= 0) return;

    // Сначала масштаб, потом ширина. ChangeView молча отказывает, когда у
    // ScrollViewer не досчитана вёрстка, а смена ширины содержимого как раз
    // её и пачкает: на левой странице масштаб от щипка справа не приезжал,
    // и список стоял узким в единице. Отказ всё же случается -- тогда ещё
    // раз, когда очередь дойдёт до нас, то есть после вёрстки; и не до
    // бесконечности.
    if (!same(viewer.zoomFactor(), zoom_) &&
        !viewer.changeView(std::nullopt, std::nullopt, static_cast<float>(zoom_), true) &&
        retries_ < kRetries) {
        ++retries_;
        DispatcherQueue::getForCurrentThread().tryEnqueue([this] { apply(); });
        return;
    }

    content_.width(viewport / zoom_);
}

}  // namespace besedka::app
