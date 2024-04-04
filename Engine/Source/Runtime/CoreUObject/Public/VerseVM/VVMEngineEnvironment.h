// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "Containers/StringFwd.h"

class FString;
class UPackage;
class UVerseVMClass;

namespace Verse
{
struct FAllocationContext;
struct VClass;
struct VPackage;
enum class EPackageStage : uint8;

// This interface must be implemented if Verse needs to create UObject instances.
class IEngineEnvironment
{
public:
	// Create a new UPackage with the given package name
	virtual UPackage* CreateUPackage(FAllocationContext Context, const TCHAR* PackageName) = 0;

	// Create a new UClass from an existing VClass
	virtual UVerseVMClass* CreateUClass(FAllocationContext Context, const VClass* Class) = 0;

	// Given a UPackage name, adjust the name when the package stage is either DEAD or TEMP.
	virtual const TCHAR* AdornPackageName(const TCHAR* PackageName, EPackageStage Stage, FString& ScratchSpace) = 0;

	// Convert a verse property name to UE property name
	virtual FString VerseToUEPropertyName(const char* VerseName, bool* bSetDisplayName = nullptr) = 0;
};
} // namespace Verse
#endif // WITH_VERSE_VM
