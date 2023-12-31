// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/open_with_browser.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom-shared.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_ui.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/drive_upload_handler.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/one_drive_upload_handler.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "extensions/browser/entry_info.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/gfx/geometry/size.h"

namespace ash::cloud_upload {

typedef base::OnceCallback<void(
    const std::vector<extensions::EntryInfo>& entries)>
    EntriesCallback;

namespace {

using ash::file_system_provider::ProvidedFileSystemInfo;
using ash::file_system_provider::ProviderId;
using ash::file_system_provider::Service;
using file_manager::file_tasks::kDriveTaskResultMetricName;
using file_manager::file_tasks::OfficeTaskResult;

const char kAndroidOneDriveAuthority[] =
    "com.microsoft.skydrive.content.StorageAccessProvider";

std::vector<ProvidedFileSystemInfo> GetODFSFileSystems(Profile* profile) {
  Service* service = Service::Get(profile);
  ProviderId provider_id = ProviderId::CreateFromExtensionId(
      file_manager::file_tasks::kODFSExtensionId);
  return service->GetProvidedFileSystemInfoList(provider_id);
}

// Open a hosted MS Office file e.g. .docx, from a url hosted in
// DriveFS. Check the file was successfully uploaded to DriveFS.
void OpenUploadedDriveUrl(const GURL& url) {
  if (url.is_empty()) {
    UMA_HISTOGRAM_ENUMERATION(kDriveTaskResultMetricName,
                              OfficeTaskResult::FAILED);
    return;
  }
  UMA_HISTOGRAM_ENUMERATION(kDriveTaskResultMetricName,
                            OfficeTaskResult::MOVED);
  file_manager::util::OpenNewTabForHostedOfficeFile(url);
}

// Open an already hosted MS Office file e.g. .docx, from a url hosted in
// DriveFS. Check there was no error retrieving the file's metadata.
void OpenAlreadyHostedDriveUrl(drive::FileError error,
                               drivefs::mojom::FileMetadataPtr metadata) {
  if (error != drive::FILE_ERROR_OK) {
    UMA_HISTOGRAM_ENUMERATION(
        file_manager::file_tasks::kDriveErrorMetricName,
        file_manager::file_tasks::OfficeDriveErrors::NO_METADATA);
    LOG(ERROR) << "Drive metadata error: " << error;
    return;
  }

  GURL hosted_url(metadata->alternate_url);
  bool opened = file_manager::util::OpenNewTabForHostedOfficeFile(hosted_url);

  if (opened) {
    UMA_HISTOGRAM_ENUMERATION(kDriveTaskResultMetricName,
                              OfficeTaskResult::OPENED);
  }
}

// Open file with |file_path| from ODFS |file_system|. Open in the OneDrive PWA
// without link captruing.
void OpenFileFromODFS(
    Profile* profile,
    file_system_provider::ProvidedFileSystemInterface* file_system,
    const base::FilePath& file_path) {
  file_system->GetActions(
      {file_path},
      base::BindOnce(
          [](base::WeakPtr<Profile> profile_weak_ptr,
             const file_system_provider::Actions& actions,
             base::File::Error result) {
            if (result != base::File::Error::FILE_OK) {
              return;
            }
            Profile* profile = profile_weak_ptr.get();
            if (!profile) {
              return;
            }
            for (const file_system_provider::Action& action : actions) {
              if (action.id == kOneDriveUrlActionId) {
                // Custom actions are used to pass a OneDrive URL as the "title"
                // attribute to be opened using an installed web app.
                GURL url(action.title);
                if (!url.is_valid()) {
                  return;
                }

                auto* proxy =
                    apps::AppServiceProxyFactory::GetForProfile(profile);
                proxy->LaunchAppWithUrl(web_app::kMicrosoftOfficeAppId,
                                        /*event_flags=*/ui::EF_NONE, url,
                                        apps::LaunchSource::kFromFileManager,
                                        /*window_info=*/nullptr);
                return;
              }
            }
          },
          profile->GetWeakPtr()));
}

// Open office file using the ODFS |url|.
void OpenODFSUrl(Profile* profile, const storage::FileSystemURL& url) {
  if (!url.is_valid()) {
    LOG(ERROR) << "Invalid uploaded file URL";
    return;
  }
  ash::file_system_provider::util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    LOG(ERROR) << "Path not in FSP";
    return;
  }
  OpenFileFromODFS(profile, parser.file_system(), parser.file_path());
}

void OpenODFSUrls(Profile* profile,
                  const std::vector<storage::FileSystemURL>& file_urls) {
  for (const auto& file_url : file_urls) {
    OpenODFSUrl(profile, file_url);
  }
}

// Open office files from ODFS that were originally selected from Android
// OneDrive. First convert the |android_onedrive_urls| to ODFS file paths, then
// open them from ODFS in the MS 365 PWA.
void OpenAndroidOneDriveUrls(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& android_onedrive_urls) {
  for (const auto& android_onedrive_url : android_onedrive_urls) {
    absl::optional<ODFSFileSystemAndPath> fs_and_path =
        AndroidOneDriveUrlToODFS(profile, android_onedrive_url);
    if (!fs_and_path.has_value()) {
      // TODO(b/269364287): Handle when Android OneDrive file can't be opened.
      LOG(ERROR) << "Android OneDrive Url cannot be converted to ODFS";
      return;
    }
    OpenFileFromODFS(profile, fs_and_path->file_system,
                     fs_and_path->file_path_within_odfs);
  }
}

void OpenAlreadyHostedDriveUrls(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_urls) {
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  base::FilePath relative_path;
  for (const auto& file_url : file_urls) {
    if (integration_service->GetRelativeDrivePath(file_url.path(),
                                                  &relative_path)) {
      integration_service->GetDriveFsInterface()->GetMetadata(
          relative_path, base::BindOnce(&OpenAlreadyHostedDriveUrl));
    } else {
      LOG(ERROR) << "Unexpected error obtaining the relative path ";
    }
  }
}

void StartUpload(Profile* profile,
                 const std::vector<storage::FileSystemURL>& file_urls,
                 const CloudProvider cloud_provider) {
  if (cloud_provider == CloudProvider::kGoogleDrive) {
    for (const auto& file_url : file_urls) {
      DriveUploadHandler::Upload(profile, file_url,
                                 base::BindOnce(&OpenUploadedDriveUrl));
    }
  } else if (cloud_provider == CloudProvider::kOneDrive) {
    for (const auto& file_url : file_urls) {
      OneDriveUploadHandler::Upload(
          profile, file_url,
          base::BindOnce(
              [](base::WeakPtr<Profile> profile_weak_ptr,
                 const storage::FileSystemURL& url) {
                Profile* profile = profile_weak_ptr.get();
                if (!profile) {
                  return;
                }
                OpenODFSUrl(profile, url);
              },
              profile->GetWeakPtr()));
    }
  }
}

void ConfirmMoveOrStartUpload(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_urls,
    const CloudProvider cloud_provider) {
  if (file_manager::file_tasks::AlwaysMoveOfficeFiles(profile)) {
    return StartUpload(profile, file_urls, cloud_provider);
  }

  if (cloud_provider == CloudProvider::kGoogleDrive) {
    CloudUploadDialog::SetUpAndShowDialog(
        profile, file_urls, mojom::DialogPage::kMoveConfirmationGoogleDrive);
  } else if (cloud_provider == CloudProvider::kOneDrive) {
    CloudUploadDialog::SetUpAndShowDialog(
        profile, file_urls, mojom::DialogPage::kMoveConfirmationOneDrive);
  }
}

bool HasWordFile(const std::vector<storage::FileSystemURL>& file_urls) {
  constexpr const char* kWordExtensions[] = {".doc", ".docx"};
  for (auto& url : file_urls) {
    for (const char* extension : kWordExtensions) {
      if (url.path().MatchesExtension(extension)) {
        return true;
      }
    }
  }
  return false;
}

bool HasExcelFile(const std::vector<storage::FileSystemURL>& file_urls) {
  constexpr const char* kExcelExtensions[] = {".xls", ".xlsx"};
  for (auto& url : file_urls) {
    for (const char* extension : kExcelExtensions) {
      if (url.path().MatchesExtension(extension)) {
        return true;
      }
    }
  }
  return false;
}

bool HasPowerPointFile(const std::vector<storage::FileSystemURL>& file_urls) {
  constexpr const char* kPowerpointExtensions[] = {".ppt", ".pptx"};
  for (auto& url : file_urls) {
    for (const char* extension : kPowerpointExtensions) {
      if (url.path().MatchesExtension(extension)) {
        return true;
      }
    }
  }
  return false;
}

// Launch the local file task in `tasks` with the position specified by
// `string_task_position`.
void LaunchLocalFileTask(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_urls,
    const std::string& string_task_position,
    std::vector<::file_manager::file_tasks::TaskDescriptor> tasks) {
  // Convert the `string_task_position` - the string of the task position in
  // `tasks_` - to an int. Ensure that it is within the range of `tasks`.
  int task_position;
  if (!base::StringToInt(string_task_position, &task_position) ||
      task_position < 0 || static_cast<size_t>(task_position) >= tasks.size()) {
    LOG(ERROR) << "Position for local file task is unexpectedly unable to be "
                  "retrieved. Retrieved position: "
               << string_task_position
               << " from user response: " << string_task_position;
    return;
  }
  // Launch the task.
  file_manager::file_tasks::TaskDescriptor& task = tasks[task_position];
  file_manager::file_tasks::ExecuteFileTask(
      profile, task, file_urls,
      base::BindOnce(
          [](Profile* profile,
             const std::vector<storage::FileSystemURL> file_urls,
             file_manager::file_tasks::TaskDescriptor task,
             extensions::api::file_manager_private::TaskResult result,
             std::string error_message) {
            if (!error_message.empty()) {
              LOG(ERROR) << "Execution of local file task with app id "
                         << task.app_id
                         << " to open office files. Led to error message: "
                         << error_message << " and result: " << result;
            } else {
              if (HasWordFile(file_urls)) {
                SetWordFileHandler(profile, task);
              }
              if (HasExcelFile(file_urls)) {
                SetExcelFileHandler(profile, task);
              }
              if (HasPowerPointFile(file_urls)) {
                SetPowerPointFileHandler(profile, task);
              }
              file_manager::file_tasks::SetOfficeSetupComplete(profile);
            }
          },
          profile, file_urls, task));
}
}  // namespace

