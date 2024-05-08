// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncScheduler.h"

namespace unsync {

FScheduler GScheduler;

FScheduler::FScheduler()
: DownloadSempahore(MAX_DOWNLOAD_TASKS)
, UploadSempahore(MAX_UPLOAD_TASKS)
, FilesystemSemaphore(MAX_FILESYSTEM_TASKS)
{
}

FScheduler::~FScheduler()
{
}

}  // namespace unsync
