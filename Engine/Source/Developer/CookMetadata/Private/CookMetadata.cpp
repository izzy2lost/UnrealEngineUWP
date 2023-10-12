// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookMetadata.h"

#include "HAL/FileManager.h"
#include "Hash/xxhash.h"
#include "Internationalization/Internationalization.h"
#include "Memory/MemoryView.h"
#include "Misc/FileHelper.h"
#include "Serialization/ArrayWriter.h"
#include "Serialization/LargeMemoryReader.h"

namespace UE::Cook
{

const FString& GetCookMetadataFilename()
{
	static const FString CookMetadataFilename(TEXT("CookMetadata.ucookmeta"));
	return CookMetadataFilename;
}

constexpr uint32 COOK_METADATA_HEADER_MAGIC = 'UCMT';

DEFINE_LOG_CATEGORY_STATIC(LogCookedMetadata, Log, All)

bool FCookMetadataState::Serialize(FArchive& Ar)
{
	uint32 MagicHeader = 0;
	if (Ar.IsLoading())
	{
		Ar << MagicHeader;
		if (MagicHeader != COOK_METADATA_HEADER_MAGIC)
		{
			return false; // not a metadata file.
		}
	}
	else
	{
		MagicHeader = COOK_METADATA_HEADER_MAGIC;
		Ar << MagicHeader;

		Version = ECookMetadataStateVersion::LatestVersion;
	}

	bool bSerializeShaderHierarchy = true;
	Ar << Version;
	if (Ar.IsLoading())
	{
		if (Version < ECookMetadataStateVersion::AddedPluginEntryType)
		{
			UE_LOG(LogCookedMetadata, Error, TEXT("Cook metadata version too old: found %d, we can load %d, latest is %d"), Version, ECookMetadataStateVersion::AddedPluginEntryType, ECookMetadataStateVersion::LatestVersion);
			return false; // invalid version - current we don't support backcompat
		}
		if (Version > ECookMetadataStateVersion::LatestVersion)
		{
			UE_LOG(LogCookedMetadata, Error, TEXT("Cook metadata version too new: found %d, latest is %d"), Version, ECookMetadataStateVersion::LatestVersion);
			return false;
		}

		bSerializeShaderHierarchy = Version >= ECookMetadataStateVersion::ActualAddShaderPseudoHierarchy;

	}
	Ar << PluginHierarchy;
	Ar << AssociatedDevelopmentAssetRegistryHash;
	Ar << AssociatedDevelopmentAssetRegistryHashPostWriteback;
	Ar << Platform;
	Ar << BuildVersion;
	Ar << HordeJobId;
	Ar << SizesPresent;

	if (bSerializeShaderHierarchy)
	{
		Ar << ShaderPseudoHierarchy;
	}
	return true;
}

bool FCookMetadataState::ReadFromFile(const FString& FilePath)
{
	TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(*FilePath));
	if (FileReader)
	{
		TArray64<uint8> Data;
		Data.SetNumUninitialized(FileReader->TotalSize());
		FileReader->Serialize(Data.GetData(), Data.Num());
		check(!FileReader->IsError());

		FLargeMemoryReader MemoryReader(Data.GetData(), Data.Num());
		if (Serialize(MemoryReader))
		{
			return true;
		}
		else
		{
			UE_LOG(LogCookedMetadata, Error, TEXT("Failed to serialize cook metadata from file (%s)"), *FilePath);
		}
	}
	else
	{
		UE_LOG(LogCookedMetadata, Error, TEXT("Failed to make file reader from (%s)"), *FilePath);
	}
	return false;
}

bool FCookMetadataState::SaveToFile(const FString& FilePath)
{
	FArrayWriter SerializedCookMetadata;
	Serialize(SerializedCookMetadata);
	if (FFileHelper::SaveArrayToFile(SerializedCookMetadata, *FilePath))
	{
		return true;
	}
	else
	{
		UE_LOG(LogCookedMetadata, Error, TEXT("Failed to write cook metadata file (%s)"), *FilePath);
		return false;
	}
}

/* static */ 
uint64 FCookMetadataState::ComputeHashOfDevelopmentAssetRegistry(FMemoryView InSerializedDevelopmentAssetRegistry)
{
	return FXxHash64::HashBufferChunked(InSerializedDevelopmentAssetRegistry.GetData(), InSerializedDevelopmentAssetRegistry.GetSize(), 1ULL << 19).Hash;
}

FText FCookMetadataState::GetSizesPresentAsText() const
{
	// Uncapitalized, presentation-ready
	static FText CookMetadataSizesPresentStrings[] =
	{
		NSLOCTEXT("CookMetadata", "CookMetadataNotPresent", "not present"),
		NSLOCTEXT("CookMetadata", "CookMetadataCompressed", "compressed"),
		NSLOCTEXT("CookMetadata", "CookMetadataUncompressed", "uncompressed")
	};

	static_assert(sizeof(CookMetadataSizesPresentStrings) / sizeof(CookMetadataSizesPresentStrings[0]) == static_cast<size_t>(ECookMetadataSizesPresent::Count));
	return CookMetadataSizesPresentStrings[static_cast<size_t>(SizesPresent)];
}

} // end namespace