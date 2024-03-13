// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMCell.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMNameValueMap.h"
#include "VerseVM/VVMPackageName.h" // Needed for the EPackage enums

class UPackage;

namespace Verse
{
enum class EDigestVariant : uint8
{
	PublicAndEpicInternal = 0,
	PublicOnly = 1,
};

struct VPackage : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	TWriteBarrier<VUTF8String> DigestCode[2]; // One for each variant

	VUTF8String& GetName() const { return *PackageName; }

	uint32 Num() const { return Map.Num(); }
	const VUTF8String& GetName(uint32 Index) const { return Map.GetName(Index); }
	VValue GetDefinition(uint32 Index) const { return Map.GetValue(Index).Follow(); }
	void AddDefinition(FAllocationContext Context, FUtf8StringView Name, VValue Definition) { Map.AddValue(Context, Name, Definition); }
	void AddDefinition(FAllocationContext Context, VUTF8String& Name, VValue Definition) { Map.AddValue(Context, Name, Definition); }
	VValue LookupDefinition(FUtf8StringView Name) const { return Map.Lookup(Name); }
	template <typename CellType>
	CellType* LookupDefinition(FUtf8StringView Name) const { return Map.LookupCell<CellType>(Name); }

	COREUOBJECT_API UPackage* GetUPackage(const TCHAR* QualifiedClassName) const;
	COREUOBJECT_API UPackage* GetOrCreateUPackage(FAllocationContext Context, const TCHAR* QualifiedClassName);
	COREUOBJECT_API FString GetUPackageName(const TCHAR* QualifiedClassName, EPackageStage Stage, EPackageType* OutPackageType = nullptr) const;

	EPackageStage GetStage() const { return PackageStage; }
	COREUOBJECT_API void SetStage(EPackageStage InPackageStage);

	EPackageType GetPackageType() const { return PackageType; }

	static VPackage& New(FAllocationContext Context, VUTF8String& Name, uint32 Capacity, EPackageStage InPackageStage = EPackageStage::Global)
	{
		return *new (Context.AllocateFastCell(sizeof(VPackage))) VPackage(Context, Name, Capacity, InPackageStage);
	}

private:
	VPackage(FAllocationContext Context, VUTF8String& Name, uint32 Capacity, EPackageStage InPackageStage)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
		, PackageName(Context, &Name)
		, Map(Context, Capacity)
		, UPackageMap(Context, 0)
		, PackageType(FPackageName::GetPackageType(StringCast<TCHAR>(Name.AsCString()).Get()))
	{
	}

	UPackage* GetUPackageInternal(FUtf8StringView FilteredQualifiedClassName) const;
	COREUOBJECT_API UPackage* CreateUPackage(FAllocationContext Context, const TCHAR* QualifiedClassName, FUtf8StringView FilteredQualifiedClassName);

	TWriteBarrier<VUTF8String> PackageName;
	VNameValueMap Map;
	VNameValueMap UPackageMap;
	EPackageType PackageType;
	EPackageStage PackageStage;
};
} // namespace Verse
#endif // WITH_VERSE_VM
