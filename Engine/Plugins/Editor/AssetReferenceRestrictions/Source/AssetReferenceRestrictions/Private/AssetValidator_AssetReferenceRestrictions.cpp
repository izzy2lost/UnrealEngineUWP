// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetValidator_AssetReferenceRestrictions.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetDataToken.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "AssetReferencingPolicySubsystem.h"
#include "AssetReferencingPolicySettings.h"
#include "AssetReferencingDomains.h"
#include "Editor/AssetReferenceFilter.h"
#include "Misc/PackageName.h"
#include "Misc/DataValidation.h"
#include "Modules/ModuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AssetValidator_AssetReferenceRestrictions)

#define LOCTEXT_NAMESPACE "AssetReferencingPolicy"

UAssetValidator_AssetReferenceRestrictions::UAssetValidator_AssetReferenceRestrictions()
	: Super()
{
}

bool UAssetValidator_AssetReferenceRestrictions::CanValidateAsset_Implementation(const FAssetData& AssetData, UObject* InAsset, FDataValidationContext& InContext) const
{
	if (InAsset)
	{
		TSharedPtr<FDomainData> DomainData = GEditor->GetEditorSubsystem<UAssetReferencingPolicySubsystem>()->GetDomainDB()->FindDomainFromAssetData(AssetData);

		const bool bIsInUnrestrictedFolder = DomainData && DomainData->IsValid() && DomainData->bCanSeeEverything;
		if (!bIsInUnrestrictedFolder)
		{
			return true;
		}
	}

	return false;
}

EDataValidationResult UAssetValidator_AssetReferenceRestrictions::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext)
{
	check(InAsset);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Validate asset references
	ValidateAssetInternal(InAssetData, AssetRegistry);
	
	// Validate each external object's references
	for (const FAssetData& ExternalObject : InContext.GetAssociatedExternalObjects())
	{
		ValidateAssetInternal(ExternalObject, AssetRegistry);
	}
	
	if (GetValidationResult() != EDataValidationResult::Invalid)
	{
		AssetPasses(InAsset);
	}

	return GetValidationResult();
}

void UAssetValidator_AssetReferenceRestrictions::ValidateAssetInternal(const FAssetData& InAssetData, const IAssetRegistry& InAssetRegistry)
{
	static const FName TransientName = GetTransientPackage()->GetFName();

	// Check for missing soft or hard references to cinematic and developers content
	FName PackageFName = InAssetData.PackageName;
	TArray<FName> SoftDependencies;
	TArray<FAssetData> AllDependencyAssets;

	const UAssetReferencingPolicySettings* Settings = GetDefault<UAssetReferencingPolicySettings>();
	UE::AssetRegistry::EDependencyQuery QueryFlags = UE::AssetRegistry::EDependencyQuery::NoRequirements;

	if (Settings->bIgnoreEditorOnlyReferences)
	{
		// If the rules allow ignoring editor-only referencers, restrict the dependencies to game references
		QueryFlags = UE::AssetRegistry::EDependencyQuery::Game;
	}

	InAssetRegistry.GetDependencies(PackageFName, SoftDependencies, UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::Soft | QueryFlags);
	for (FName SoftDependency : SoftDependencies)
	{
		const FString SoftDependencyStr = SoftDependency.ToString();
		if (!FPackageName::IsScriptPackage(SoftDependencyStr) && !FPackageName::IsVersePackage(SoftDependencyStr))
		{
			TArray<FAssetData> DependencyAssets;
			InAssetRegistry.GetAssetsByPackageName(SoftDependency, DependencyAssets, true);
			if (DependencyAssets.Num() == 0)
			{
				if (SoftDependency != TransientName)
				{
					AssetMessage(InAssetData, EMessageSeverity::Error, FText::Format(LOCTEXT("IllegalReference_MissingSoftRef", "Soft references {0} which does not exist"), FText::FromString(SoftDependencyStr)));
				}
			}
			else
			{
				AllDependencyAssets.Append(DependencyAssets);
			}
		}
	}

	// Now check hard references to cinematic and developers content
	TArray<FName> HardDependencies;
	InAssetRegistry.GetDependencies(PackageFName, HardDependencies, UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::Hard | QueryFlags);
	for (FName HardDependency : HardDependencies)
	{
		//@TODO: Probably not needed anymore?
#if 0
		const FString HardDependencyStr = HardDependency.ToString();
		FString UncookedFolderName;
		if (IsInUncookedFolder(HardDependencyStr, &UncookedFolderName))
		{
			AssetMessage(InAssetData, EMessageSeverity::Error, FText::Format(LOCTEXT("IllegalReference_HardDependency", "Illegally hard references {0} asset {1}"), FText::FromString(UncookedFolderName), FText::FromString(HardDependencyStr)));
		}
#endif

		InAssetRegistry.GetAssetsByPackageName(HardDependency, AllDependencyAssets, true);
	}

	if ((GetValidationResult() != EDataValidationResult::Invalid) && (AllDependencyAssets.Num() > 0))
	{
		FAssetReferenceFilterContext AssetReferenceFilterContext;
		AssetReferenceFilterContext.ReferencingAssets = { InAssetData };
		TSharedPtr<IAssetReferenceFilter> AssetReferenceFilter = GEditor ? GEditor->MakeAssetReferenceFilter(AssetReferenceFilterContext) : nullptr;
		if (ensure(AssetReferenceFilter.IsValid()))
		{
			for (const FAssetData& Dependency : AllDependencyAssets)
			{
				FText FailureReason;
				if (!AssetReferenceFilter->PassesFilter(Dependency, &FailureReason))
				{
					AssetMessage(InAssetData, EMessageSeverity::Error, FText::Format(LOCTEXT("IllegalReference_AssetFilterFail", "Illegal reference:"), FailureReason))
						->AddToken(FAssetDataToken::Create(Dependency))
						->AddText(FText::Format(LOCTEXT("IllegalReference_FailureReason", ". {0}"), FailureReason));
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