bool IsEligibleAndEnabledUploadOfficeToCloud() {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user_manager) {
    return false;
  }

  user_manager::User* user = user_manager->GetActiveUser();
  if (!user) {
    return false;
  }

  // |profile_manager| can be null in unit tests, even though a user was
  // created. If it is null, `GetBrowserContextByUser` call will cause crash.
  auto* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager) {
    return false;
  }

  Profile* profile = Profile::FromBrowserContext(
      BrowserContextHelper::Get()->GetBrowserContextByUser(user));
  if (!profile) {
    return false;
  }

  // Managed users, e.g. enterprise account, child account, are not eligible
  // with the exception of Google employees. `GetUserCloudPolicyManagerAsh`
  // returns non-nullptr if a profile is a managed account. This approach is
  // taken in `UserTypeByDeviceTypeMetricsProvider::GetUserSegment`.
  if (profile->GetUserCloudPolicyManagerAsh() &&
      !gaia::IsGoogleInternalAccountEmail(
          user->GetAccountId().GetUserEmail())) {
    return false;
  }

  return features::IsUploadOfficeToCloudEnabled();
}

void OnDialogComplete(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_urls,
    const std::string& user_response,
    std::vector<::file_manager::file_tasks::TaskDescriptor> tasks) {
  using file_manager::file_tasks::SetExcelFileHandlerToFilesSWA;
  using file_manager::file_tasks::SetOfficeSetupComplete;
  using file_manager::file_tasks::SetPowerPointFileHandlerToFilesSWA;
  using file_manager::file_tasks::SetWordFileHandlerToFilesSWA;

  if (user_response == kUserActionConfirmOrUploadToGoogleDrive) {
    SetWordFileHandlerToFilesSWA(
        profile, file_manager::file_tasks::kActionIdWebDriveOfficeWord);
    SetExcelFileHandlerToFilesSWA(
        profile, file_manager::file_tasks::kActionIdWebDriveOfficeExcel);
    SetPowerPointFileHandlerToFilesSWA(
        profile, file_manager::file_tasks::kActionIdWebDriveOfficePowerPoint);
    SetOfficeSetupComplete(profile);
    OpenOrMoveFiles(profile, file_urls, CloudProvider::kGoogleDrive);
  } else if (user_response == kUserActionConfirmOrUploadToOneDrive) {
    // Default handlers have already been set by this point for
    // Office/OneDrive.
    OpenOrMoveFiles(profile, file_urls, CloudProvider::kOneDrive);
  } else if (user_response == kUserActionUploadToGoogleDrive) {
    StartUpload(profile, file_urls, CloudProvider::kGoogleDrive);
  } else if (user_response == kUserActionUploadToOneDrive) {
    StartUpload(profile, file_urls, CloudProvider::kOneDrive);
  } else if (user_response == kUserActionSetUpGoogleDrive) {
    CloudUploadDialog::SetUpAndShowDialog(profile, file_urls,
                                          mojom::DialogPage::kGoogleDriveSetup);
  } else if (user_response == kUserActionSetUpOneDrive) {
    CloudUploadDialog::SetUpAndShowDialog(profile, file_urls,
                                          mojom::DialogPage::kOneDriveSetup);
  } else if (user_response == kUserActionCancel) {
    UMA_HISTOGRAM_ENUMERATION(kDriveTaskResultMetricName,
                              OfficeTaskResult::CANCELLED);
  } else {
    LaunchLocalFileTask(profile, file_urls, user_response, std::move(tasks));
  }
}

