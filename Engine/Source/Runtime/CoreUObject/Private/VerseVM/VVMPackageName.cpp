// Copyright Epic Games, Inc. All Rights Reserved.

#include "VerseVM/VVMPackageName.h"
#include "UObject/Object.h"
#include "VerseVM/VVMPackageTypes.h"

namespace Verse
{

FString FPackageName::GetVersePackageNameForVni(const TCHAR* MountPointName, const TCHAR* CppModuleName)
{
	return FString::Format(TEXT("{0}/{1}"), {MountPointName, CppModuleName});
}

FString FPackageName::GetVersePackageNameForContent(const TCHAR* MountPointName)
{
	return FString(MountPointName);
}

FString FPackageName::GetVersePackageNameForPublishedContent(const TCHAR* MountPointName)
{
	return FString::Format(TEXT("{0}{1}"), {MountPointName, PublishedPackageNameSuffix});
}

FString FPackageName::GetVersePackageNameForAssets(const TCHAR* MountPointName)
{
	return FString::Format(TEXT("{0}/{1}"), {MountPointName, AssetsSubPathForPackageName});
}

FString FPackageName::GetVersePackageDirForContent(const TCHAR* MountPointName)
{
	return FString::Format(TEXT("/{0}/{1}/"), {MountPointName, VerseSubPath});
}

FString FPackageName::GetVersePackageDirForAssets(const TCHAR* MountPointName)
{
	return FString::Format(TEXT("/{0}/{1}/{2}/"), {MountPointName, VerseSubPath, AssetsSubPath});
}

FString FPackageName::GetUClassPackagePathForVni(const TCHAR* MountPointName, const TCHAR* CppModuleName)
{
	return FString::Format(TEXT("/{0}/{1}/{2}/{3}"), {MountPointName, VerseSubPath, VniSubPath, CppModuleName});
}

FString FPackageName::GetUClassPackagePathForContent(const TCHAR* MountPointName, const TCHAR* QualifiedClassName)
{
	return FString::Format(TEXT("/{0}/{1}/{2}"), {MountPointName, VerseSubPath, QualifiedClassName});
}

FString FPackageName::GetUClassPackagePathForAssets(const TCHAR* MountPointName, const TCHAR* QualifiedClassName)
{
	return FString::Format(TEXT("/{0}/{1}/{2}/{3}"), {MountPointName, VerseSubPath, AssetsSubPath, QualifiedClassName});
}

FString FPackageName::GetUClassPackagePath(const TCHAR* VersePackageName, const TCHAR* QualifiedClassName, EVersePackageType* OutPackageType /* = nullptr */)
{
	ensure(QualifiedClassName[0] != 0); // Must not be the empty string

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
				*OutPackageType = EVersePackageType::Assets;
			}
			return GetUClassPackagePathForAssets(*FString::ConstructFromPtrSize(VersePackageName, int32(Slash - VersePackageName)), QualifiedClassName);
		}

		// VNI: All VNI classes are combined in a single UPackage with the name of the UBT module
		if (OutPackageType)
		{
			*OutPackageType = EVersePackageType::VNI;
		}
		return GetUClassPackagePathForVni(*FString::ConstructFromPtrSize(VersePackageName, int32(Slash - VersePackageName)), Slash + 1);
	}

	// No, each class is stored in its own UPackage
	if (OutPackageType)
	{
		*OutPackageType = EVersePackageType::Content;
	}
	return GetUClassPackagePathForContent(VersePackageName, *FString(QualifiedClassName).Replace(TEXT("."), TEXT("_")));
}

