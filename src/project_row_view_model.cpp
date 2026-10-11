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
    install.bind_to<ProjectRowViewModel, &ProjectRowViewModel::install_sdk, &ProjectRowViewModel::can_install_sdk>(*this);
}

void ProjectRowViewModel::open_project() {
    app_->request(LauncherRequest{.kind = LauncherRequest::Kind::Open, .index = index_});
}

void ProjectRowViewModel::remove_project() {
    app_->request(LauncherRequest{.kind = LauncherRequest::Kind::Remove, .index = index_});
}

bool ProjectRowViewModel::can_open_project() const {
    return openable_;
}

void ProjectRowViewModel::install_sdk() {
    app_->request(LauncherRequest{.kind = LauncherRequest::Kind::InstallVersion, .version = install_version_});
}

bool ProjectRowViewModel::can_install_sdk() const {
    return install_enabled_;
}

void ProjectRowViewModel::offer_install(std::string version, std::string label, bool enabled) {
    install_version_ = std::move(version);
    install_enabled_ = enabled;
    installLabel = std::move(label);
    openDisplay = std::string("none");
    installDisplay = std::string("block");
}

}
