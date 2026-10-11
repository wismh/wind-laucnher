#pragma once

#include "method_command.h"

#include <engine/ui/bindable.h>
#include <engine/ui/view_model.h>

#include <cstddef>
#include <string>

namespace launcher {

class LauncherApp;

// One row of the Available list (`available` in assets/ui/launcher.xml): a version GitHub offers for this platform.
// Install and Cancel ask the launcher for the row's index; the launcher rebuilds the rows when an install starts or
// ends, and writes `state` of the row being installed every frame.
class AvailableSdkRowViewModel final : public engine::ui::ViewModel {
public:
    AvailableSdkRowViewModel(LauncherApp& app, std::size_t index, bool installable);

    AvailableSdkRowViewModel(const AvailableSdkRowViewModel&) = delete;
    AvailableSdkRowViewModel& operator=(const AvailableSdkRowViewModel&) = delete;

    void install_sdk();
    void cancel_install();
    [[nodiscard]] bool can_install_sdk() const;

    engine::ui::Bindable<std::string> version;
    // "alpha, 2026-10-12, 130.5 MB, newest".
    engine::ui::Bindable<std::string> detail;
    // "Installed", the progress of the install, why the last one failed, or empty.
    engine::ui::Bindable<std::string> state;
    // state's color through `var-tone`: red for a failure. Empty: the stylesheet's default.
    engine::ui::Bindable<std::string> tone;
    // "block" or "none" through `var-display`: Install until the SDK is installed or being installed, Cancel only while
    // this row installs.
    engine::ui::Bindable<std::string> installDisplay{std::string("block")};
    engine::ui::Bindable<std::string> cancelDisplay{std::string("none")};
    MethodCommand install;
    MethodCommand cancel;

private:
    LauncherApp* app_;
    std::size_t index_;
    bool installable_;
};

}
