// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetRegistry/AssetRegistryHelpers.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry.h"
#include "Algo/AnyOf.h"
#include "Blueprint/BlueprintSupport.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/LinkerLoad.h"

#if WITH_EDITOR
#include "Misc/RedirectCollector.h"
#endif

TScriptInterface<IAssetRegistry> UAssetRegistryHelpers::GetAssetRegistry()
{
	return &UAssetRegistryImpl::Get();
}

FAssetData UAssetRegistryHelpers::CreateAssetData(const UObject* InAsset, bool bAllowBlueprintClass /*= false*/)
{
	if (InAsset && InAsset->IsAsset())
	{
		return FAssetData(InAsset, bAllowBlueprintClass);
	}
	else
	{
		return FAssetData();
	}
}

bool UAssetRegistryHelpers::IsValid(const FAssetData& InAssetData)
{
	return InAssetData.IsValid();
}

bool UAssetRegistryHelpers::IsUAsset(const FAssetData& InAssetData)
{
	return InAssetData.IsUAsset();
}

FString UAssetRegistryHelpers::GetFullName(const FAssetData& InAssetData)
{
	return InAssetData.GetFullName();
}

bool UAssetRegistryHelpers::IsRedirector(const FAssetData& InAssetData)
{
	return InAssetData.IsRedirector();
}

FSoftObjectPath UAssetRegistryHelpers::ToSoftObjectPath(const FAssetData& InAssetData) 
{
	return InAssetData.ToSoftObjectPath();
}

UClass* UAssetRegistryHelpers::GetClass(const FAssetData& InAssetData)
{
	return InAssetData.GetClass();
}

UObject* UAssetRegistryHelpers::GetAsset(const FAssetData& InAssetData)
{
	return InAssetData.GetAsset();
}

bool UAssetRegistryHelpers::IsAssetLoaded(const FAssetData& InAssetData)
{
	return InAssetData.IsAssetLoaded();
}

FString UAssetRegistryHelpers::GetExportTextName(const FAssetData& InAssetData)
{
	return InAssetData.GetExportTextName();
}

bool UAssetRegistryHelpers::GetTagValue(const FAssetData& InAssetData, const FName& InTagName, FString& OutTagValue)
{
	return InAssetData.GetTagValue(InTagName, OutTagValue);
}

FARFilter UAssetRegistryHelpers::SetFilterTagsAndValues(const FARFilter& InFilter, const TArray<FTagAndValue>& InTagsAndValues)
{
	FARFilter FilterCopy = InFilter;
	for (const FTagAndValue& TagAndValue : InTagsAndValues)
	{
		FilterCopy.TagsAndValues.Add(TagAndValue.Tag, TagAndValue.Value);
	}

	return FilterCopy;
}

UClass* UAssetRegistryHelpers::FindAssetNativeClass(const FAssetData& AssetData)
{
	UClass* AssetClass = AssetData.GetClass();
	if (AssetClass == nullptr)
	{
		const IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
		
		TArray<FTopLevelAssetPath> AncestorClasses;
		AssetRegistry.GetAncestorClassNames(AssetData.AssetClassPath, AncestorClasses);
		for (const FTopLevelAssetPath& AncestorClassPath : AncestorClasses)
		{
			AssetClass = FindObject<UClass>(AncestorClassPath);
			if (AssetClass)
			{
				break;
			}
		}
	}
	while (AssetClass && !AssetClass->HasAnyClassFlags(CLASS_Native))
	{
		AssetClass = AssetClass->GetSuperClass();
	}

	return AssetClass;
}

void UAssetRegistryHelpers::FindReferencersOfAssetOfClass(UObject* AssetInstance, TConstArrayView<UClass*> InMatchClasses, TArray<FAssetData>& OutAssetDatas)
{
	FindReferencersOfAssetOfClass(AssetInstance->GetOutermost()->GetFName(), InMatchClasses, OutAssetDatas);
}

