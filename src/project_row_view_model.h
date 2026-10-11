#pragma once

#include "method_command.h"

#include <engine/ui/bindable.h>
#include <engine/ui/view_model.h>

#include <cstddef>
#include <string>

namespace launcher {

class LauncherApp;

// One project row (`projects` in assets/ui/launcher.xml). The buttons ask the launcher for the row's index; the
// launcher acts at the end of its frame system, after the rows may have been rebuilt.
class ProjectRowViewModel final : public engine::ui::ViewModel {
public:
    ProjectRowViewModel(LauncherApp& app, std::size_t index, bool openable);

    ProjectRowViewModel(const ProjectRowViewModel&) = delete;
    ProjectRowViewModel& operator=(const ProjectRowViewModel&) = delete;

    void open_project();
    void remove_project();
    [[nodiscard]] bool can_open_project() const;
    void install_sdk();
    [[nodiscard]] bool can_install_sdk() const;

    // Offers "Install <version>" in the place of Open: the project needs an SDK that is not installed and GitHub has it.
    // `enabled` is false while an install runs, and `label` then says "Installing…" for this version.
    void offer_install(std::string version, std::string label, bool enabled);

    engine::ui::Bindable<std::string> name;
    engine::ui::Bindable<std::string> path;
    // "Editor 0.1.0", "Needs 0.2.0", or what is wrong with wind_project.toml.
    engine::ui::Bindable<std::string> engineText;
    // engineText's color through `var-tone`. Empty: the stylesheet's default.
    engine::ui::Bindable<std::string> tone;
    // "block" or "none" through `var-display`: Open, or Install in its place.
    engine::ui::Bindable<std::string> openDisplay{std::string("block")};
    engine::ui::Bindable<std::string> installDisplay{std::string("none")};
    engine::ui::Bindable<std::string> installLabel;
    MethodCommand open;
    MethodCommand remove;
    MethodCommand install;

private:
    LauncherApp* app_;
    std::size_t index_;
    bool openable_;
    std::string install_version_;
    bool install_enabled_ = false;
};

}
