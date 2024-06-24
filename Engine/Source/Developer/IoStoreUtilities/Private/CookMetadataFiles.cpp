// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookMetadataFiles.h"

#include "AssetRegistry/AssetRegistryState.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CookMetadata.h"
#include "HAL/FileManager.h"
#include "IO/IoStore.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "Serialization/LargeMemoryReader.h"

// Returns the hash of the development asset registry or 0 on failure.
static uint64 LoadAssetRegistry(const FString& InAssetRegistryFileName, FAssetRegistryState& OutAssetRegistry)
{
	FAssetRegistryVersion::Type Version;
	FAssetRegistryLoadOptions Options(UE::AssetRegistry::ESerializationTarget::ForDevelopment);

	TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(*InAssetRegistryFileName));
	if (FileReader)
	{
		TArray64<uint8> Data;
		Data.SetNumUninitialized(FileReader->TotalSize());
		FileReader->Serialize(Data.GetData(), Data.Num());
		check(!FileReader->IsError());

		uint64 DevArHash = UE::Cook::FCookMetadataState::ComputeHashOfDevelopmentAssetRegistry(MakeMemoryView(Data));

		FLargeMemoryReader MemoryReader(Data.GetData(), Data.Num());
		if (OutAssetRegistry.Load(MemoryReader, Options, &Version))
		{
			return DevArHash;
		}
	}

	return 0;;
}

ECookMetadataFiles FindAndLoadMetadataFiles(
	const FString& InCookedDir, ECookMetadataFiles InRequiredFiles, 
	FAssetRegistryState& OutAssetRegistry, FString* OutAssetRegistryFileName /*optional, set on success*/,
	UE::Cook::FCookMetadataState* OutCookMetadata, FString* OutCookMetadataFileName /*optional, set on success or need*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(LoadingAssetRegistry);

	// Look for the development registry. Should be in \\GameName\\Metadata\\DevelopmentAssetRegistry.bin, but we don't know what "GameName" is.
	TArray<FString> PossibleAssetRegistryFiles;
	IFileManager::Get().FindFilesRecursive(PossibleAssetRegistryFiles, *InCookedDir, GetDevelopmentAssetRegistryFilename(), true, false);

	if (PossibleAssetRegistryFiles.Num() > 1)
	{
		UE_LOG(LogIoStore, Warning, TEXT("Found multiple possible development asset registries:"));
		for (FString& Filename : PossibleAssetRegistryFiles)
		{
			UE_LOG(LogIoStore, Warning, TEXT("    %s"), *Filename);
		}
	}

	if (PossibleAssetRegistryFiles.Num() == 0)
	{
		if (EnumHasAnyFlags(InRequiredFiles, ECookMetadataFiles::AssetRegistry))
		{
			UE_LOG(LogIoStore, Error, TEXT("No development asset registry file found!"));
		}
		else
		{
			UE_LOG(LogIoStore, Display, TEXT("No development asset registry file found!"));
		}
		return ECookMetadataFiles::None;
	}

	UE_LOG(LogIoStore, Display, TEXT("Using input asset registry: %s"), *PossibleAssetRegistryFiles[0]);
	uint64 LoadedDevArHash = LoadAssetRegistry(PossibleAssetRegistryFiles[0], OutAssetRegistry);

	if (LoadedDevArHash == 0)
	{
		return ECookMetadataFiles::None; // already logged
	}

	// If we found the asset registry, try and find the cook metadata that should be next to it.
	ECookMetadataFiles ResultFiles = ECookMetadataFiles::AssetRegistry;

	if (OutCookMetadata)
	{
		// The cook metadata file should be adjacent to the development asset registry.
		FString CookMetadataFileName = FPaths::GetPath(PossibleAssetRegistryFiles[0]) / UE::Cook::GetCookMetadataFilename();
		if (IFileManager::Get().FileExists(*CookMetadataFileName))
		{
			if (OutCookMetadata->ReadFromFile(CookMetadataFileName) == false)
			{
				UE_LOG(LogIoStore, Error, TEXT("Failed to deserialize cook metadata file - invalid data. [%s]"), *CookMetadataFileName);
				if (EnumHasAnyFlags(InRequiredFiles, ECookMetadataFiles::CookMetadata))
				{
					return ECookMetadataFiles::None;
				}
			}
			else if (OutCookMetadata->GetAssociatedDevelopmentAssetRegistryHash() != LoadedDevArHash &&
				OutCookMetadata->GetAssociatedDevelopmentAssetRegistryHashPostWriteback() != LoadedDevArHash) // during testing we can repeat stage after cook so we might have already edited it.
			{
				if (EnumHasAnyFlags(InRequiredFiles, ECookMetadataFiles::CookMetadata))
				{
					UE_LOG(LogIoStore, Error,
						TEXT("Cook metadata file mismatch: Hash of associated development asset registry does not match. [%s] %llx vs %llx (%llx post writeback)"),
						*CookMetadataFileName, LoadedDevArHash, OutCookMetadata->GetAssociatedDevelopmentAssetRegistryHash(), OutCookMetadata->GetAssociatedDevelopmentAssetRegistryHashPostWriteback());
					return ECookMetadataFiles::None;
				}
				else
				{
					UE_LOG(LogIoStore, Display,
						TEXT("Cook metadata file mismatch: Hash of associated development asset registry does not match. [%s] %llx vs %llx (%llx post writeback)"),
						*CookMetadataFileName, LoadedDevArHash, OutCookMetadata->GetAssociatedDevelopmentAssetRegistryHash(), OutCookMetadata->GetAssociatedDevelopmentAssetRegistryHashPostWriteback());
					OutCookMetadata->Reset();
				}
			}
			else
			{
				EnumAddFlags(ResultFiles, ECookMetadataFiles::CookMetadata);
				if (OutCookMetadataFileName)
				{
					*OutCookMetadataFileName = MoveTemp(CookMetadataFileName);
				}
			}
		}
		else
		{
			if (EnumHasAnyFlags(InRequiredFiles, ECookMetadataFiles::CookMetadata))
			{
				UE_LOG(LogIoStore, Error, TEXT("Failed to open and read cook metadata file %s"), *CookMetadataFileName);
				return ECookMetadataFiles::None;
			}

			UE_LOG(LogIoStore, Display, TEXT("No cook metadata file found, checked %s"), *CookMetadataFileName);
			if (OutCookMetadataFileName)
			{
				*OutCookMetadataFileName = FString("");
			}
		}
	}


	if (OutAssetRegistryFileName)
	{
		*OutAssetRegistryFileName = MoveTemp(PossibleAssetRegistryFiles[0]);
	}
	return ResultFiles;
}