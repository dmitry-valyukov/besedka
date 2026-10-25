#pragma once
// Отложенная запись на диск настроек рабочего места.
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

/// Настройки -- у рабочего места, которое живёт дольше: писатель держит его
/// ссылкой и пишет его настройки в его файл.
class SettingsWriter {
public:
    SettingsWriter(const wxl::DispatcherQueue& queue, Workspace& workspace);

    SettingsWriter(const SettingsWriter&) = delete;
    SettingsWriter& operator=(const SettingsWriter&) = delete;

    Settings& settings() noexcept { return workspace_.settings; }

    /// Дописать перед записью то, что известно не настройкам, а окну, --
    /// его место. Спрашивается раз на запись, а не на каждое движение рамки.
    std::function<void(Settings&)> beforeSave;

    /// Что-то изменилось: записать, когда утихнет.
    void scheduleSave();

    /// Записать сейчас -- на закрытии, когда таймер уже не тикнет.
    void saveNow();

private:
    void write();

    Workspace& workspace_;
    wxl::DispatcherQueueTimer timer_;
};

}  // namespace besedka::app
