// Copyright Epic Games, Inc. All Rights Reserved.

#include "VerseVM/VVMPackageName.h"
#include "Misc/Paths.h"
#include "UObject/Object.h"

namespace Verse
{
namespace Private
{
uint32 DeadPackageNameNumber = 0;
}

FString FPackageName::GetUClassPackagePathForVni(const TCHAR* MountPointName, const TCHAR* CppModuleName, EPackageStage PackageStage)
{
	switch (PackageStage)
	{
		case EPackageStage::Global:
			return FString::Format(TEXT("/{0}/{1}/{2}/{3}"), {MountPointName, VerseSubPath, VniSubPath, CppModuleName});
		case EPackageStage::Temp:
			return FString::Format(TEXT("/{0}/{1}_TEMP/{2}/{3}"), {MountPointName, VerseSubPath, VniSubPath, CppModuleName});
		case EPackageStage::Dead:
			return FString::Format(TEXT("/{0}/{1}_DEAD/{2}/{3}_{4}"), {MountPointName, VerseSubPath, VniSubPath, CppModuleName, ++Private::DeadPackageNameNumber});
		default:
			unimplemented();
			return FString();
	}
}

FString FPackageName::GetUClassPackagePathForContent(const TCHAR* MountPointName, const TCHAR* QualifiedClassName, EPackageStage PackageStage)
{
	switch (PackageStage)
	{
		case EPackageStage::Global:
			return FString::Format(TEXT("/{0}/{1}/{2}"), {MountPointName, VerseSubPath, QualifiedClassName});
		case EPackageStage::Temp:
			return FString::Format(TEXT("/{0}/{1}_TEMP/{2}"), {MountPointName, VerseSubPath, QualifiedClassName});
		case EPackageStage::Dead:
			return FString::Format(TEXT("/{0}/{1}_DEAD/{2}_{4}"), {MountPointName, VerseSubPath, QualifiedClassName, ++Private::DeadPackageNameNumber});
		default:
			unimplemented();
			return FString();
	}
}

FString FPackageName::GetUClassPackagePathForAssets(const TCHAR* MountPointName, const TCHAR* QualifiedClassName, EPackageStage PackageStage)
{
	switch (PackageStage)
	{
		case EPackageStage::Global:
			return FString::Format(TEXT("/{0}/{1}/{2}/{3}"), {MountPointName, VerseSubPath, AssetsSubPath, QualifiedClassName});
		case EPackageStage::Temp:
			return FString::Format(TEXT("/{0}/{1}_TEMP/{2}/{3}"), {MountPointName, VerseSubPath, AssetsSubPath, QualifiedClassName});
		case EPackageStage::Dead:
			return FString::Format(TEXT("/{0}/{1}_DEAD/{2}/{3}_{4}"), {MountPointName, VerseSubPath, AssetsSubPath, QualifiedClassName, ++Private::DeadPackageNameNumber});
		default:
			unimplemented();
			return FString();
	}
}

FString FPackageName::GetUClassPackagePath(const TCHAR* VersePackageName, const TCHAR* QualifiedClassName, EPackageStage PackageStage, EPackageType* OutPackageType /* = nullptr */)
{
	// Ast package names are either
	// "<plugin_name>" for the content Verse package in a plugin, or
	// "<plugin_name>/<vni_module_name>" for VNI Verse packages inside plugins
	// "<plugin_name>/Assets" for reflected assets Verse packages inside plugins

	// Is this a VNI or assets package?
	const TCHAR* Slash = FCString::Strchr(VersePackageName, '/');
	if (Slash)
	{
		// Assets or VNI?
		if (FCString::Strcmp(Slash + 1, AssetsSubPathForPackageName) == 0)
		{
			// Assets, each class is stored in its own UPackage
			if (OutPackageType)
			{
				*OutPackageType = EPackageType::Assets;
			}
			ensure(QualifiedClassName[0] != 0); // Must not be the empty string
			return GetUClassPackagePathForAssets(*FString(int32(Slash - VersePackageName), VersePackageName), QualifiedClassName, PackageStage);
		}

		// VNI: All VNI classes are combined in a single UPackage with the name of the UBT module
		if (OutPackageType)
		{
			*OutPackageType = EPackageType::VNI;
		}
		return GetUClassPackagePathForVni(*FString(int32(Slash - VersePackageName), VersePackageName), Slash + 1, PackageStage);
	}

	// No, each class is stored in its own UPackage
	if (OutPackageType)
	{
		*OutPackageType = EPackageType::Content;
	}
	ensure(QualifiedClassName[0] != 0); // Must not be the empty string
	return GetUClassPackagePathForContent(VersePackageName, *FString(QualifiedClassName).Replace(TEXT("."), TEXT("_")), PackageStage);
}

FPackageName::EPackageType FPackageName::GetPackageType(const TCHAR* VersePackageName)
{
	// Is this a VNI or assets package?
	const TCHAR* Slash = FCString::Strchr(VersePackageName, '/');
	if (Slash)
	{
		// Assets or VNI?
		if (FCString::Strcmp(Slash + 1, AssetsSubPathForPackageName) == 0)
		{
			return EPackageType::Assets;
		}

		return EPackageType::VNI;
	}

	if (FStringView(VersePackageName).EndsWith(PublishedPackageNameSuffix))
	{
		return EPackageType::PublishedContent;
	}

	// No, each class is stored in its own UPackage
	return EPackageType::Content;
}

} // namespace Verse