bool OpenFilesWithCloudProvider(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_urls,
    const CloudProvider cloud_provider) {
  bool empty_selection = file_urls.empty();
  DCHECK(!empty_selection);
  if (empty_selection) {
    return false;
  }
  // Run the setup flow if it's never been completed.
  if (!file_manager::file_tasks::OfficeSetupComplete(profile)) {
    return CloudUploadDialog::SetUpAndShowDialog(
        profile, file_urls, mojom::DialogPage::kFileHandlerDialog);
  }

  if (ShouldFixUpOffice(profile, cloud_provider)) {
    // TODO(cassycc): Use page specifically for fix up.
    return CloudUploadDialog::SetUpAndShowDialog(
        profile, file_urls, mojom::DialogPage::kOneDriveSetup);
  }
  OpenOrMoveFiles(profile, file_urls, cloud_provider);
  return true;
}

void OpenOrMoveFiles(Profile* profile,
                     const std::vector<storage::FileSystemURL>& file_urls,
                     const CloudProvider cloud_provider) {
  if (cloud_provider == CloudProvider::kGoogleDrive &&
      FileIsOnDriveFS(profile, file_urls.front().path())) {
    // The files are on Drive already.
    OpenAlreadyHostedDriveUrls(profile, file_urls);
  } else if (cloud_provider == CloudProvider::kOneDrive &&
             FileIsOnODFS(profile, file_urls.front())) {
    // The files are on OneDrive already, selected from ODFS.
    OpenODFSUrls(profile, file_urls);
  } else if (cloud_provider == CloudProvider::kOneDrive &&
             FileIsOnAndroidOneDrive(profile, file_urls.front())) {
    // The files are on OneDrive already, selected from Android OneDrive.
    OpenAndroidOneDriveUrlsIfAccountMatchedODFS(profile, file_urls);
  } else {
    // The files need to be moved.
    ConfirmMoveOrStartUpload(profile, file_urls, cloud_provider);
  }
}

