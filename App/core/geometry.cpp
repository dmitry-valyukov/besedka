module besedka.app;

namespace besedka::app {

Extent fitToPicture(const Extent room, const Extent picture) noexcept {
    double height = room.height;
    double width = height * picture.width / picture.height;

    if (width > room.width) {
        width = room.width;
        height = width * picture.height / picture.width;
    }

    return {static_cast<int>(width), static_cast<int>(height)};
}

}  // namespace besedka::app
