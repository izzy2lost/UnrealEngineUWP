// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookPackageSplitter.h"

#if WITH_EDITOR
#include "Misc/Paths.h"
#include "Misc/PathViews.h"

const TCHAR* ICookPackageSplitter::GetGeneratedPackageSubPath()
{
	return TEXT("_Generated_");
}

bool ICookPackageSplitter::IsUnderGeneratedPackageSubPath(FStringView FileOrLongPackagePath)
{
	FStringView GeneratedSubDir(GetGeneratedPackageSubPath());
	int32 Index = FileOrLongPackagePath.Find(GeneratedSubDir, ESearchCase::IgnoreCase);
	if (Index < 0)
	{
		return false;
	}
	if (Index == 0 || !FPathViews::IsSeparator(FileOrLongPackagePath[Index - 1]))
	{
		// "_Generated_/..." or ".../Prefix_Generated_/...", not the /_Generated_/ subfolder we're looking for.
		return false;
	}
	if (Index + GeneratedSubDir.Len() < FileOrLongPackagePath.Len()
		&& !FPathViews::IsSeparator(FileOrLongPackagePath[Index + GeneratedSubDir.Len()]))
	{
		// ".../_Generated_Suffix/...", not the /_Generated_/ subfolder we're looking for.
		return false;
	}
	return true;
}

FString ICookPackageSplitter::ConstructGeneratedPackageName(FName OwnerPackageName, FStringView RelPath,
	FStringView GeneratedRootOverride)
{
	FString PackageRoot;
	if (GeneratedRootOverride.IsEmpty())
	{
		PackageRoot = OwnerPackageName.ToString();
	}
	else
	{
		PackageRoot = GeneratedRootOverride;
	}
	return FPaths::RemoveDuplicateSlashes(FString::Printf(TEXT("/%s/%s/%.*s"),
		*PackageRoot, GetGeneratedPackageSubPath(), RelPath.Len(), RelPath.GetData()));
}

namespace UE::Cook::Private
{

static TLinkedList<FRegisteredCookPackageSplitter*>* GRegisteredCookPackageSplitterList = nullptr;

FRegisteredCookPackageSplitter::FRegisteredCookPackageSplitter()
: GlobalListLink(this)
{
	GlobalListLink.LinkHead(GetRegisteredList());
}

FRegisteredCookPackageSplitter::~FRegisteredCookPackageSplitter()
{
	GlobalListLink.Unlink();
}

TLinkedList<FRegisteredCookPackageSplitter*>*& FRegisteredCookPackageSplitter::GetRegisteredList()
{
	return GRegisteredCookPackageSplitterList;
}

void FRegisteredCookPackageSplitter::ForEach(TFunctionRef<void(FRegisteredCookPackageSplitter*)> Func)
{
	for (TLinkedList<FRegisteredCookPackageSplitter*>::TIterator It(GetRegisteredList()); It; It.Next())
	{
		Func(*It);
	}
}

}

#endif