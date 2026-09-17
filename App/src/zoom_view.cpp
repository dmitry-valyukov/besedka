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

bool same(const double left, const double right) noexcept { return std::abs(left - right) < kSame; }

}  // namespace

ZoomView::ZoomView(const FrameworkElement& content) {
    root_ = ScrollViewer{
        wxl::dsl::content = content,
        zoomMode = ZoomMode::Enabled,
        // Горизонтали нет: содержимое всегда ровно в ширину окна.
        horizontalScrollMode = ScrollMode::Disabled,
        horizontalScrollBarVisibility = ScrollBarVisibility::Disabled,

        onViewChanged =
            [this](Object const&, ScrollViewerViewChangedEventArgs& args) {
                const ScrollViewer& viewer = root_.value();
                const double factor = viewer.zoomFactor();

                if (args.isIntermediate() || same(factor, 1)) return;

                // Сначала назад к единице, потом наружу: масштаб окна,
                // умноженный на жест, уже включает то, что прокрутка показала.
                viewer.changeView(std::nullopt, std::nullopt, 1.0f, true);

                if (onZoomChanged) onZoomChanged(factor);
            },
    };

    root_.value().minZoomFactor(static_cast<float>(kZoomSteps[0]));
    root_.value().maxZoomFactor(static_cast<float>(std::end(kZoomSteps)[-1]));
}

}  // namespace besedka::app