void UAssetRegistryHelpers::FindReferencersOfAssetOfClass(const FAssetIdentifier& InAssetIdentifier, TConstArrayView<UClass*> InMatchClasses, TArray<FAssetData>& OutAssetDatas)
{
	// If the asset registry is still loading assets, we cant check for referencers, so we must open the rename dialog
	const IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

	TArray<FAssetIdentifier> Referencers;
	AssetRegistry.GetReferencers(InAssetIdentifier, Referencers);

	for (auto AssetIdentifier : Referencers)
	{
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByPackageName(AssetIdentifier.PackageName, Assets);

		for (auto AssetData : Assets)
		{
			if (InMatchClasses.Num() > 0)
			{
				if (Algo::AnyOf(InMatchClasses, [&AssetData](UClass* MatchClass){ return AssetData.IsInstanceOf(MatchClass); }))
				{
					OutAssetDatas.AddUnique(AssetData);
				}
			}
			else
			{
				OutAssetDatas.AddUnique(AssetData);
			}
		}
	}
}

void UAssetRegistryHelpers::GetBlueprintAssets(const FARFilter& InFilter, TArray<FAssetData>& OutAssetData)
{
	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

	FARFilter Filter(InFilter);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	UE_CLOG(!InFilter.ClassNames.IsEmpty(), LogCore, Error,
		TEXT("ARFilter.ClassNames is not supported by UAssetRegistryHelpers::GetBlueprintAssets and will be ignored."));
	Filter.ClassNames.Empty();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;

	// Expand list of classes to include derived classes
	TArray<FTopLevelAssetPath> BlueprintParentClassPathRoots = MoveTemp(Filter.ClassPaths);
	TSet<FTopLevelAssetPath> BlueprintParentClassPaths;
	if (Filter.bRecursiveClasses)
	{
		AssetRegistry.GetDerivedClassNames(BlueprintParentClassPathRoots, TSet<FTopLevelAssetPath>(), BlueprintParentClassPaths);
	}
	else
	{
		BlueprintParentClassPaths.Append(BlueprintParentClassPathRoots);
	}

	// Search for all blueprints and then check BlueprintParentClassPaths in the results
	Filter.ClassPaths.Reset(1);
	Filter.ClassPaths.Add(FTopLevelAssetPath(FName(TEXT("/Script/Engine")), FName(TEXT("BlueprintCore"))));
	Filter.bRecursiveClasses = true;

	auto FilterLambda = [&OutAssetData, &BlueprintParentClassPaths](const FAssetData& AssetData)
	{
		// Verify blueprint class
		if (BlueprintParentClassPaths.IsEmpty() || IsAssetDataBlueprintOfClassSet(AssetData, BlueprintParentClassPaths))
		{
			OutAssetData.Add(AssetData);
		}
		return true;
	};
	AssetRegistry.EnumerateAssets(Filter, FilterLambda);
}

bool UAssetRegistryHelpers::IsAssetDataBlueprintOfClassSet(const FAssetData& AssetData, const TSet<FTopLevelAssetPath>& ClassNameSet)
{
	const FString ParentClassFromData = AssetData.GetTagValueRef<FString>(FBlueprintTags::ParentClassPath);
	if (!ParentClassFromData.IsEmpty())
	{
		const FTopLevelAssetPath ClassObjectPath(FPackageName::ExportTextPathToObjectPath(ParentClassFromData));
		const FName ClassName = ClassObjectPath.GetAssetName();

		TArray<FTopLevelAssetPath> ValidNames;
		ValidNames.Add(ClassObjectPath);
		// Check for redirected name
		FTopLevelAssetPath RedirectedName = FTopLevelAssetPath(FLinkerLoad::FindNewPathNameForClass(ClassObjectPath.ToString(), false));
		if (!RedirectedName.IsNull())
		{
			ValidNames.Add(RedirectedName);
		}
		for (const FTopLevelAssetPath& ValidName : ValidNames)
		{
			if (ClassNameSet.Contains(ValidName))
			{
				// Our parent class is in the class name set
				return true;
			}
		}
	}
	return false;
}