bool ShouldFixUpOffice(Profile* profile, const CloudProvider cloud_provider) {
  return cloud_provider == CloudProvider::kOneDrive &&
         !(CloudUploadDialog::IsODFSMounted(profile) &&
           CloudUploadDialog::IsOfficeWebAppInstalled(profile));
}

bool FileIsOnDriveFS(Profile* profile, const base::FilePath& file_path) {
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  base::FilePath relative_path;
  return integration_service->GetRelativeDrivePath(file_path, &relative_path);
}

bool FileIsOnODFS(Profile* profile, const FileSystemURL& url) {
  ash::file_system_provider::util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    return false;
  }

  auto provider_id = file_system_provider::ProviderId::CreateFromExtensionId(
      file_manager::file_tasks::kODFSExtensionId);
  return parser.file_system()->GetFileSystemInfo().provider_id() == provider_id;
}

bool FileIsOnAndroidOneDrive(Profile* profile, const FileSystemURL& url) {
  std::string authority;
  std::string root_document_id;
  base::FilePath path;
  return arc::ParseDocumentsProviderUrl(url, &authority, &root_document_id,
                                        &path) &&
         authority == kAndroidOneDriveAuthority;
}

absl::optional<std::string> GetEmailFromAndroidOneDriveRootDoc(
    const std::string& root_document_id) {
  // After escaping the '/', the Root Document Id is:
  // pivots%2F<user-microsoft-account-email>.
  // Convert back to:
  // pivots/<user-microsoft-account-email>
  std::string root_document_id_unescaped = base::UnescapeURLComponent(
      root_document_id, base::UnescapeRule::PATH_SEPARATORS);
  std::vector<base::FilePath::StringType> components =
      base::FilePath(root_document_id_unescaped).GetComponents();
  if (components.size() != 2) {
    LOG(ERROR) << "Android OneDrive documents provider root document id is not "
                  "as expected.";
    return absl::nullopt;
  }
  if (components[0] != "pivots") {
    LOG(ERROR) << "Android OneDrive documents provider root document id is not "
                  "as expected.";
    return absl::nullopt;
  }
  return components[1];
}

