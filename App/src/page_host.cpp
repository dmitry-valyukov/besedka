// ensure() из wxl.core: макрос, и потому заголовком, а не импортом -- до своих
// заголовков, которые ведут к import.
#include "abi.h"

#include "page_host.h"
#include "palette.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace besedka::app {

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Граница -- всегда первый ребёнок, страницы идут за ней.
constexpr std::uint32_t kFirstPage = 1;

// Шесть логических -- полоска, которую видно и в которую попадают мышью, не
// отнимая заметной ширины ни у одной из страниц.
constexpr double kSplitterWidth = 6;

/// Колонка шириной в долю. Star, а не пиксели: доли складываются в единицу, и
/// сетка сама раздаёт им ширину, сколько бы её ни было.
ColumnDefinition starColumn(const double share) {
    ColumnDefinition column;

    column.width(GridLength{share, GridUnitType::Star});

    return column;
}

ColumnDefinition pixelColumn(const double width) {
    ColumnDefinition column;

    column.width(GridLength{width, GridUnitType::Pixel});

    return column;
}

}  // namespace

PageHost::PageHost() {
    splitter_ = Border{
        column = 1,
        background = palette.divider,
        visibility = Visibility::Collapsed,
        toolTip = L"Граница страниц: потяните, чтобы изменить ширину",
        // Стрелка-растяжка, пока указатель над полоской. Ставится раз: XAML
        // сам показывает курсор элемента, ловить вход и выход не нужно.
        cursor = InputSystemCursorShape::SizeWestEast,

        onPointerPressed =
            [this](Object const&, PointerRoutedEventArgs& args) {
                // Берётся у своего же поля, а не у отправителя: полоска --
                // часть хоста и живёт столько же, сколько он.
                if (!splitter_.value().capturePointer(args.pointer())) return;

                dragging_ = true;
            },

        onPointerMoved =
            [this](Object const&, PointerRoutedEventArgs& args) {
                if (!dragging_) return;

                const double width = root_.value().actualWidth();

                if (width <= 0) return;

                // Доля считается от указателя, а не складыванием сдвигов:
                // накопленная сумма разъезжается с рукой на каждом
                // подрезанном пределом движении.
                splitFraction(args.getCurrentPoint(root_.value()).position().x / width);
            },

        onPointerReleased =
            [this](Object const&, PointerRoutedEventArgs& args) {
                if (!dragging_) return;

                splitter_.value().releasePointerCapture(args.pointer());

                dragging_ = false;

                if (onSplitChanged) onSplitChanged(fraction_);
            },

        // Захват пропадает и сам: окно потеряло активацию, касание отменили.
        // Тяга, которая ждала бы только отпускания, осталась бы зажатой.
        onPointerCaptureLost =
            [this](Object const&, PointerRoutedEventArgs&) {
                if (!std::exchange(dragging_, false)) return;

                if (onSplitChanged) onSplitChanged(fraction_);
            },
    };

    root_ = Grid{
        row = 1,
        splitter_.value(),
    };

    root_.value().columnDefinitions().append(starColumn(1));
}

void PageHost::setScreen(const Screen screen, const UIElement& root) {
    screens_.emplace_back(screen, root);
}

const UIElement& PageHost::elementOf(const Screen screen) const {
    const auto found = std::ranges::find(screens_, screen, &std::pair<Screen, UIElement>::first);

    ensure(found != screens_.end() && "no screen registered for this route kind");

    return found->second;
}

void PageHost::setWide(const bool wide) {
    if (wide == wide_) return;

    wide_ = wide;

    const Collection<ColumnDefinition> columns = root_.value().columnDefinitions();

    columns.clear();

    if (wide_) {
        columns.append(starColumn(fraction_));
        columns.append(pixelColumn(kSplitterWidth));
        columns.append(starColumn(1 - fraction_));
    } else {
        columns.append(starColumn(1));
    }
}

void PageHost::apply(const PagePlan& plan) {
    const Collection<UIElement> children = root_.value().children();

    for (const Screen screen : plan.leaving) {
        const auto at = std::ranges::find(inTree_, screen);

        ensure(at != inTree_.end() && "a page leaves that was never in the tree");

        children.removeAt(kFirstPage + static_cast<std::uint32_t>(at - inTree_.begin()));
        inTree_.erase(at);
    }

    for (const Screen screen : plan.entering) {
        children.append(elementOf(screen));
        inTree_.push_back(screen);
    }

    // Первая показанная -- в нулевую колонку, вторая -- во вторую; между ними
    // колонка границы. Порядок в коллекции детей на место не влияет вовсе:
    // место ребёнку сетки задаёт Grid.Column, -- и это то, на чём держится
    // переезд страницы справа налево без снятия с дерева.
    for (const PagePlan::Slot& slot : plan.shown)
        Grid::setColumn(elementOf(slot.screen).try_as<FrameworkElement>(), slot.secondary ? 2 : 0);

    // Граница показывается только когда ей есть что делить: полоска между
    // страницей и пустотой -- шов, за которым ничего нет.
    splitter_.value().visibility(plan.shown.size() > 1 ? Visibility::Visible : Visibility::Collapsed);
}

void PageHost::splitFraction(const double value) {
    const double wanted = clampedSplit(value);

    if (wanted == fraction_) return;

    fraction_ = wanted;

    // Не пересобирать колонки целиком: при тяге это на каждое движение мыши.
    // Меняются две ширины, дети остаются на местах.
    const Collection<ColumnDefinition> columns = root_.value().columnDefinitions();

    if (columns.size() != 3) return;

    columns[0].width(GridLength{fraction_, GridUnitType::Star});
    columns[2].width(GridLength{1 - fraction_, GridUnitType::Star});
}

}  // namespace besedka::app
