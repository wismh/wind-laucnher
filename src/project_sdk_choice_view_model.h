#pragma once

#include "method_command.h"

#include <engine/ui/bindable.h>
#include <engine/ui/view_model.h>

#include <cstddef>
#include <string>

namespace launcher {

class LauncherApp;

// One SDK in a project row's SDK menu (`sdkChoices` in assets/ui/launcher.xml). Picking it asks the launcher to write
// that version into the project's wind_project.toml; the launcher acts at the end of its frame system.
class ProjectSdkChoiceViewModel final : public engine::ui::ViewModel {
public:
    ProjectSdkChoiceViewModel(LauncherApp& app, std::size_t project_index, std::string sdk_version);

    ProjectSdkChoiceViewModel(const ProjectSdkChoiceViewModel&) = delete;
    ProjectSdkChoiceViewModel& operator=(const ProjectSdkChoiceViewModel&) = delete;

    void pick_sdk();

    // "0.1.1a", plus "located" for an SDK the user pointed at.
    engine::ui::Bindable<std::string> text;
    // The version the project has now.
    engine::ui::Bindable<bool> selected;
    MethodCommand pick;

private:
    LauncherApp* app_;
    std::size_t project_index_;
    std::string sdk_version_;
};

}