void OpenAndroidOneDriveUrlsIfAccountMatchedODFS(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& android_onedrive_urls) {
  // Get email account associated with Android OneDrive.
  std::string authority;
  std::string root_document_id;
  base::FilePath path;
  if (!arc::ParseDocumentsProviderUrl(android_onedrive_urls.front(), &authority,
                                      &root_document_id, &path)) {
    return;
  }

  absl::optional<std::string> android_onedrive_email =
      GetEmailFromAndroidOneDriveRootDoc(root_document_id);
  if (!android_onedrive_email.has_value()) {
    return;
  }

  // Get email account associated with ODFS.
  absl::optional<ODFSFileSystemAndPath> fs_and_path =
      AndroidOneDriveUrlToODFS(profile, android_onedrive_urls.front());
  if (!fs_and_path.has_value()) {
    // TODO(b/269364287): Handle when Android OneDrive file can't be opened.
    LOG(ERROR) << "Android OneDrive Url cannot be converted to ODFS";
    return;
  }
  fs_and_path->file_system->GetActions(
      {fs_and_path->file_path_within_odfs},
      base::BindOnce(
          [](base::WeakPtr<Profile> profile_weak_ptr,
             std::string android_onedrive_email,
             const std::vector<storage::FileSystemURL>& android_onedrive_urls,
             const file_system_provider::Actions& actions,
             base::File::Error result) {
            if (result != base::File::Error::FILE_OK) {
              return;
            }
            Profile* profile = profile_weak_ptr.get();
            if (!profile) {
              return;
            }
            // Query whether the account logged into Android OneDrive is the
            // same as ODFS.
            for (const file_system_provider::Action& action : actions) {
              if (action.id == kUserEmailActionId) {
                if (android_onedrive_email == action.title) {
                  OpenAndroidOneDriveUrls(profile, android_onedrive_urls);
                } else {
                  LOG(ERROR) << "Email accounts associated with ODFS and "
                                "Android OneDrive don't match.";
                }
                return;
              }
            }
          },
          profile->GetWeakPtr(), std::move(android_onedrive_email.value()),
          android_onedrive_urls));
}

