// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Utf8String.h"
#include "CoreMinimal.h"
#include "Misc/StringBuilder.h"
#include "VerseVM/VVMPackageTypes.h"

namespace Verse::Names
{
static constexpr int32 DefaultNameLength = 64;

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetDecoratedName(TStringView<CharType> Path, TStringView<CharType> Module, TStringView<CharType> Name)
{
	if (!Module.IsEmpty())
	{
		return TStringBuilderWithBuffer<CharType, DefaultNameLength>(InPlace, '(', Path, '/', Module, ":)", Name);
	}
	else
	{
		return TStringBuilderWithBuffer<CharType, DefaultNameLength>(InPlace, '(', Path, ":)", Name);
	}
}

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetDecoratedName(TStringView<CharType> Path, TStringView<CharType> Name)
{
	return GetDecoratedName(Path, TStringView<CharType>(), Name);
}

#define UE_MAKE_CONSTANT_STRING_METHODS(Name, Text)   \
	template <typename CharType>                      \
	const CharType* Get##Name();                      \
	template <>                                       \
	FORCEINLINE const UTF8CHAR* Get##Name<UTF8CHAR>() \
	{                                                 \
		return UTF8TEXT(Text);                        \
	}                                                 \
	template <>                                       \
	FORCEINLINE const TCHAR* Get##Name<TCHAR>()       \
	{                                                 \
		return TEXT(Text);                            \
	}

UE_MAKE_CONSTANT_STRING_METHODS(VerseSubPath, "_Verse")
UE_MAKE_CONSTANT_STRING_METHODS(VniSubPath, "VNI")
UE_MAKE_CONSTANT_STRING_METHODS(AssetsSubPath, "Assets")
UE_MAKE_CONSTANT_STRING_METHODS(AssetsSubPathForPackageName, "Assets")
UE_MAKE_CONSTANT_STRING_METHODS(PublishedPackageNameSuffix, "-Published")

#undef UE_MAKE_CONSTANT_STRING_METHODS

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetVersePackageNameForVni(TStringView<CharType> MountPointName, TStringView<CharType> CppModuleName)
{
	return TStringBuilderWithBuffer<CharType, DefaultNameLength>(InPlace, MountPointName, '/', CppModuleName);
}

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetVersePackageNameForContent(TStringView<CharType> MountPointName)
{
	return TStringBuilderWithBuffer<CharType, DefaultNameLength>(InPlace, MountPointName);
}

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetVersePackageNameForPublishedContent(TStringView<CharType> MountPointName)
{
	return TStringBuilderWithBuffer<CharType, DefaultNameLength>(InPlace, MountPointName, GetPublishedPackageNameSuffix<CharType>());
}

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetVersePackageNameForAssets(TStringView<CharType> MountPointName)
{
	return TStringBuilderWithBuffer<CharType, DefaultNameLength>(InPlace, MountPointName, '/', GetAssetsSubPathForPackageName<CharType>());
}

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetVersePackageDirForContent(TStringView<CharType> MountPointName)
{
	return TStringBuilderWithBuffer<CharType, DefaultNameLength>(InPlace, '/', MountPointName, '/', GetVerseSubPath<CharType>(), '/');
}

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetVersePackageDirForAssets(TStringView<CharType> MountPointName)
{
	return TStringBuilderWithBuffer<CharType, DefaultNameLength>(InPlace, '/', MountPointName, '/', GetVerseSubPath<CharType>(), '/', GetAssetsSubPath<CharType>(), '/');
}

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetUClassPackagePathForVni(TStringView<CharType> MountPointName, TStringView<CharType> CppModuleName)
{
	return TStringBuilderWithBuffer<CharType, DefaultNameLength>(InPlace, '/', MountPointName, '/', GetVerseSubPath<CharType>(), '/', GetVniSubPath<CharType>(), '/', CppModuleName);
}

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetUClassPackagePathForContent(TStringView<CharType> MountPointName, TStringView<CharType> QualifiedClassName)
{
	return TStringBuilderWithBuffer<CharType, DefaultNameLength>(InPlace, '/', MountPointName, '/', GetVerseSubPath<CharType>(), '/', QualifiedClassName);
}

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetUClassPackagePathForAssets(TStringView<CharType> MountPointName, TStringView<CharType> QualifiedClassName)
{
	return TStringBuilderWithBuffer<CharType, DefaultNameLength>(InPlace, '/', MountPointName, '/', GetVerseSubPath<CharType>(), '/', GetAssetsSubPath<CharType>(), '/', QualifiedClassName);
}

template <typename CharType>
TStringBuilderWithBuffer<CharType, DefaultNameLength> GetUClassPackagePath(TStringView<CharType> VersePackageName, TStringView<CharType> QualifiedClassName, EVersePackageType* OutPackageType = nullptr)
{
	ensure(!QualifiedClassName.IsEmpty()); // Must not be the empty string

	// Ast package names are either
	// "<plugin_name>" for the content Verse package in a plugin, or
	// "<plugin_name>/<vni_module_name>" for VNI Verse packages inside plugins
	// "<plugin_name>/Assets" for reflected assets Verse packages inside plugins

	// Is this a VNI or assets package?
	const CharType* Slash = TCString<CharType>::Strchr(VersePackageName.GetData(), CharType('/'));
	if (Slash)
	{
		// Assets or VNI?
		if (TCString<CharType>::Strcmp(Slash + 1, GetAssetsSubPathForPackageName<CharType>()) == 0)
		{
			// Assets, each class is stored in its own UPackage
			if (OutPackageType)
			{
				*OutPackageType = EVersePackageType::Assets;
			}
			return GetUClassPackagePathForAssets(VersePackageName.Left(int32(Slash - VersePackageName.GetData())), QualifiedClassName);
		}

		// VNI: All VNI classes are combined in a single UPackage with the name of the UBT module
		if (OutPackageType)
		{
			*OutPackageType = EVersePackageType::VNI;
		}
		return GetUClassPackagePathForVni(VersePackageName.Left(int32(Slash - VersePackageName.GetData())), TStringView<CharType>(Slash + 1));
	}

	// No, each class is stored in its own UPackage
	if (OutPackageType)
	{
		*OutPackageType = EVersePackageType::Content;
	}
	TString<CharType> ContentClassName(QualifiedClassName);
	ContentClassName.ReplaceCharInline(CharType('.'), CharType('_'), ESearchCase::CaseSensitive);
	return GetUClassPackagePathForContent(VersePackageName, TStringView<CharType>(ContentClassName));
}

} // namespace Verse::Names
