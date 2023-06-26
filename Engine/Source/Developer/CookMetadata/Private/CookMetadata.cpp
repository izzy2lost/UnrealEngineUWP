// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookMetadata.h"

#include "Hash/xxhash.h"
#include "Internationalization/Internationalization.h"
#include "Memory/MemoryView.h"

namespace UE::Cook
{

const FString& GetCookMetadataFilename()
{
	static const FString CookMetadataFilename(TEXT("CookMetadata.ucookmeta"));
	return CookMetadataFilename;
}

constexpr uint32 COOK_METADATA_HEADER_MAGIC = 'UCMT';

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

	Ar << Version;
	if (Ar.IsLoading())
	{
		if (Version != ECookMetadataStateVersion::LatestVersion)
		{
			return false; // invalid version - current we don't support backcompat
		}
	}
	Ar << PluginHierarchy;
	Ar << AssociatedDevelopmentAssetRegistryHash;
	Ar << AssociatedDevelopmentAssetRegistryHashPostWriteback;
	Ar << Platform;
	Ar << BuildVersion;
	Ar << HordeJobId;
	Ar << SizesPresent;
	return true;
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