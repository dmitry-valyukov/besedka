#pragma once
// Настройки в памяти и их отложенная запись на диск.
//
// Тянуть рамку окна или границу страниц мышью -- это сотни событий в
// секунду, а файл переписывается целиком. Поэтому правки копятся, а пишется
// файл один раз, когда рука отпустила: таймером очереди интерфейса, который
// каждое новое событие отодвигает. Ни сна, ни потока -- очередь и так есть.

#include <chrono>
#include <functional>

#include "pch.h"

import besedka.app;

namespace besedka::app {

class SettingsWriter {
public:
    SettingsWriter(const wxl::DispatcherQueue& queue, Settings settings);

    SettingsWriter(const SettingsWriter&) = delete;
    SettingsWriter& operator=(const SettingsWriter&) = delete;

    Settings& settings() noexcept { return settings_; }

    /// Дописать перед записью то, что известно не настройкам, а окну, --
    /// его место. Спрашивается раз на запись, а не на каждое движение рамки.
    std::function<void(Settings&)> beforeSave;

    /// Что-то изменилось: записать, когда утихнет.
    void scheduleSave();

    /// Записать сейчас -- на закрытии, когда таймер уже не тикнет.
    void saveNow();

private:
    void write();

    Settings settings_;
    wxl::DispatcherQueueTimer timer_;
};

}  // namespace besedka::app
