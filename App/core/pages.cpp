module;

#include "abi.h"

module besedka.app;

import std;
import wxl.core;

namespace besedka::app {

bool isWide(const double logicalWidth) noexcept { return logicalWidth > kTwoPageWidth; }

std::size_t pagesShown(const double logicalWidth) noexcept { return isWide(logicalWidth) ? 2 : 1; }

PagePlan planPages(const std::span<const Screen> inTree, const std::span<const Route> wanted) {
    ensure(wanted.size() <= 2 && "at most two pages are ever shown");

    PagePlan plan;

    // Первая показанная -- слева, вторая -- справа. В одностраничном показе
    // единственная страница -- левая: в ней выбирают одиночным щелчком, как
    // выбирали всегда.
    for (std::size_t at = 0; at < wanted.size(); ++at)
        plan.shown.push_back({screenOf(wanted[at]), at > 0});

    if (plan.shown.size() == 2)
        ensure(plan.shown[0].screen != plan.shown[1].screen &&
               "two adjacent history entries share a screen");

    const auto wantedHas = [&plan](const Screen screen) {
        return std::ranges::any_of(plan.shown,
                                   [screen](const PagePlan::Slot& slot) { return slot.screen == screen; });
    };

    for (const Screen screen : inTree)
        if (!wantedHas(screen)) plan.leaving.push_back(screen);

    for (const PagePlan::Slot& slot : plan.shown)
        if (std::ranges::find(inTree, slot.screen) == inTree.end()) plan.entering.push_back(slot.screen);

    return plan;
}

double clampedSplit(const double fraction) noexcept {
    return fraction < kSplitLower ? kSplitLower : (fraction > kSplitUpper ? kSplitUpper : fraction);
}

}  // namespace besedka::app
