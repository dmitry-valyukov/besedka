// История переходов: куда ходили, где стоим, куда можно вернуться и куда --
// вперёд. Как у браузера: новый переход обрывает всё, что было впереди.
//
// Никого не знает и ничего не показывает: последовательность маршрутов с
// указателем на текущий, и только. Сколько из неё видно на экране, решает
// :pages, а что при этом делается с экранами -- Navigator.

export module besedka.app:history;

import std;
import :route;

export namespace besedka::app {

class History {
public:
    bool empty() const noexcept { return entries_.empty(); }
    std::size_t size() const noexcept { return entries_.size(); }

    /// Где стоим: номер текущей записи. Только когда не пусто.
    std::size_t position() const noexcept;

    const Route& current() const;
    const Route& at(std::size_t index) const;

    bool canGoBack() const noexcept { return !empty() && position_ > 0; }
    bool canGoForward() const noexcept { return !empty() && position_ + 1 < entries_.size(); }

    /// Новый переход: становится текущим, а всё, что было впереди, забыто.
    void push(Route route);

    /// Только когда canGoBack().
    void back();

    /// Только когда canGoForward().
    void forward();

    /// Последние `count` записей до текущей включительно, слева направо, --
    /// то, что показывают одной или двумя страницами. Меньше, если столько не
    /// набирается; пусто, пока истории нет.
    std::span<const Route> tail(std::size_t count) const noexcept;

private:
    std::vector<Route> entries_;
    std::size_t position_ = 0;
};

}  // namespace besedka::app
