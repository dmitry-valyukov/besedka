#include "settings_writer.h"

#include <utility>

namespace besedka::app {

using namespace wxl;

namespace {

// Сколько должно постоять смирно, прежде чем писать: рука, отпустившая
// рамку, за это время её не схватит снова.
constexpr auto kQuiet = std::chrono::milliseconds(800);

}  // namespace

SettingsWriter::SettingsWriter(const DispatcherQueue& queue, Settings settings)
    : settings_(std::move(settings)), timer_(queue.createTimer()) {
    timer_.interval(kQuiet);
    timer_.isRepeating(false);

    timer_.add_onTick([this](Object const&, Object const&) {
        timer_.stop();
        write();
    });
}

void SettingsWriter::scheduleSave() {
    timer_.stop();
    timer_.start();
}

void SettingsWriter::saveNow() {
    timer_.stop();
    write();
}

void SettingsWriter::write() {
    if (beforeSave) beforeSave(settings_);

    saveSettings(settings_);
}

}  // namespace besedka::app
