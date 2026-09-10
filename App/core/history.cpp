module;

// ensure(): нарушенное предусловие -- ошибка самой программы, и на неё
// падают, а не бросают.
#include "abi.h"

module besedka.app;

import std;
import wxl.core;

namespace besedka::app {

std::size_t History::position() const noexcept {
    ensure(!empty() && "position() of an empty history");

    return position_;
}

const Route& History::current() const {
    ensure(!empty() && "current() of an empty history");

    return entries_[position_];
}

const Route& History::at(const std::size_t index) const {
    ensure(index < entries_.size() && "history index out of range");

    return entries_[index];
}

void History::push(Route route) {
    // Хвост «вперёд» обрывается: после нового перехода вернуться вперёд
    // некуда, как и в браузере.
    if (!empty()) entries_.resize(position_ + 1);

    entries_.push_back(std::move(route));
    position_ = entries_.size() - 1;
}

void History::back() {
    ensure(canGoBack() && "back() with nowhere to go");

    --position_;
}

void History::forward() {
    ensure(canGoForward() && "forward() with nowhere to go");

    ++position_;
}

std::span<const Route> History::tail(const std::size_t count) const noexcept {
    if (empty()) return {};

    const std::size_t end = position_ + 1;
    const std::size_t begin = end > count ? end - count : 0;

    return std::span<const Route>(entries_).subspan(begin, end - begin);
}

}  // namespace besedka::app
