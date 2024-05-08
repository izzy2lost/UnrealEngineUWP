// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnsyncCommon.h"
#include "UnsyncThread.h"

namespace unsync {

class FScheduler
{
public:
	UNSYNC_DISALLOW_COPY_ASSIGN(FScheduler)

	static constexpr uint32 MAX_DOWNLOAD_TASKS	 = 8;
	static constexpr uint32 MAX_UPLOAD_TASKS	 = 2;
	static constexpr uint32 MAX_FILESYSTEM_TASKS = 64;

	FScheduler();
	~FScheduler();

	FSemaphore DownloadSempahore;
	FSemaphore UploadSempahore;
	FSemaphore FilesystemSemaphore;

private:
};

extern FScheduler GScheduler;

}  // namespace unsync
