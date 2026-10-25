#include "settings_writer.h"

namespace besedka::app {

using namespace wxl;

namespace {

// Сколько должно постоять смирно, прежде чем писать: рука, отпустившая
// рамку, за это время её не схватит снова.
constexpr auto kQuiet = std::chrono::milliseconds(800);

}  // namespace

SettingsWriter::SettingsWriter(const DispatcherQueue& queue, Workspace& workspace)
    : workspace_(workspace), timer_(queue.createTimer()) {
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
    if (beforeSave) beforeSave(workspace_.settings);

    saveSettings(workspace_.settingsPath(), workspace_.settings);
}

}  // namespace besedka::app
