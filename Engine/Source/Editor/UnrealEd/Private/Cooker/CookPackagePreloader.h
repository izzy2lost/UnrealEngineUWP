// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cooker/CookPackageData.h"
#include "Templates/RefCounting.h"
#include "Templates/SharedPointer.h"
#include "UObject/PackageResourceManager.h"

#include <atomic>

namespace UE::Cook
{

/**
 * Helper class for the Load state on FPackageData. Also supports loads of other packages that depend on the owner
 * package. Handles preloading or asyncloading the package before we put the package into LoadReadyQueue for
 * a call to LoadPackage.
 */
class FPackagePreloader : public FThreadSafeRefCountedObject
{
public:
	FPackagePreloader(FPackageData& InPackageData);
	~FPackagePreloader();

	/** Free any IO requests or buffers, and remove all references to any other FPackagePreloaders. */
	void Shutdown();

	/** Set the SelfReference that keeps this object in memory while the owner package is using it. */
	void SetSelfReference();
	/** Clear the SelfReference; the owner package is done using this object. */
	void ClearSelfReference();

	/** Try to preload the file. Return true if preloading is complete (succeeded or failed or was skipped). */
	bool TryPreload();
	/** Clear any allocated preload data. */
	void ClearPreload();
	/** Issue check statements confirming that no preload data is allocated or flags are set. */
	void CheckPreloadEmpty();

private:
	/**
	 * The number of active PreloadableFiles is tracked globally; wrap the PreloadableFile in a struct that
	 * guarantees we always update the counter when changing it
	 */
	struct FTrackedPreloadableFilePtr
	{
		const TSharedPtr<FPreloadableArchive>& Get() { return Ptr; }
		void Set(TSharedPtr<FPreloadableArchive>&& InPtr, FPackageData& PackageData);
		void Reset(FPackageData& PackageData);
	private:
		TSharedPtr<FPreloadableArchive> Ptr;
	};

	/**
	 * Structure used when we are doing async packageloads rather than just preloading. Holds the
	 * request data that gets written from the AsyncLoading thread.
	 */
	struct FAsyncRequest
	{
		int32 RequestID{ 0 };
		std::atomic<bool> bHasFinished{ false };
	};

private:
	FTrackedPreloadableFilePtr PreloadableFile;
	FOpenPackageResult PreloadableFileOpenResult;
	FPackageData& PackageData;
	TRefCountPtr<FPackagePreloader> SelfReference;
	TSharedPtr<FAsyncRequest> AsyncRequest;
	bool bIsPreloadAttempted = false;
	bool bIsPreloaded = false;
};

}

///////////////////////////////////////////////////////
// Inline implementations
///////////////////////////////////////////////////////

namespace UE::Cook
{

inline FPackagePreloader::FPackagePreloader(FPackageData& InPackageData)
: PackageData(InPackageData)
{

}

inline void FPackagePreloader::SetSelfReference()
{
	SelfReference = this;
}

inline void FPackagePreloader::ClearSelfReference()
{
	SelfReference.SafeRelease();
}

}