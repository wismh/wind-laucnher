#include "project_sdk_choice_view_model.h"

#include "launcher_app.h"

#include <asset_ids.h>

namespace launcher {

ProjectSdkChoiceViewModel::ProjectSdkChoiceViewModel(LauncherApp& app, std::size_t project_index, std::string sdk_version)
    : app_(&app)
    , project_index_(project_index)
    , sdk_version_(std::move(sdk_version)) {
    assets::ui::Launcher::Projects::SdkChoices::bind(*this);
    pick.bind_to<ProjectSdkChoiceViewModel, &ProjectSdkChoiceViewModel::pick_sdk>(*this);
}

void ProjectSdkChoiceViewModel::pick_sdk() {
    app_->request(LauncherRequest{
            .kind = LauncherRequest::Kind::ChangeProjectSdk, .index = project_index_, .version = sdk_version_});
}

}
