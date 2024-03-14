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

	// @TODO: SOL-997, this flag will need to be cleared for cooked assets
	Package->SetPackageFlags(PKG_InMemoryOnly);

	// @TODO: SOL-1175, this works around a crash when cooking any game on any platform.  During cooking, the Event Driven Loader (EDL)
	// records when UObjects are requested for load (ENotifyRegistrationPhase::NRP_Added), when they're started (ENotifyRegistrationPhase::NRP_Started),
	// and then when they're finished.  Using this information, it tries to order loading for maximum efficiency.  UClasses are special, because they have
	// an associated CDO with them.  For Blueprint classes, the CDO is always ahead of its associated class in the linker table, meaning it is loaded first (NRP_Added),
	// and then the UClass is loaded (NRP_Added).  When loading the UClass, it calls CreateDefaultObject(), which then attempts to serialize the CDO manually (NRP_Started).
	//
	// For Verse classes currently, we generate these classes and their CDOs at runtime.  So, the UClass gets created, and then it runs CreateDefaultObject().  Unlike the
	// Blueprint case, the CDO hasn't been loaded because the class didn't exist on disk.  So, when UClass::CreateDefaultObject() tries to note the loading for EDL (with NRP_Started),
	// the EDL code asserts because the NRP_Started event for the CDO happened before NRP_Added.  Native classes get around this in their binding code, where a struct helper manually
	// fires the EDL events for NRP_Added for both the Class and the CDO.  Since Verse classes are not necessarily native, but are generated at runtime like native classes, we
	// bypass the event behaviour with this package flag.
	//
	// This probably has ramifications for cooked games, and should be revisited when Verse supports cooked projects.  In the meantime, this is a surgical fix to prevent crashes
	// and allow runtime classes to exist.  Note that EDL is strictly contained in CoreUObject, because nothing should ever mess with it.  UClasses are the exception in UObjects,
	// but since they exist in CoreUObject unlike Verse class, we decided not to expose the EDL notifications publicly.  The intent is that you should never have to mess with it.
	Package->SetPackageFlags(PKG_RuntimeGenerated);

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