FName FPackageName::GetVersePackageNameFromUClassPackagePath(FName UClassPackagePath, EVersePackageType* OutPackageType /* = nullptr */)
{
	FString Path = UClassPackagePath.ToString();
	const TCHAR* Ch = *Path;

	auto ParsePart = [&Ch]() {
		if (*Ch != '/')
		{
			return FString();
		}
		const TCHAR* Begin = ++Ch;
		while (*Ch && *Ch != '/')
		{
			++Ch;
		}
		return FString::ConstructFromPtrSize(Begin, int32(Ch - Begin));
	};

	FString ParsedMountPointName = ParsePart();
	FString ParsedVerseSubPath = ParsePart();
	FString ParsedVniSubPath = ParsePart();
	FString ParsedCppModuleName = ParsePart();

	if (ParsedMountPointName.Len() == 0 || ParsedVerseSubPath != VerseSubPath)
	{
		return NAME_None;
	}

	// Is this a VNI package?
	if (ParsedVniSubPath == VniSubPath && ParsedCppModuleName.Len() > 0)
	{
		// Yes, all VNI classes are combined in a single UPackage with the name of the UBT module
		if (OutPackageType)
		{
			*OutPackageType = EVersePackageType::VNI;
		}
		return FName(ParsedMountPointName / ParsedCppModuleName);
	}

	// Is this an assets package?
	if (ParsedVniSubPath == AssetsSubPath && ParsedCppModuleName.Len() > 0)
	{
		// Yes, each class is stored in its own UPackage
		if (OutPackageType)
		{
			*OutPackageType = EVersePackageType::Assets;
		}
		return FName(ParsedMountPointName / AssetsSubPathForPackageName);
	}

	// Is this a content package?
	if (ParsedVniSubPath.Len() > 0 && ParsedCppModuleName.Len() == 0)
	{
		// Yes, each class is stored in its own UPackage
		if (OutPackageType)
		{
			*OutPackageType = EVersePackageType::Content;
		}
		return FName(ParsedMountPointName);
	}

	return NAME_None;
}

FString FPackageName::GetMountPointName(const TCHAR* VersePackageName)
{
	const TCHAR* Slash = FCString::Strchr(VersePackageName, '/');
	return Slash ? FString::ConstructFromPtrSize(VersePackageName, int32(Slash - VersePackageName)) : FString(VersePackageName);
}

FName FPackageName::GetCppModuleName(const TCHAR* VersePackageName)
{
	const TCHAR* Slash = FCString::Strchr(VersePackageName, '/');
	return Slash ? FName(Slash + 1) : FName();
}

EVersePackageType FPackageName::GetPackageType(const TCHAR* VersePackageName)
{
	// Is this a VNI or assets package?
	const TCHAR* Slash = FCString::Strchr(VersePackageName, '/');
	if (Slash)
	{
		// Assets or VNI?
		if (FCString::Strcmp(Slash + 1, AssetsSubPathForPackageName) == 0)
		{
			return EVersePackageType::Assets;
		}

		return EVersePackageType::VNI;
	}

	if (FStringView(VersePackageName).EndsWith(PublishedPackageNameSuffix))
	{
		return EVersePackageType::PublishedContent;
	}

	// No, each class is stored in its own UPackage
	return EVersePackageType::Content;
}

EVersePackageType FPackageName::GetPackageType(const UTF8CHAR* VersePackageName)
{
	// Is this a VNI or assets package?
	const UTF8CHAR* Slash = FCStringUtf8::Strchr(VersePackageName, UTF8CHAR('/'));
	if (Slash)
	{
		// Assets or VNI?
		if (FCStringUtf8::Strcmp(Slash + 1, (const UTF8CHAR*)AssetsSubPathForPackageNameUTF8) == 0)
		{
			return EVersePackageType::Assets;
		}

		return EVersePackageType::VNI;
	}

	if (FUtf8StringView(VersePackageName).EndsWith(PublishedPackageNameSuffixUTF8))
	{
		return EVersePackageType::PublishedContent;
	}

	// No, each class is stored in its own UPackage
	return EVersePackageType::Content;
}

FString FPackageName::GetTaskUClassName(const TCHAR* OwnerScopeName, const TCHAR* DecoratedAndMangledFunctionName)
{
	//! Must match NativeInterfaceWriter.cpp in GetTaskUClassName()
	return FString::Printf(TEXT("%s%s$%s"), TaskUClassPrefix, OwnerScopeName, DecoratedAndMangledFunctionName);
}

FString FPackageName::GetTaskUClassName(const UObject& OwnerScope, const TCHAR* DecoratedAndMangledFunctionName)
{
	return GetTaskUClassName(*OwnerScope.GetName(), DecoratedAndMangledFunctionName);
}

bool FPackageName::PackageRequiresInternalAPI(const char* Name, const EVersePackageScope VerseScope)
{
	return VerseScope == EVersePackageScope::InternalUser && GetPackageType(reinterpret_cast<const UTF8CHAR*>(Name)) != EVersePackageType::Assets;
}

} // namespace Verse
