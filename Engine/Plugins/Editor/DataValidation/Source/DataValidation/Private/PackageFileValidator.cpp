// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackageFileValidator.h"

#include "DataValidationChangelist.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/DataValidation.h"
#include "UObject/PackageFileSummary.h"
#include "UObject/PackageTrailer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PackageFileValidator)

#define LOCTEXT_NAMESPACE "PackageFileValidator"

bool UPackageFileValidator::CanValidateAsset_Implementation(const FAssetData& AssetData, UObject* Asset, FDataValidationContext& Context) const
{
	// We don't want to validate the package on disk when saving, as we will be overwriting that file anyway
	if (Context.GetValidationUsecase() == EDataValidationUsecase::Save)
	{
		return false;
	}

	if (Asset == nullptr)
	{
		return false;
	}

	if (UDataValidationChangelist::StaticClass() == Asset->GetClass())
	{
		return true;
	}

	if (UPackage* Package = Asset->GetPackage())
	{
		// Memory only packages don't have any files to validate
		return !Package->HasAnyPackageFlags(PKG_InMemoryOnly);
	}

	return false;
}

EDataValidationResult UPackageFileValidator::ValidateLoadedAsset_Implementation(const FAssetData& AssetData, UObject* Asset, FDataValidationContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPackageFileValidator::ValidateLoadedAsset_Implementation);

	TArray<FName> PackageNames = FindPackageNames(Asset);

	int32 NumFailedPackages = 0;

	for (FName PackageName : PackageNames)
	{
		FPackagePath PackagePath;
		if (!FPackagePath::TryFromPackageName(PackageName, PackagePath))
		{
			NumFailedPackages++;
			AssetFails(nullptr, FText::Format(LOCTEXT("NoPackageTag", "{0} Unable to open for reading"), FText::FromName(PackageName)));
			
			continue;
		}

		TUniquePtr<FArchive> PackageAr(IFileManager::Get().CreateFileReader(*PackagePath.GetLocalFullPath(), FILEREAD_Silent));
		if (!PackageAr.IsValid())
		{
			NumFailedPackages++;
			AssetFails(nullptr, FText::Format(LOCTEXT("NoPackageTag", "{0} Unable to open for reading"), FText::FromName(PackageName)));
			
			continue;
		}

		FPackageFileSummary Summary;
		if (!ValidatePackageSummary(PackageName, *PackageAr, Context, Summary))
		{
			NumFailedPackages++;
			continue;
		}

		if (Summary.PayloadTocOffset > 0 && !ValidatePackageTrailer(PackageName, *PackageAr, Context))
		{
			NumFailedPackages++;
			continue;
		}
	}

	return NumFailedPackages == 0 ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}

bool UPackageFileValidator::ValidatePackageSummary(FName PackageName, FArchive& Ar, FDataValidationContext& Context, FPackageFileSummary& OutSummary)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPackageFileValidator::ValidatePackageSummary);

	const int64 TagOffset = Ar.TotalSize() - sizeof(uint32);
	Ar.Seek(TagOffset);

	uint32 Tag = 0;
	Ar << Tag;

	if (Tag != PACKAGE_FILE_TAG || Ar.IsError())
	{
		AssetFails(nullptr, FText::Format(LOCTEXT("BadPkgTag", "{0} The end of package tag is not valid, the file is probably corrupt"), FText::FromName(PackageName)));
		return false;
	}

	Ar.Seek(0);

	Ar << OutSummary;

	if (Ar.IsError() || OutSummary.Tag != PACKAGE_FILE_TAG)
	{
		AssetFails(nullptr, FText::Format(LOCTEXT("BadPkgSummary", "{0} Failed to read the package file summary, the file is probably corrupt"), FText::FromName(PackageName)));
		return false;
	}

	if (OutSummary.IsFileVersionTooOld())
	{
		AssetFails(nullptr, FText::Format(LOCTEXT("PkgOutOfDate", "{0} is out of date and is not backwards compatible with the current process. Min Required Version: {1}  Package Version: {2}"),
			FText::FromName(PackageName),
			(int32)VER_UE4_OLDEST_LOADABLE_PACKAGE,
			OutSummary.GetFileVersionUE().FileVersionUE4));
		return false;
	}

	return true;
}

bool UPackageFileValidator::ValidatePackageTrailer(FName PackageName, FArchive& Ar, FDataValidationContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPackageFileValidator::ValidatePackageTrailer);

	using namespace UE;

	FPackageTrailer Trailer;
	if (!FPackageTrailer::TryLoadFromArchive(Ar, Trailer))
	{
		AssetFails(nullptr, FText::Format(LOCTEXT("BadPkgTrailer", "{0} Failed to read the package trailer, the file is probably corrupt"), FText::FromName(PackageName)));
		return false;
	}

	TArray<FIoHash> LocalPayloads = Trailer.GetPayloads(EPayloadStorageType::Local);

	for (const FIoHash& Id : LocalPayloads)
	{
		FCompressedBuffer Payload = Trailer.LoadLocalPayload(Id, Ar);
		if (Payload.IsNull())
		{
			AssetFails(nullptr, FText::Format(LOCTEXT("BadPayload", "{0} Failed to read the payload {1}, the file is probably corrupt"), FText::FromName(PackageName), FText::FromString(LexToString(Id))));
			return false;
		}

		if (Id != Payload.GetRawHash())
		{
			AssetFails(nullptr, FText::Format(LOCTEXT("BadPayloadId", "{0} Failed to read the payload {1}, the file is probably corrupt"), FText::FromName(PackageName), FText::FromString(LexToString(Id))));
			return false;
		}

		if (bValidatePayloadHashes)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPackageFileValidator::ValidatePackageTrailer::HashPayload);

			FIoHash PayloadHash = FIoHash::HashBuffer(Payload.Decompress());
			if (Id != PayloadHash)
			{
				AssetFails(nullptr, FText::Format(LOCTEXT("BadPayloadData", "{0} The payload data did not match it's stored hash {1} vs {2}, the file is probably corrupt"),
					FText::FromName(PackageName),
					FText::FromString(LexToString(Id)),
					FText::FromString(LexToString(PayloadHash))));

				return false;
			}
		}
	}

	return true;
}

TArray<FName> UPackageFileValidator::FindPackageNames(UObject* Asset) const
{
	if (UDataValidationChangelist* ChangeList = Cast<UDataValidationChangelist>(Asset))
	{
		return ChangeList->ModifiedPackageNames;
	}
	else if(UPackage* Package = Asset->GetPackage())
	{
		TArray<FName> PackageNames;
		PackageNames.Add(Package->GetFName());

		return PackageNames;
	}

	return TArray<FName>();
}

#undef LOCTEXT_NAMESPACE