UAssetRegistryHelpers::FTemporaryCachingModeScope::FTemporaryCachingModeScope(bool InTempCachingMode)
{
	PreviousCachingMode = UAssetRegistryHelpers::GetAssetRegistry()->GetTemporaryCachingMode();
	UAssetRegistryHelpers::GetAssetRegistry()->SetTemporaryCachingMode(InTempCachingMode);
}

UAssetRegistryHelpers::FTemporaryCachingModeScope::~FTemporaryCachingModeScope()
{
	UAssetRegistryHelpers::GetAssetRegistry()->SetTemporaryCachingMode(PreviousCachingMode);
}

bool UAssetRegistryHelpers::FixupRedirectedAssetPath(FSoftObjectPath& InOutSoftObjectPath)
{
	if (InOutSoftObjectPath.IsNull())
	{
		// Empty path, no redirect
		return true;
	}

#if WITH_EDITOR
	// Check GRedirectCollector first for faster fixup
	FSoftObjectPath FoundRedirection = GRedirectCollector.GetAssetPathRedirection(InOutSoftObjectPath.GetWithoutSubPath());
	if (!FoundRedirection.IsNull())
	{
		InOutSoftObjectPath.SetPath(FoundRedirection.GetAssetPath(), InOutSoftObjectPath.GetSubPathString());
		return true;
	}
#endif

	if (InOutSoftObjectPath.GetAssetName().IsEmpty())
	{
		// A package name. No need to ask the asset registry for assets, it wont find any
		return true;
	}

	const FAssetData* AssetData;
	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

	FTopLevelAssetPath AssetPath = InOutSoftObjectPath.GetAssetPath();
	TSet<FTopLevelAssetPath> Visited;
	Visited.Add(AssetPath);

	for (;;)
	{
		TArray<FAssetData> Assets;
		AssetRegistry.ScanFilesSynchronous({ AssetPath.GetPackageName().ToString() }, /*bForceRescan*/false);
		AssetRegistry.GetAssetsByPackageName(AssetPath.GetPackageName(), Assets, /*bIncludeOnlyOnDiskAssets*/true);

		if (!Assets.Num())
		{
			UE_LOG(LogCore, Warning, TEXT("Failed to find assets for asset path '%s'"), *AssetPath.ToString());
			return false;
		}

		AssetData = Assets.FindByPredicate([&AssetPath](const FAssetData& AssetData)
		{
			return (AssetData.ToSoftObjectPath().GetAssetPath() == AssetPath);
		});

		if (!AssetData)
		{
			UE_LOG(LogCore, Warning, TEXT("Failed to find asset for asset path '%s'"), *AssetPath.ToString());
			return false;
		}

		if (!AssetData->IsRedirector())
		{
			break;
		}

		FString DestinationObjectPath;
		if (!AssetData->GetTagValue(TEXT("DestinationObject"), DestinationObjectPath))
		{
			UE_LOG(LogCore, Warning, TEXT("Failed to follow redirector for '%s'"), *AssetPath.ToString());
			return false;
		}

		// Update asset path
		AssetPath = FTopLevelAssetPath(DestinationObjectPath);

		bool bAlreadyVisited = false;
		Visited.Add(AssetPath, &bAlreadyVisited);
		if (bAlreadyVisited)
		{
			UE_LOG(LogCore, Warning, TEXT("Asset path was already visited '%s'"), *AssetPath.ToString());
			return false;
		}
	}

	InOutSoftObjectPath.SetPath(AssetPath, InOutSoftObjectPath.GetSubPathString());

	return true;
}

bool UAssetRegistryHelpers::FixupRedirectedAssetPath(FName& InOutAssetPath)
{
	FSoftObjectPath SoftObjectPath(InOutAssetPath.ToString());
	if (FixupRedirectedAssetPath(SoftObjectPath))
	{
		InOutAssetPath = FName(*SoftObjectPath.ToString());
		return true;
	}

	return false;
}
