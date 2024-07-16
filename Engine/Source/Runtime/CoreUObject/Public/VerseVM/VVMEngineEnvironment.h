// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "Containers/StringFwd.h"

class FString;
class UPackage;
class UStruct;
struct FTopLevelAssetPath;

namespace uLang
{
class CTypeBase;
class CScope;
} // namespace uLang

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
	// Collect property information during code generation.
	virtual void CollectPropertyInfo(FAllocationContext Context, const uLang::CTypeBase* Type, VPropertyType** OutPropertyType) = 0;

	// Build the key used to look up native binding info for a module, class, or struct.
	virtual FTopLevelAssetPath GetAssetPathForScope(const uLang::CScope& Scope) = 0;

	// Bind a native module, class, or struct.
	virtual void TryBindNativeAsset(FAllocationContext Context, const FTopLevelAssetPath& Path) = 0;

	// Given a UPackage name, adjust the name when the package stage is either DEAD or TEMP.
	virtual const TCHAR* AdornPackageName(const TCHAR* PackageName, EPackageStage Stage, FString& ScratchSpace) = 0;

	// Create a new UPackage with the given package name
	virtual UPackage* CreateUPackage(FAllocationContext Context, const TCHAR* PackageName) = 0;

	// Create a new UClass/UScriptStruct from an existing VClass during native binding or for CVarUObjectProbability.
	virtual UStruct* CreateUStruct(FAllocationContext Context, VClass* Class) = 0;
};
} // namespace Verse
#endif // WITH_VERSE_VM