absl::optional<ODFSFileSystemAndPath> AndroidOneDriveUrlToODFS(
    Profile* profile,
    const FileSystemURL& android_onedrive_file_url) {
  if (!FileIsOnAndroidOneDrive(profile, android_onedrive_file_url)) {
    LOG(ERROR) << "File not on Android OneDrive";
    return absl::nullopt;
  }

  // Get the ODFS mount path.
  std::vector<ProvidedFileSystemInfo> odfs_file_system_infos =
      GetODFSFileSystems(profile);
  if (odfs_file_system_infos.size() != 1u) {
    LOG(ERROR) << "One and only one filesystem should be mounted for the ODFS "
                  "extension";
    return absl::nullopt;
  }
  base::FilePath odfs_path = odfs_file_system_infos[0].mount_path();

  // Find the relative path from Android OneDrive Url.
  std::string authority;
  std::string root_document_id;
  base::FilePath path;
  if (!arc::ParseDocumentsProviderUrl(android_onedrive_file_url, &authority,
                                      &root_document_id, &path)) {
    return absl::nullopt;
  }
  // Format for Android OneDrive documents provider `path` is:
  // Files/<rel_path>
  std::vector<base::FilePath::StringType> components =
      base::FilePath(path.value()).GetComponents();
  if (components.size() < 2) {
    LOG(ERROR)
        << "Android OneDrive documents provider path is not as expected.";
    return absl::nullopt;
  }
  if (components[0] != "Files") {
    LOG(ERROR)
        << "Android OneDrive documents provider path is not as expected.";
    return absl::nullopt;
  }
  // Append relative path from Android OneDrive Url.
  for (size_t i = 1; i < components.size(); i++) {
    odfs_path = odfs_path.Append(components[i]);
  }

  ash::file_system_provider::util::LocalPathParser parser(profile, odfs_path);
  if (!parser.Parse()) {
    LOG(ERROR) << "Path not in FSP";
    return absl::nullopt;
  }
  return ODFSFileSystemAndPath{parser.file_system(), parser.file_path()};
}

void GetEntriesFromFilePathsAndMimeTypes(
    const std::vector<base::FilePath>& file_paths,
    EntriesCallback entries_callback,
    std::unique_ptr<extensions::app_file_handler_util::MimeTypeCollector>
        mime_collector,
    std::unique_ptr<std::vector<std::string>> mime_types) {
  std::vector<extensions::EntryInfo> entries;
  DCHECK_EQ(file_paths.size(), mime_types->size());
  for (size_t i = 0; i < file_paths.size(); ++i) {
    entries.emplace_back(file_paths[i], (*mime_types)[i], false);
  }
  std::move(entries_callback).Run(entries);
}

// Find the file tasks that can open the `file_urls` and pass them to the
// `find_all_types_of_tasks_callback`.
void FindTasksForDialog(Profile* profile,
                        const std::vector<storage::FileSystemURL>& file_urls,
                        file_manager::file_tasks::FindTasksCallback
                            find_all_types_of_tasks_callback) {
  // Get the file info for finding the tasks.
  std::vector<base::FilePath> local_paths;
  std::vector<GURL> gurls;
  for (const auto& file_url : file_urls) {
    local_paths.push_back(file_url.path());
    gurls.push_back(file_url.ToGURL());
  }

  // Callback to find the tasks after the file entries have been collected.
  EntriesCallback entries_callback = base::BindOnce(
      [](Profile* profile, const std::vector<GURL>& gurls,
         file_manager::file_tasks::FindTasksCallback
             find_all_types_of_tasks_callback,
         const std::vector<extensions::EntryInfo>& entries) {
        // TODO(cassycc): Handle dlp_source_urls appropriately.
        const std::vector<std::string> dlp_source_urls(entries.size(), "");
        FindAllTypesOfTasks(profile, entries, gurls, dlp_source_urls,
                            std::move(find_all_types_of_tasks_callback));
      },
      profile, gurls, std::move(find_all_types_of_tasks_callback));

  // Get the mime types of the files and then pass them to the callback to
  // get the entries.
  std::unique_ptr<extensions::app_file_handler_util::MimeTypeCollector>
      mime_collector = std::make_unique<
          extensions::app_file_handler_util::MimeTypeCollector>(profile);
  mime_collector.get()->CollectForLocalPaths(
      local_paths,
      base::BindOnce(&GetEntriesFromFilePathsAndMimeTypes, local_paths,
                     std::move(entries_callback), std::move(mime_collector)));
}

