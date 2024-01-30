// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/AvalancheVersionInfo.h"
#include "Misc/EngineVersion.h"
#include "Serialization/CustomVersion.h"
#include "UObject/ObjectVersion.h"

void FAvalancheFileVersionInfo::UpdateToLatest()
{
	FileVersionUE4      = GPackageFileUEVersion.FileVersionUE4;
	FileVersionUE5      = GPackageFileUEVersion.FileVersionUE5;
	FileVersionLicensee = GPackageFileLicenseeUEVersion;
}

void FAvalancheEngineVersionInfo::UpdateToLatest()
{
	const FEngineVersion& EngineVersion(FEngineVersion::Current());
	Major      = EngineVersion.GetMajor();
	Minor      = EngineVersion.GetMinor();
	Patch      = EngineVersion.GetPatch();
	Changelist = EngineVersion.GetChangelist();
}

void FAvalancheCustomVersionInfo::SetCustomVersion(const FCustomVersion& InVersion)
{
	FriendlyName = InVersion.GetFriendlyName();
	Key          = InVersion.Key;
	Version      = InVersion.Version;
}

void FAvalancheVersionInfo::UpdateToLatest()
{
	FileVersion.UpdateToLatest();
	EngineVersion.UpdateToLatest();

	if (!UsedCustomVersions.IsEmpty())
	{
		CustomVersions.Empty(UsedCustomVersions.Num());
		for (const FGuid& UsedVersionGuid : UsedCustomVersions)
		{
			const TOptional<FCustomVersion> CustomVersion = FCurrentCustomVersions::Get(UsedVersionGuid);
			if (CustomVersion.IsSet())
			{
				CustomVersions.AddDefaulted_GetRef().SetCustomVersion(CustomVersion.GetValue());
			}
		}
	}
	else
	{
		FCustomVersionArray CurrentCustomVersions = FCurrentCustomVersions::GetAll().GetAllVersions();
		CustomVersions.Empty(CurrentCustomVersions.Num());
	
		for (const FCustomVersion& CustomVersion : CurrentCustomVersions)
		{
			CustomVersions.AddDefaulted_GetRef().SetCustomVersion(CustomVersion);
		}
	}
}

void FAvalancheVersionInfo::ApplyToArchive(FArchive& Archive) const
{
	//If CustomVersions is Empty, it's pre-versioning
	if (CustomVersions.IsEmpty())
	{
		return;
	}
	
	FPackageFileVersion UEVersion;
	UEVersion.FileVersionUE4 = FileVersion.FileVersionUE4;
	UEVersion.FileVersionUE5 = FileVersion.FileVersionUE5;

	Archive.SetUEVer(UEVersion);
	Archive.SetLicenseeUEVer(FileVersion.FileVersionLicensee);
	Archive.SetEngineVer(FEngineVersionBase(EngineVersion.Major, EngineVersion.Minor, EngineVersion.Patch, EngineVersion.Changelist));

	FCustomVersionContainer CustomVersionContainer;
	for (const FAvalancheCustomVersionInfo& CustomVersion : CustomVersions)
	{
		CustomVersionContainer.SetVersion(CustomVersion.Key
			, CustomVersion.Version
			, CustomVersion.FriendlyName);
	}
	
	Archive.SetCustomVersions(CustomVersionContainer);
}
