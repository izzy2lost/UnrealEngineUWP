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
		return GEditor->GetEditorSubsystem<UAssetReferencingPolicySubsystem>()->ShouldValidateAssetReferences(AssetData);
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
    UAssetReferencingPolicySubsystem* Subsystem = GEditor->GetEditorSubsystem<UAssetReferencingPolicySubsystem>();
	TValueOrError<void, TArray<FAssetReferenceError>> Result = Subsystem->ValidateAssetReferences(InAssetData);
	if (Result.HasError())
	{
		for (const FAssetReferenceError& Error : Result.GetError())
		{
			AssetMessage(InAssetData, EMessageSeverity::Error, Error.Message)->AddToken(FAssetDataToken::Create(Error.ReferencedAsset));
		}
	}
}

#undef LOCTEXT_NAMESPACE
