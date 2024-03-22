// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackageEntry.h"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

FPackageEntry::FPackageEntry(const TraceServices::FPackageData& PackageData)
	: Id(PackageData.Id)
	, Name(PackageData.Name)
	, LoadTimeIncl(PackageData.LoadTimeIncl)
	, LoadTimeExcl(PackageData.LoadTimeExcl)
	, SaveTimeIncl(PackageData.SaveTimeIncl)
	, SaveTimeExcl(PackageData.SaveTimeExcl)
	, BeginCacheForCookedPlatformDataIncl(PackageData.BeginCacheForCookedPlatformDataIncl)
	, BeginCacheForCookedPlatformDataExcl(PackageData.BeginCacheForCookedPlatformDataExcl)
	, IsCachedCookedPlatformDataLoadedIncl(PackageData.IsCachedCookedPlatformDataLoadedIncl)
	, IsCachedCookedPlatformDataLoadedExcl(PackageData.IsCachedCookedPlatformDataLoadedExcl)
	, AssetClass(PackageData.AssetClass)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