// static
void CloudUploadDialog::ShowDialog(
    mojom::DialogArgsPtr args,
    const mojom::DialogPage dialog_page,
    UploadRequestCallback upload_callback,
    std::unique_ptr<file_manager::file_tasks::ResultingTasks> resulting_tasks) {
  std::vector<file_manager::file_tasks::TaskDescriptor> tasks;
  if (resulting_tasks) {
    for (int i = 0; static_cast<size_t>(i) < resulting_tasks->tasks.size();
         i++) {
      auto task = resulting_tasks->tasks[i];
      // Ignore Google Docs and MS Office tasks as they are already
      // set up to show in the dialog. And ignore QuickOffice.
      if (IsWebDriveOfficeTask(task.task_descriptor) ||
          file_manager::file_tasks::IsOpenInOfficeTask(task.task_descriptor) ||
          extension_misc::IsQuickOfficeExtension(task.task_descriptor.app_id)) {
        continue;
      }
      mojom::DialogTaskPtr dialog_task = mojom::DialogTask::New();
      // The (unique and positive) `position` of the task in the `tasks` vector.
      // If the user responds with the `position`, the task will be launched via
      // `LaunchLocalFileTask()`.
      dialog_task->position = i;
      dialog_task->title = task.task_title;
      dialog_task->icon_url = task.icon_url.spec();
      dialog_task->app_id = task.task_descriptor.app_id;

      args->tasks.push_back(std::move(dialog_task));
      tasks.push_back(std::move(task.task_descriptor));
    }
  }
  CloudUploadDialog* dialog =
      new CloudUploadDialog(std::move(args), std::move(upload_callback),
                            dialog_page, std::move(tasks));

  dialog->ShowSystemDialog();
}

// static
bool CloudUploadDialog::SetUpAndShowDialog(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_urls,
    const mojom::DialogPage dialog_page) {
  // Allow no more than one upload dialog at a time. In the case of multiple
  // upload requests, they should either be handled simultaneously or queued.
  if (SystemWebDialogDelegate::HasInstance(
          GURL(chrome::kChromeUICloudUploadURL))) {
    return false;
  }

  mojom::DialogArgsPtr args = mojom::DialogArgs::New();
  for (const auto& file_url : file_urls) {
    args->file_names.push_back(file_url.path().BaseName().value());
  }
  args->dialog_page = dialog_page;
  args->first_time_setup =
      !file_manager::file_tasks::OfficeSetupComplete(profile);

  // The pointer is managed by an instance of `views::WebDialogView` and
  // removed in `SystemWebDialogDelegate::OnDialogClosed`.
  UploadRequestCallback upload_callback =
      base::BindOnce(&OnDialogComplete, profile, file_urls);

  // Display local file handlers (tasks) only for the file handler dialog.
  if (dialog_page == mojom::DialogPage::kFileHandlerDialog) {
    // Callback to show the dialog after the tasks have been found.
    file_manager::file_tasks::FindTasksCallback
        find_all_types_of_tasks_callback =
            base::BindOnce(&ShowDialog, std::move(args), dialog_page,
                           std::move(upload_callback));
    // Find the file tasks that can open the `file_urls` and then run
    // `ShowDialog`.
    FindTasksForDialog(profile, file_urls,
                       std::move(find_all_types_of_tasks_callback));

  } else {
    ShowDialog(std::move(args), dialog_page, std::move(upload_callback),
               nullptr);
  }
  return true;
}

bool CloudUploadDialog::IsODFSMounted(Profile* profile) {
  // Assume any file system mounted by ODFS is the correct one.
  return !GetODFSFileSystems(profile).empty();
}

