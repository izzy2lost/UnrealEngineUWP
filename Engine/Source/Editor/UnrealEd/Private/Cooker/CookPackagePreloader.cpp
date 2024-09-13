// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cooker/CookPackagePreloader.h"

#include "CookOnTheSide/CookLog.h"
#include "CoreGlobals.h"
#include "EditorDomain/EditorDomain.h"
#include "Logging/LogMacros.h"
#include "Misc/AssertionMacros.h"
#include "Misc/PackagePath.h"
#include "Misc/PreloadableFile.h"
#include "Misc/StringBuilder.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectGlobals.h"

namespace UE::Cook
{

FPackagePreloader::~FPackagePreloader()
{
	PackageData.OnPackagePreloaderDestroyed(*this);
}

void FPackagePreloader::Shutdown()
{
	TRefCountPtr<FPackagePreloader> LocalRef(this);
	ClearPreload();
}

bool FPackagePreloader::TryPreload()
{
	if (bIsPreloadAttempted)
	{
		return true;
	}
	if (FindObjectFast<UPackage>(nullptr, PackageData.GetPackageName()))
	{
		if (AsyncRequest && !AsyncRequest->bHasFinished)
		{
			// In case of async loading, the object can be found while still being asynchronously serialized, we need
			// to wait until the callback is called and the async request is completely done.
			return false;
		}

		// If the package has already loaded, then there is no point in further preloading
		ClearPreload();
		bIsPreloadAttempted = true;
		return true;
	}
	if (PackageData.IsGenerated())
	{
		// Deferred populate generated packages are loaded from their generator, not from disk
		ClearPreload();
		bIsPreloadAttempted = true;
		return true;
	}
	if (IsAsyncLoadingMultithreaded())
	{
		if (!AsyncRequest.IsValid())
		{
			PackageData.GetPackageDatas().GetMonitor().OnPreloadAllocatedChanged(PackageData, true);
			AsyncRequest = MakeShared<FAsyncRequest>();
			AsyncRequest->RequestID = LoadPackageAsync(
				PackageData.GetFileName().ToString(),
				FLoadPackageAsyncDelegate::CreateLambda(
					[AsyncRequest = AsyncRequest](const FName&, UPackage*, EAsyncLoadingResult::Type)
					{
						AsyncRequest->bHasFinished = true;
					}
				),
				32 /* Use arbitrary higher priority for preload as we're going to need them very soon */
			);
		}

		// always return false so we continue to check the status of the load until FindObjectFast above finds the
		// loaded object
		return false;
	}
	if (!PreloadableFile.Get())
	{
		if (FEditorDomain* EditorDomain(FEditorDomain::Get());
			EditorDomain && EditorDomain->IsReadingPackages())
		{
			EditorDomain->PrecachePackageDigest(PackageData.GetPackageName());
		}
		TStringBuilder<NAME_SIZE> FileNameString;
		PackageData.GetFileName().ToString(FileNameString);
		PreloadableFile.Set(MakeShared<FPreloadableArchive>(FileNameString.ToString()), PackageData);
		PreloadableFile.Get()->InitializeAsync([this]()
			{
				TStringBuilder<NAME_SIZE> FileNameString;
				// Note this async callback has an read of PackageData->GetFilename and a write of this->PreloadableFileOpenResult
				// outside of a critical section. This read and write is allowed because GetFilename does
				// not change until the PackageData is destructed, and the destructor does not run and other threads do not read
				// or write PreloadableFileOpenResult until after PreloadableFile.Get() has finished initialization
				// and this callback is therefore complete.
				// The code that accomplishes that waiting is in TryPreload (IsInitialized) and ClearPreload
				// (ReleaseCache)
				PackageData.GetFileName().ToString(FileNameString);
				FPackagePath PackagePath = FPackagePath::FromLocalPath(FileNameString);
				FOpenPackageResult Result = IPackageResourceManager::Get().OpenReadPackage(PackagePath);
				if (Result.Archive)
				{
					this->PreloadableFileOpenResult.CopyMetaData(Result);
				}
				return Result.Archive.Release();
			},
			FPreloadableFile::Flags::PreloadHandle | FPreloadableFile::Flags::Prime);
	}
	const TSharedPtr<FPreloadableArchive>& FilePtr = PreloadableFile.Get();
	if (!FilePtr->IsInitialized())
	{
		if (PackageData.GetIsUrgent())
		{
			// For urgent requests, wait on them to finish preloading rather than letting them run asynchronously
			// and coming back to them later
			FilePtr->WaitForInitialization();
			check(FilePtr->IsInitialized());
		}
		else
		{
			return false;
		}
	}
	if (FilePtr->TotalSize() < 0)
	{
		UE_LOG(LogCook, Warning, TEXT("Failed to find file when preloading %s."),
			*PackageData.GetFileName().ToString());
		bIsPreloadAttempted = true;
		PreloadableFile.Reset(PackageData);
		PreloadableFileOpenResult = FOpenPackageResult();
		return true;
	}

	TStringBuilder<NAME_SIZE> FileNameString;
	PackageData.GetFileName().ToString(FileNameString);
	if (!IPackageResourceManager::TryRegisterPreloadableArchive(FPackagePath::FromLocalPath(FileNameString),
		FilePtr, PreloadableFileOpenResult))
	{
		UE_LOG(LogCook, Warning, TEXT("Failed to register %s for preload."),
			*PackageData.GetFileName().ToString());
		bIsPreloadAttempted = true;
		PreloadableFile.Reset(PackageData);
		PreloadableFileOpenResult = FOpenPackageResult();
		return true;
	}

	bIsPreloaded = true;
	bIsPreloadAttempted = true;
	return true;
}

void FPackagePreloader::ClearPreload()
{
	if (AsyncRequest)
	{
		if (!AsyncRequest->bHasFinished)
		{
			FlushAsyncLoading(AsyncRequest->RequestID);
			check(AsyncRequest->bHasFinished);
		}
		PackageData.GetPackageDatas().GetMonitor().OnPreloadAllocatedChanged(PackageData, false);
		AsyncRequest.Reset();
	}

	const TSharedPtr<FPreloadableArchive>& FilePtr = PreloadableFile.Get();
	if (bIsPreloaded)
	{
		check(FilePtr);
		TStringBuilder<NAME_SIZE> FileNameString;
		PackageData.GetFileName().ToString(FileNameString);
		if (IPackageResourceManager::UnRegisterPreloadableArchive(FPackagePath::FromLocalPath(FileNameString)))
		{
			UE_LOG(LogCook, Display,
				TEXT("PreloadableFile was created for %s but never used. This is wasteful and bad for cook performance."),
				*PackageData.GetPackageName().ToString());
		}
		FilePtr->ReleaseCache(); // ReleaseCache to conserve memory if the Linker still has a pointer to it
	}
	else
	{
		check(!FilePtr || !FilePtr->IsCacheAllocated());
	}

	PreloadableFile.Reset(PackageData);
	PreloadableFileOpenResult = FOpenPackageResult();
	bIsPreloaded = false;
	bIsPreloadAttempted = false;
}

void FPackagePreloader::CheckPreloadEmpty()
{
	check(!AsyncRequest);
	check(!bIsPreloadAttempted);
	check(!PreloadableFile.Get());
	check(!bIsPreloaded);
}

void FPackagePreloader::FTrackedPreloadableFilePtr::Set(TSharedPtr<FPreloadableArchive>&& InPtr, FPackageData& Owner)
{
	Reset(Owner);
	if (InPtr)
	{
		Ptr = MoveTemp(InPtr);
		Owner.GetPackageDatas().GetMonitor().OnPreloadAllocatedChanged(Owner, true);
	}
}

void FPackagePreloader::FTrackedPreloadableFilePtr::Reset(FPackageData& Owner)
{
	if (Ptr)
	{
		Owner.GetPackageDatas().GetMonitor().OnPreloadAllocatedChanged(Owner, false);
		Ptr.Reset();
	}
}

}