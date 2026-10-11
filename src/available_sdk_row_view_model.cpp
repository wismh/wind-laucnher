#include "available_sdk_row_view_model.h"

#include "launcher_app.h"

#include <asset_ids.h>

namespace launcher {

AvailableSdkRowViewModel::AvailableSdkRowViewModel(LauncherApp& app, std::size_t index, bool installable)
    : app_(&app)
    , index_(index)
    , installable_(installable) {
    assets::ui::Launcher::Available::bind(*this);
    install.bind_to<AvailableSdkRowViewModel, &AvailableSdkRowViewModel::install_sdk,
            &AvailableSdkRowViewModel::can_install_sdk>(*this);
    cancel.bind_to<AvailableSdkRowViewModel, &AvailableSdkRowViewModel::cancel_install>(*this);
}

void AvailableSdkRowViewModel::install_sdk() {
    app_->request(LauncherRequest{.kind = LauncherRequest::Kind::InstallSdk, .index = index_});
}

void AvailableSdkRowViewModel::cancel_install() {
    app_->request(LauncherRequest{.kind = LauncherRequest::Kind::CancelInstall});
}

bool AvailableSdkRowViewModel::can_install_sdk() const {
    return installable_;
}

}
