#include "project_row_view_model.h"

#include "launcher_app.h"

#include <asset_ids.h>

namespace launcher {

ProjectRowViewModel::ProjectRowViewModel(LauncherApp& app, std::size_t index, bool openable)
    : app_(&app)
    , index_(index)
    , openable_(openable) {
    assets::ui::Launcher::Projects::bind(*this);
    open.bind_to<ProjectRowViewModel, &ProjectRowViewModel::open_project, &ProjectRowViewModel::can_open_project>(
            *this);
    remove.bind_to<ProjectRowViewModel, &ProjectRowViewModel::remove_project>(*this);
    toggleMenu.bind_to<ProjectRowViewModel, &ProjectRowViewModel::toggle_menu>(*this);
    toggleSdkMenu.bind_to<ProjectRowViewModel, &ProjectRowViewModel::toggle_sdk_menu>(*this);
    changeSdk.bind_to<ProjectRowViewModel, &ProjectRowViewModel::change_sdk>(*this);
    install.bind_to<ProjectRowViewModel, &ProjectRowViewModel::install_sdk, &ProjectRowViewModel::can_install_sdk>(*this);
}

void ProjectRowViewModel::open_project() {
    menuOpen = false;
    app_->request(LauncherRequest{.kind = LauncherRequest::Kind::Open, .index = index_});
}

void ProjectRowViewModel::remove_project() {
    menuOpen = false;
    app_->request(LauncherRequest{.kind = LauncherRequest::Kind::Remove, .index = index_});
}

bool ProjectRowViewModel::can_open_project() const {
    return openable_;
}

void ProjectRowViewModel::toggle_menu() {
    menuOpen = !menuOpen.get();
}

void ProjectRowViewModel::toggle_sdk_menu() {
    sdkMenuOpen = !sdkMenuOpen.get();
}

void ProjectRowViewModel::change_sdk() {
    menuOpen = false;
    sdkMenuOpen = true;
}

void ProjectRowViewModel::install_sdk() {
    sdkMenuOpen = false;
    app_->request(LauncherRequest{.kind = LauncherRequest::Kind::InstallVersion, .version = install_version_});
}

bool ProjectRowViewModel::can_install_sdk() const {
    return install_enabled_;
}

void ProjectRowViewModel::set_sdk_choices(std::vector<std::shared_ptr<ProjectSdkChoiceViewModel>> choices) {
    sdkChoices.set(std::move(choices));
}

void ProjectRowViewModel::offer_install(std::string version, std::string label, bool enabled) {
    install_version_ = std::move(version);
    install_enabled_ = enabled;
    installLabel = std::move(label);
    installDisplay = std::string("block");
}

}
