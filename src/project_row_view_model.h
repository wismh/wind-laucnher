#pragma once

#include "method_command.h"
#include "project_sdk_choice_view_model.h"

#include <engine/ui/bindable.h>
#include <engine/ui/view_model.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace launcher {

class LauncherApp;

// One project row (`projects` in assets/ui/launcher.xml). The name and path open the project, the SDK button opens the
// menu of SDKs to switch to, and ··· opens a menu with Open, Change SDK…, and Remove. The buttons ask the launcher for
// the row's index; the launcher acts at the end of its frame system, after the rows may have been rebuilt. Both menus
// are the row's own state.
class ProjectRowViewModel final : public engine::ui::ViewModel {
public:
    ProjectRowViewModel(LauncherApp& app, std::size_t index, bool openable);

    ProjectRowViewModel(const ProjectRowViewModel&) = delete;
    ProjectRowViewModel& operator=(const ProjectRowViewModel&) = delete;

    void open_project();
    void remove_project();
    [[nodiscard]] bool can_open_project() const;
    void toggle_menu();
    void toggle_sdk_menu();
    // The ··· menu's Change SDK…: closes it and opens the SDK menu.
    void change_sdk();
    void install_sdk();
    [[nodiscard]] bool can_install_sdk() const;

    // The SDKs the project can switch to.
    void set_sdk_choices(std::vector<std::shared_ptr<ProjectSdkChoiceViewModel>> choices);
    // Offers "Install <version>" first in the SDK menu: the project needs an SDK that is not installed and GitHub has
    // it. `enabled` is false while an install runs, and `label` then says "Installing…" for this version.
    void offer_install(std::string version, std::string label, bool enabled);

    engine::ui::Bindable<std::string> name;
    engine::ui::Bindable<std::string> path;
    // "SDK 0.1.0", "Needs SDK 0.2.0", or what is wrong with wind_project.toml.
    engine::ui::Bindable<std::string> engineText;
    // engineText's color through `var-tone`. Empty: the stylesheet's default.
    engine::ui::Bindable<std::string> tone;
    engine::ui::Bindable<bool> menuOpen;
    engine::ui::Bindable<bool> sdkMenuOpen;
    engine::ui::BindableList<std::shared_ptr<ProjectSdkChoiceViewModel>> sdkChoices;
    // "block" or "none" through `var-display`: the Install item of the SDK menu.
    engine::ui::Bindable<std::string> installDisplay{std::string("none")};
    engine::ui::Bindable<std::string> installLabel;
    MethodCommand open;
    MethodCommand remove;
    MethodCommand toggleMenu;
    MethodCommand toggleSdkMenu;
    MethodCommand changeSdk;
    MethodCommand install;

private:
    LauncherApp* app_;
    std::size_t index_;
    bool openable_;
    std::string install_version_;
    bool install_enabled_ = false;
};

}
