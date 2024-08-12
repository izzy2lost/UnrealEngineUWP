// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZenCookArtifactReader.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "StorageServerClientModule.h"
#include "ZenStoreHttpClient.h"

FZenCookArtifactReader::FZenCookArtifactReader(
	const FString& InputPath, 
	const FString& InMetadataDirectoryPath, 
	const ITargetPlatform* InTargetPlatform
)
	: ZenRootPath(InputPath)
	, StorageServerPlatformFile(IStorageServerClientModule::Get().TryCreateCustomPlatformFile(*InputPath, &FPlatformFileManager::Get().GetPlatformFile()))
{
	if (StorageServerPlatformFile)
	{
		StorageServerPlatformFile->SetLowerLevel(nullptr);
	}
}

FZenCookArtifactReader::~FZenCookArtifactReader()
{
}

bool FZenCookArtifactReader::FileExists(const TCHAR* Filename)
{
	if (StorageServerPlatformFile)
	{
		FString StandardFilename;
		if (MakeStorageServerPath(Filename, StandardFilename))
		{
			return StorageServerPlatformFile->FileExists(*StandardFilename);
		}
	}
	return false;
}

int64 FZenCookArtifactReader::FileSize(const TCHAR* Filename)
{
	if (StorageServerPlatformFile)
	{
		FString StandardFilename;
		if (MakeStorageServerPath(Filename, StandardFilename))
		{
			return StorageServerPlatformFile->FileSize(*StandardFilename);
		}
	}

	return -1;
}

IFileHandle* FZenCookArtifactReader::OpenRead(const TCHAR* Filename)
{
	if (StorageServerPlatformFile)
	{
		FString StandardFilename;
		if (MakeStorageServerPath(Filename, StandardFilename))
		{
			return StorageServerPlatformFile->OpenRead(*StandardFilename);
		}
	}
	return nullptr;
}

bool FZenCookArtifactReader::IterateDirectory(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor)
{
	if (StorageServerPlatformFile)
	{
		FString StandardDirectory;
		if (MakeStorageServerPath(Directory, StandardDirectory))
		{
			return StorageServerPlatformFile->IterateDirectory(Directory, Visitor);
		}
	}

	return false;
}

bool FZenCookArtifactReader::MakeStorageServerPath(const TCHAR* Filename, FString& OutFilename) const
{
	OutFilename = Filename;
	if (FPaths::IsUnderDirectory(OutFilename, ZenRootPath) && FPaths::MakePathRelativeTo(OutFilename, *ZenRootPath))
	{
		static FString ProjectPrefix = FString::Printf(TEXT("%s/"), FApp::GetProjectName());
		if (OutFilename.StartsWith(*ProjectPrefix))
		{
			OutFilename.RightChopInline(ProjectPrefix.Len());
			OutFilename.InsertAt(0, *FPaths::ProjectDir());
		}
		else
		{
			OutFilename.InsertAt(0, "../../../");
		}
		return true;
	}
	return false;
}