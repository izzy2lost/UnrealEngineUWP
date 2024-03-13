// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

namespace Verse
{

enum class EPackageStage : uint8
{
	Global,
	Temp,
	Dead
};

enum class EPackageType : uint8
{
	VNI,              // A package associated with a C++ module
	Content,          // A package associated with a plugin
	PublishedContent, // A package associated with the published content of a plugin
	Assets            // A package associated with the reflected binary assets of a plugin
};

/**
 * Functionality for handling Verse UPackages
 */
class COREUOBJECT_API FPackageName
{
public:
	using EPackageType = Verse::EPackageType;

	static FString GetUClassPackagePath(const TCHAR* VersePackageName, const TCHAR* QualifiedClassName, EPackageType* OutPackageType = nullptr);
	static FString GetUClassPackagePath(const TCHAR* VersePackageName, const TCHAR* QualifiedClassName, EPackageStage PackageStage, EPackageType* OutPackageType = nullptr);
	static FString GetUClassPackagePathForVni(const TCHAR* MountPointName, const TCHAR* CppModuleName);
	static FString GetUClassPackagePathForVni(const TCHAR* MountPointName, const TCHAR* CppModuleName, EPackageStage PackageStage);
	static FString GetUClassPackagePathForContent(const TCHAR* MountPointName, const TCHAR* QualifiedClassName);
	static FString GetUClassPackagePathForContent(const TCHAR* MountPointName, const TCHAR* QualifiedClassName, EPackageStage PackageStage);
	static FString GetUClassPackagePathForAssets(const TCHAR* MountPointName, const TCHAR* QualifiedClassName);
	static FString GetUClassPackagePathForAssets(const TCHAR* MountPointName, const TCHAR* QualifiedClassName, EPackageStage PackageStage);
	static EPackageType GetPackageType(const TCHAR* VersePackageName);

	// Constants used for package paths of compiled Verse code
	static constexpr TCHAR const* const VerseSubPath = TEXT("_Verse");
	static constexpr TCHAR const* const VniSubPath = TEXT("VNI");
	static constexpr TCHAR const* const AssetsSubPath = TEXT("Assets");

	// Constants used for package paths of Verse package names

	static constexpr TCHAR const* const AssetsSubPathForPackageName = TEXT("Assets");
	static constexpr char const* const AssetsSubPathForPackageNameUTF8 = "Assets";

	static constexpr TCHAR const PublishedPackageNameSuffix[] = TEXT("-Published");
	static constexpr char const PublishedPackageNameSuffixUTF8[] = "-Published";

	// Class name substitute for root module classes of a package
	static constexpr char const* const RootModuleClassName = "_Root"; // Keep in sync with RootModuleClassName in NativeInterfaceWriter.cpp
};

inline FString FPackageName::GetUClassPackagePath(const TCHAR* VersePackageName, const TCHAR* QualifiedClassName, FPackageName::EPackageType* OutPackageType /* = nullptr */)
{
	return GetUClassPackagePath(VersePackageName, QualifiedClassName, EPackageStage::Global, OutPackageType);
}

inline FString FPackageName::GetUClassPackagePathForVni(const TCHAR* MountPointName, const TCHAR* CppModuleName)
{
	return GetUClassPackagePathForVni(MountPointName, CppModuleName, EPackageStage::Global);
}

inline FString FPackageName::GetUClassPackagePathForContent(const TCHAR* MountPointName, const TCHAR* QualifiedClassName)
{
	return GetUClassPackagePathForContent(MountPointName, QualifiedClassName, EPackageStage::Global);
}

inline FString FPackageName::GetUClassPackagePathForAssets(const TCHAR* MountPointName, const TCHAR* QualifiedClassName)
{
	return GetUClassPackagePathForAssets(MountPointName, QualifiedClassName, EPackageStage::Global);
}

} // namespace Verse