bool CloudUploadDialog::IsOfficeWebAppInstalled(Profile* profile) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return false;
  }
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  bool installed = false;
  proxy->AppRegistryCache().ForOneApp(
      web_app::kMicrosoftOfficeAppId,
      [&installed](const apps::AppUpdate& update) {
        installed = apps_util::IsInstalled(update.Readiness());
      });
  return installed;
}

void CloudUploadDialog::OnDialogShown(content::WebUI* webui) {
  DCHECK(dialog_args_);
  SystemWebDialogDelegate::OnDialogShown(webui);
  static_cast<CloudUploadUI*>(webui->GetController())
      ->SetDialogArgs(std::move(dialog_args_));
}

void CloudUploadDialog::OnDialogClosed(const std::string& json_retval) {
  UploadRequestCallback callback = std::move(callback_);
  std::vector<file_manager::file_tasks::TaskDescriptor> tasks =
      std::move(tasks_);
  // Deletes this, so we store the `callback` and `tasks` first.
  SystemWebDialogDelegate::OnDialogClosed(json_retval);
  // The callback can create a new dialog. It must be called last because we
  // can only have one of these dialogs at a time.
  if (callback) {
    std::move(callback).Run(json_retval, std::move(tasks));
  }
}

CloudUploadDialog::CloudUploadDialog(
    mojom::DialogArgsPtr args,
    UploadRequestCallback callback,
    const mojom::DialogPage dialog_page,
    std::vector<file_manager::file_tasks::TaskDescriptor> tasks)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUICloudUploadURL),
                              std::u16string() /* title */),
      dialog_args_(std::move(args)),
      callback_(std::move(callback)),
      dialog_page_(dialog_page),
      tasks_(std::move(tasks)) {}

CloudUploadDialog::~CloudUploadDialog() = default;

bool CloudUploadDialog::ShouldCloseDialogOnEscape() const {
  // The One Drive setup dialog handles escape in the webui as it needs to
  // display a confirmation dialog on cancellation.
  return dialog_page_ != mojom::DialogPage::kOneDriveSetup;
}

bool CloudUploadDialog::ShouldShowCloseButton() const {
  return false;
}

namespace {
const int kDialogWidthForOneDriveSetup = 512;
const int kDialogHeightForOneDriveSetup = 552;

const int kDialogWidthForFileHandlerDialog = 512;
const int kDialogHeightForFileHandlerDialog = 475;
const int kDialogHeightForFileHandlerDialogNoLocalApp = 411;

const int kDialogWidthForDriveSetup = 512;
const int kDialogHeightForDriveSetup = 220;

const int kDialogWidthForMoveConfirmation = 448;
const int kDialogHeightForMoveConfirmation = 228;
}  // namespace

void CloudUploadDialog::GetDialogSize(gfx::Size* size) const {
  switch (dialog_page_) {
    // TODO(cassycc): resize dialog based on number of local file tasks.
    case mojom::DialogPage::kFileHandlerDialog: {
      size->set_width(kDialogWidthForFileHandlerDialog);
      size->set_height(tasks_.size() == 0
                           ? kDialogHeightForFileHandlerDialogNoLocalApp
                           : kDialogHeightForFileHandlerDialog);
      return;
    }
    case mojom::DialogPage::kOneDriveSetup: {
      size->set_width(kDialogWidthForOneDriveSetup);
      size->set_height(kDialogHeightForOneDriveSetup);
      return;
    }
    case mojom::DialogPage::kGoogleDriveSetup: {
      size->set_width(kDialogWidthForDriveSetup);
      size->set_height(kDialogHeightForDriveSetup);
      return;
    }
    case mojom::DialogPage::kMoveConfirmationGoogleDrive:
    case mojom::DialogPage::kMoveConfirmationOneDrive: {
      size->set_width(kDialogWidthForMoveConfirmation);
      size->set_height(kDialogHeightForMoveConfirmation);
      return;
    }
  }
}

}  // namespace ash::cloud_upload
