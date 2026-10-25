module;

#include <windows.h>

#include <shlobj.h>

module besedka.app;

import std;
import wxl.core;
import wxl.fmt;
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
    return wxl::core::xml_escaped(wxl::core::unicode::repaired(value).to_utf8().chars());
}

}  // namespace

bool readSettings(std::string xml, Settings& into) {
    if (xml.empty()) return true;   // первого запуска ещё не было

    // Сперва всё читается в местные, и лишь потом ложится в поля: битый файл
    // не должен оставить настройки прочитанными наполовину.
    Settings settings;

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
        // Битый файл, файл от будущей версии, файл, который правили руками, --
        // всё это повод открыться со значениями по умолчанию, а не повод не
        // открыться. Сказать об этом читателю -- дело вызывающего.
        return false;
    }

    into = std::move(settings);
    return true;
}

std::string formatSettings(const Settings& settings) {
    // text_builder, а не поток: локали у него нет вовсе, и написанное не
    // зависит от того, что стоит в Windows. Аллокатор -- STA-пул: формирователь
    // зовёт корутина рабочего места (`Workspace::saveSettings`) до своего
    // `co_await`, то есть на потоке вызова -- у Беседки интерфейсном; на
    // рабочий поток уходят только байты.
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

    if (FAILED(::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &folder)))
        wxl::core::abort("dataDirectory: нет каталога LocalAppData");

    std::filesystem::path path{folder};

    ::CoTaskMemFree(folder);

    return path / L"Besedka";
}

}  // namespace besedka::app
