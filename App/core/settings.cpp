module;

#include <windows.h>

#include <shlobj.h>

module besedka.app;

import std;
import wxl.core;
import wxl.xml;

namespace besedka::app {

namespace {

/// Значение атрибута так, как его можно положить в XML.
///
/// `repaired`, а не обещание: сюда уходит текст, пришедший от Windows, а он не
/// обязан быть правильным UTF-16 -- непарный суррогат в нём не запрещён.
/// Взятый на веру, он превратился бы в три байта, которых UTF-8 не знает, и
/// при следующем запуске wxl::xml отвергла бы файл целиком, то есть настройки
/// пропали бы из-за одной дурной единицы.
std::string xmlValue(const std::wstring_view value) {
    return wxl::core::xml_escaped(wxl::core::repaired(value).to_utf8().chars());
}

std::string readWhole(const std::filesystem::path& path) {
    wxl::core::file in = wxl::core::file::open_read(path.c_str());

    if (!in.opened()) return {};   // первого запуска ещё не было

    const wxl::core::nullable<std::uint64_t> size = in.size();

    // Настройки -- двести байт. Файл в мегабайт означает, что это не наш файл,
    // и разбирать его незачем.
    if (!size || *size > 64 * 1024) return {};

    std::string bytes(static_cast<std::size_t>(*size), '\0');

    const std::size_t got = in.read({reinterpret_cast<std::byte*>(bytes.data()), bytes.size()});

    bytes.resize(got);

    return bytes;
}

}  // namespace

Settings parseSettings(std::string xml) {
    Settings settings;

    if (xml.empty()) return settings;

    try {
        wxl::xml::document document;

        const wxl::xml::node& root = document.load(std::move(xml));

        if (const wxl::xml::node* window = root.child("window")) {
            if (const std::optional<wxl::core::u8_view> placement = window->attribute("placement"))
                settings.windowPlacement = std::wstring(placement->to_utf16().wchars());
        }

        if (const wxl::xml::node* layout = root.child("layout")) {
            if (const std::optional<wxl::core::u8_view> split = layout->attribute("split")) {
                // Разбор без локали: в файле точка, что бы ни стояло в
                // Windows. Непрочитанное число оставляет умолчание -- половину:
                // try_parse не трогает результат, пока не разберёт весь текст.
                wxl::core::try_parse(split->chars(), settings.splitFraction);
            }
        }

        if (const wxl::xml::node* view = root.child("view")) {
            if (const std::optional<wxl::core::u8_view> zoom = view->attribute("zoom"))
                wxl::core::try_parse(zoom->chars(), settings.zoom);
        }
    } catch (...) {
        return Settings{};
    }

    return settings;
}

std::string formatSettings(const Settings& settings) {
    // text_builder, а не поток: локали у него нет вовсе, и написанное не
    // зависит от того, что стоит в Windows. Настройки пишутся с потока
    // интерфейса (таймер его очереди), поэтому буфер -- из его пула.
    wxl::core::text_builder<wxl::core::sta_allocator> out;

    out.append("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    out.format("<settings version=\"{}\">\n", Settings::kVersion);
    out.format("  <window placement=\"{}\"/>\n", xmlValue(settings.windowPlacement));

    // Три знака после точки -- доля с точностью до пикселя на любом мониторе,
    // и без хвоста, который двоичная дробь тянет за собой.
    out.format("  <layout split=\"{:.3f}\"/>\n", settings.splitFraction);
    out.format("  <view zoom=\"{:.3f}\"/>\n", settings.zoom);

    out.append("</settings>\n");

    return std::string(out.view());
}

std::filesystem::path dataDirectory() {
    PWSTR folder = nullptr;

    if (FAILED(::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &folder))) return {};

    std::filesystem::path path{folder};

    ::CoTaskMemFree(folder);

    return path / L"Besedka";
}

std::filesystem::path settingsPath() {
    const std::filesystem::path directory = dataDirectory();

    return directory.empty() ? std::filesystem::path{} : directory / L"settings.xml";
}

Settings loadSettings() {
    const std::filesystem::path path = settingsPath();

    if (path.empty()) return Settings{};

    return parseSettings(readWhole(path));
}

void saveSettings(const Settings& settings) {
    const std::filesystem::path path = settingsPath();

    if (path.empty()) return;

    const std::string content = formatSettings(settings);

    std::error_code failed;

    std::filesystem::create_directories(path.parent_path(), failed);

    const std::filesystem::path temporary = std::filesystem::path(path).concat(L".tmp");

    {
        wxl::core::file file = wxl::core::file::create(temporary.c_str());

        if (!file.opened()) return;

        const std::size_t written =
            file.write({reinterpret_cast<const std::byte*>(content.data()), content.size()});

        // Сброс на носитель до переименования: иначе выключение питания
        // оставило бы имя новым, а содержимое старым или нулевым.
        if (written != content.size() || !file.flush()) return;
    }

    // WRITE_THROUGH -- чтобы и сама замена дошла до диска.
    ::MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

}  // namespace besedka::app
