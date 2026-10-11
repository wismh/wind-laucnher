#pragma once

#include "available_sdk_row_view_model.h"
#include "method_command.h"
#include "project_row_view_model.h"
#include "sdk_option_view_model.h"
#include "sdk_row_view_model.h"

#include <engine/ui/bindable.h>
#include <engine/ui/view_model.h>

#include <memory>
#include <string>

namespace launcher {

// Fields of assets/ui/launcher.xml. Names are the XML binding paths, so they stay camelCase.
class LauncherViewModel final : public engine::ui::ViewModel {
public:
    LauncherViewModel();

    // The navbar: which page is shown. `*Tab` checks its button (New project keeps Projects checked); `*Display` is
    // the page's CSS `display` through `var-display` ("block" or "none").
    engine::ui::Bindable<bool> projectsTab{true};
    engine::ui::Bindable<bool> sdksTab;
    engine::ui::Bindable<std::string> projectsDisplay{std::string("block")};
    engine::ui::Bindable<std::string> sdksDisplay{std::string("none")};
    engine::ui::Bindable<std::string> newProjectDisplay{std::string("none")};
    // The Install SDK page, reached from SDKs (which keeps its tab checked).
    engine::ui::Bindable<std::string> availableDisplay{std::string("none")};

    engine::ui::BindableList<std::shared_ptr<ProjectRowViewModel>> projects;
    engine::ui::BindableList<std::shared_ptr<SdkRowViewModel>> sdks;
    // The SDKs GitHub offers for this platform, newest first (the Install SDK page), and a line about the list:
    // "Checking GitHub…", "offline: showing the list from 2026-10-10", or empty.
    engine::ui::BindableList<std::shared_ptr<AvailableSdkRowViewModel>> available;
    engine::ui::Bindable<std::string> availableNote;
    // Where SDKs are installed, or why there is no such directory.
    engine::ui::Bindable<std::string> sdkFolderText;
    engine::ui::Bindable<std::string> statusText;

    // The New project page. The two text fields are written by typing; the launcher reads them every frame the page
    // is shown.
    engine::ui::Bindable<std::string> newName;
    engine::ui::Bindable<std::string> newLocation;
    // The picked SDK on the picker button, and the picker's list.
    engine::ui::Bindable<std::string> newSdkText;
    engine::ui::Bindable<bool> sdkPickerOpen;
    engine::ui::BindableList<std::shared_ptr<SdkOptionViewModel>> sdkOptions;
    // Where the project will go, or why it cannot be created; `newHintTone` is red for the second.
    engine::ui::Bindable<std::string> newHint;
    engine::ui::Bindable<std::string> newHintTone;

    MethodCommand showProjects;
    MethodCommand showSdks;
    MethodCommand installSdk;
    MethodCommand closeInstallSdk;
    MethodCommand addProject;
    MethodCommand newProject;
    MethodCommand locateSdk;
    MethodCommand refreshAvailable;
    MethodCommand showSdkFolder;
    MethodCommand browseLocation;
    MethodCommand toggleSdkPicker;
    MethodCommand createProject;
    MethodCommand cancelNewProject;
};

}
