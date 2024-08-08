// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/Paths.h"
#include "MetaHumanProjectUtilities.h"

// Common data types used in various parts of the MetaHumanProjectUtilities module

struct FMetaHumanAssetImportDescription;

struct FMetaHumanAssetVersion
{
	int32 Major;
	int32 Minor;

	// Comparison operators
	friend bool operator <(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right)
	{
		return Left.Major < Right.Major || (Left.Major == Right.Major && Left.Minor < Right.Minor);
	}

	friend bool operator>(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right) { return Right < Left; }
	friend bool operator<=(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right) { return !(Left > Right); }
	friend bool operator>=(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right) { return !(Left < Right); }

	friend bool operator ==(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right)
	{
		return Right.Major == Left.Major && Right.Minor == Left.Minor;
	}

	friend bool operator!=(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right) { return !(Left == Right); }

	static FMetaHumanAssetVersion FromString(const FString& String)
	{
		FString MajorPart;
		FString MinorPart;
		String.Split(TEXT("."), &MajorPart, &MinorPart);
		return FMetaHumanAssetVersion{FCString::Atoi(*MajorPart), FCString::Atoi(*MinorPart)};
	}

	FString AsString() const
	{
		return FString::Format(TEXT("{0}.{1}"), {Major, Minor});
	}
};

// Reason for performing an update (currently only version difference, but this could be extended).
struct FAssetUpdateReason
{
	FMetaHumanAssetVersion OldVersion;
	FMetaHumanAssetVersion NewVersion;

	// Whether or not the update is a breaking change (change in major version number)
	bool IsBreakingChange() const
	{
		return NewVersion.Major != OldVersion.Major;
	}
};

// List of relative asset paths to be Added, Replaced etc. as part of the current import action
struct FAssetOperationPaths
{
	TArray<FString> Add;
	TArray<FString> Replace;
	TArray<FString> Skip;
	TArray<FString> Update;
	TArray<FAssetUpdateReason> UpdateReasons;
};

// Helper structure to simplify management of file and asset paths. All paths are absolute and explicit as to whether
// they are a file path or an asset path.
struct FImportPaths
{
	const FString MetaHumansFolderName = TEXT("MetaHumans");
	const FString CommonFolderName = TEXT("Common");

	explicit FImportPaths(FMetaHumanAssetImportDescription ImportDescription);

	static FString FilenameToAssetName(const FString& Filename)
	{
		return FString::Format(TEXT("{0}.{0}"), {FPaths::GetBaseFilename(Filename)});
	}

	static FString AssetNameToFilename(const FString& AssetName)
	{
		return FString::Format(TEXT("{0}.uasset"), {AssetName});
	}

	FString CharacterNameToBlueprintAssetPath(const FString& CharacterName) const
	{
		return FPaths::Combine(DestinationMetaHumansAssetPath, CharacterName, FString::Format(TEXT("BP_{0}.BP_{0}"), {CharacterName}));
	}

	FString GetSourceFile(const FString& RelativeFilePath) const
	{
		return FPaths::Combine(SourceRootFilePath, RelativeFilePath);
	}

	FString GetDestinationFile(const FString& RelativeFilePath) const
	{
		return FPaths::Combine(DestinationRootFilePath, RelativeFilePath);
	}

	FString GetDestinationAsset(const FString& RelativeFilePath) const
	{
		return FPaths::Combine(DestinationRootAssetPath, FPaths::GetPath(RelativeFilePath), FilenameToAssetName(RelativeFilePath));
	}

	FString SourceRootFilePath;
	FString SourceMetaHumansFilePath;
	FString SourceCharacterFilePath;
	FString SourceCommonFilePath;

	FString DestinationRootFilePath;
	FString DestinationMetaHumansFilePath;
	FString DestinationCharacterFilePath;
	FString DestinationCommonFilePath;

	FString DestinationRootAssetPath;
	FString DestinationMetaHumansAssetPath;
	FString DestinationCharacterAssetPath;
	FString DestinationCommonAssetPath;
};


// Class that handles the layout on-disk of a MetaHuman being used as the source of an Import operation
// Gives us a single place to handle simple path operations, filenames etc.
class FSourceMetaHuman
{
public:
	FSourceMetaHuman(const FString& RootPath, const FString& Name)
		: RootPath(RootPath)
		, Name(Name)
	{
		const FString VersionFilePath = FPaths::Combine(RootPath, Name, TEXT("VersionInfo.txt"));
		Version = FMetaHumanVersion::ReadFromFile(VersionFilePath);
	}

	const FString& GetName() const
	{
		return Name;
	}

	const FMetaHumanVersion& GetVersion() const
	{
		return Version;
	}

	EMetaHumanQualityLevel GetQualityLevel() const
	{
		if (RootPath.Contains(TEXT("Tier0")))
		{
			return EMetaHumanQualityLevel::High;
		}
		if (RootPath.Contains(TEXT("Tier2")))
		{
			return EMetaHumanQualityLevel::Medium;
		}
		else
		{
			return EMetaHumanQualityLevel::Low;
		}
	}

private:
	FString RootPath;
	FString Name;
	FMetaHumanVersion Version;
};



