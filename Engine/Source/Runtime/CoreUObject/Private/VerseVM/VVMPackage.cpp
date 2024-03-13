// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMPackage.h"
#include "Containers/AnsiString.h"
#include "Containers/UnrealString.h"
#include "Containers/Utf8String.h"
#include "Templates/Casts.h"
#include "UObject/Package.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMMarkStackVisitorInline.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VPackage);
TGlobalTrivialEmergentTypePtr<&VPackage::StaticCppClassInfo> VPackage::GlobalTrivialEmergentType;

UPackage* VPackage::GetUPackage(const TCHAR* QualifiedClassName) const
{
	auto FilteredQualifiedClassName = StringCast<UTF8CHAR>(PackageType == EPackageType::VNI ? TEXT("") : QualifiedClassName);
	return GetUPackageInternal(FilteredQualifiedClassName);
}

UPackage* VPackage::GetOrCreateUPackage(FAllocationContext Context, const TCHAR* QualifiedClassName)
{
	auto FilteredQualifiedClassName = StringCast<UTF8CHAR>(PackageType == EPackageType::VNI ? TEXT("") : QualifiedClassName);
	UPackage* Package = GetUPackageInternal(FilteredQualifiedClassName);
	if (Package == nullptr)
	{
		Package = CreateUPackage(Context, QualifiedClassName, FilteredQualifiedClassName);
	}
	return Package;
}

void VPackage::SetStage(EPackageStage InPackageStage)
{
	if (PackageStage == InPackageStage)
	{
		return;
	}
	PackageStage = InPackageStage;
	for (uint32 Index = UPackageMap.Num(); Index-- > 0;)
	{
		const VUTF8String& QualifiedClassName = UPackageMap.GetName(Index);
		VValue PackageValue = UPackageMap.GetValue(Index);
		if (PackageValue.IsUObject())
		{
			UPackage* Package = Cast<UPackage>(PackageValue.AsUObject());
			Package->Rename(*GetUPackageName(StringCast<TCHAR>(QualifiedClassName.AsCString()).Get(), PackageStage));
		}
	}
}

UPackage* VPackage::GetUPackageInternal(FUtf8StringView FilteredQualifiedClassName) const
{
	VValue PackageValue = UPackageMap.Lookup(FilteredQualifiedClassName);
	return PackageValue.IsUObject() ? Cast<UPackage>(PackageValue.AsUObject()) : nullptr;
}

UPackage* VPackage::CreateUPackage(FAllocationContext Context, const TCHAR* QualifiedClassName, FUtf8StringView FilteredQualifiedClassName)
{
	ensure(GetUPackageInternal(FilteredQualifiedClassName) == nullptr);
	UPackage* Package = CreatePackage(*GetUPackageName(QualifiedClassName, PackageStage));
	UPackageMap.AddValue(Context, FilteredQualifiedClassName, VValue(Package));
	return Package;
}

FString VPackage::GetUPackageName(const TCHAR* QualifiedClassName, EPackageStage Stage, EPackageType* OutPackageType) const
{
	return FPackageName::GetUClassPackagePath(StringCast<TCHAR>(PackageName->AsCString()).Get(), QualifiedClassName, Stage, OutPackageType);
}

template <typename TVisitor>
void VPackage::VisitReferencesImpl(TVisitor& Visitor)
{
	Map.Visit(Visitor, TEXT("DefinitionMap"));
	Visitor.Visit(DigestCode[(int)EDigestVariant::PublicAndEpicInternal], TEXT("PublicAndEpicInternalDigest"));
	Visitor.Visit(DigestCode[(int)EDigestVariant::PublicOnly], TEXT("PublicOnlyDigest"));
	Visitor.Visit(PackageName, TEXT("PackageName"));
	UPackageMap.Visit(Visitor, TEXT("UPackageMap"));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
