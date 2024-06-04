// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "Containers/StringFwd.h"

class FString;
class UPackage;
class UVerseVMClass;

namespace uLang
{
class CTypeBase;
}

namespace Verse
{
struct FAllocationContext;
struct VClass;
struct VPackage;
struct VPropertyType;
enum class EPackageStage : uint8;

// This interface must be implemented if Verse needs to create UObject instances.
class IEngineEnvironment
{
public:
	// Create a new UPackage with the given package name
	virtual UPackage* CreateUPackage(FAllocationContext Context, const TCHAR* PackageName) = 0;

	// Given a UPackage name, adjust the name when the package stage is either DEAD or TEMP.
	virtual const TCHAR* AdornPackageName(const TCHAR* PackageName, EPackageStage Stage, FString& ScratchSpace) = 0;

	// Create a new UClass from an existing VClass
	virtual UVerseVMClass* CreateUClass(FAllocationContext Context, VClass* Class) = 0;

	// Collect property information
	virtual void CollectPropertyInfo(FAllocationContext Context, const uLang::CTypeBase* Type, VPropertyType** OutPropertyType) = 0;
};
} // namespace Verse
#endif // WITH_VERSE_VM
