// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMContentBrowserIntegration.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "CoreGlobals.h"
#include "DMTextureSet.h"
#include "DMTextureSetBlueprintFunctionLibrary.h"
#include "DMTextureSetContentBrowserIntegration.h"
#include "Engine/Texture.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDynamicMaterialEditorModule.h"
#include "Material/DynamicMaterialInstance.h"
#include "Material/DynamicMaterialInstanceFactory.h"
#include "Materials/Material.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "ScopedTransaction.h"
#include "Styling/SlateIconFinder.h"

#define LOCTEXT_NAMESPACE "FDMContentBrowserIntegration"

FDelegateHandle FDMContentBrowserIntegration::PopulateHandle;

void FDMContentBrowserIntegration::Integrate()
{
	if (PopulateHandle.IsValid())
	{
		Disintegrate();
	}

	PopulateHandle = FDMTextureSetContentBrowserIntegration::GetPopulateExtenderDelegate().AddStatic(&FDMContentBrowserIntegration::ExtendMenu);
}

void FDMContentBrowserIntegration::Disintegrate()
{
	if (!PopulateHandle.IsValid())
	{
		return;
	}

	FDMTextureSetContentBrowserIntegration::GetPopulateExtenderDelegate().Remove(PopulateHandle);

	PopulateHandle.Reset();
}

void FDMContentBrowserIntegration::ExtendMenu(FMenuBuilder& InMenuBuilder, const TArray<FAssetData>& InSelectedAssets)
{
	InMenuBuilder.AddMenuEntry(
		LOCTEXT("CreateMaterialDesignerInstanceFromTextureSet", "Create Material Designer Instance"),
		LOCTEXT("CreateMaterialDesignerInstanceFromTextureSetTooltip", "Creates a Material Designer Instance in the content browser using a Texture Set."),
		FSlateIconFinder::FindIconForClass(UMaterial::StaticClass()),
		FUIAction(FExecuteAction::CreateStatic(&FDMContentBrowserIntegration::CreateMaterialDesignerInstanceFromTextureSet, InSelectedAssets))
	);

	InMenuBuilder.AddMenuEntry(
		LOCTEXT("UpdateMaterialDesignerInstanceFromTextureSetAdd", "Update Material Designer Instance (Add)"),
		LOCTEXT("UpdateMaterialDesignerInstanceFromTextureAddSetTooltip", "Updates the opened Material Designer Instance using a Texture Set, adding new layers to the Model."),
		FSlateIconFinder::FindIconForClass(UMaterial::StaticClass()),
		FUIAction(FExecuteAction::CreateStatic(&FDMContentBrowserIntegration::UpdateMaterialDesignerInstanceFromTextureSet, InSelectedAssets, /* Replace */ false))
	);

	InMenuBuilder.AddMenuEntry(
		LOCTEXT("UpdateMaterialDesignerInstanceFromTextureSetReplace", "Update Material Designer Instance (Replace)"),
		LOCTEXT("UpdateMaterialDesignerInstanceFromTextureSetReplaceTooltip", "Updates the opened Material Designer Instance using a Texture Set, replacing slots in the Model."),
		FSlateIconFinder::FindIconForClass(UMaterial::StaticClass()),
		FUIAction(FExecuteAction::CreateStatic(&FDMContentBrowserIntegration::UpdateMaterialDesignerInstanceFromTextureSet, InSelectedAssets, /* Replace */ true))
	);
}

void FDMContentBrowserIntegration::CreateMaterialDesignerInstanceFromTextureSet(TArray<FAssetData> InSelectedAssets)
{
	if (InSelectedAssets.IsEmpty())
	{
		return;
	}

	UDMTextureSetBlueprintFunctionLibrary::CreateTextureSetFromAssetsInteractive(
		InSelectedAssets,
		FDMTextureSetBuilderOnComplete::CreateStatic(
			&FDMContentBrowserIntegration::OnCreateMaterialDesignerInstanceFromTextureSetComplete,
			InSelectedAssets[0].PackagePath.ToString()
		)
	);
}

void FDMContentBrowserIntegration::OnCreateMaterialDesignerInstanceFromTextureSetComplete(UDMTextureSet* InTextureSet, bool bInAccepted,
	FString InPath)
{
	if (!InTextureSet || !bInAccepted)
	{
		return;
	}

	UDynamicMaterialInstance* Instance = Cast<UDynamicMaterialInstance>(GetMutableDefault<UDynamicMaterialInstanceFactory>()->FactoryCreateNew(
		UDynamicMaterialInstance::StaticClass(),
		GetTransientPackage(),
		NAME_None,
		RF_Transactional,
		/* Context */ nullptr,
		GWarn
	));

	if (!Instance)
	{
		return;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(Instance);

	if (!EditorOnlyData)
	{
		return;
	}

	if (!EditorOnlyData->AddTextureSet(InTextureSet, /* Replace */ true))
	{
		return;
	}

	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	FString UniquePackageName;
	FString UniqueAssetName;

	const FString BasePackageName = InPath / TEXT("MDI_NewMaterial");
	AssetToolsModule.Get().CreateUniqueAssetName(BasePackageName, TEXT(""), UniquePackageName, UniqueAssetName);

	UPackage* Package = CreatePackage(*UniquePackageName);

	if (!Package)
	{
		return;
	}

	Instance->SetFlags(RF_Standalone | RF_Public);
	Instance->Rename(*UniqueAssetName, Package, REN_DontCreateRedirectors);

	FAssetRegistryModule::AssetCreated(Instance);
}

void FDMContentBrowserIntegration::UpdateMaterialDesignerInstanceFromTextureSet(TArray<FAssetData> InSelectedAssets, bool bInReplace)
{
	if (InSelectedAssets.IsEmpty())
	{
		return;
	}

	UDMTextureSetBlueprintFunctionLibrary::CreateTextureSetFromAssetsInteractive(
		InSelectedAssets,
		FDMTextureSetBuilderOnComplete::CreateStatic(
			&FDMContentBrowserIntegration::OnUpdateMaterialDesignerInstanceFromTextureSetComplete,
			bInReplace
		)
	);
}

void FDMContentBrowserIntegration::OnUpdateMaterialDesignerInstanceFromTextureSetComplete(UDMTextureSet* InTextureSet, bool bInAccepted,
	bool bInReplace)
{
	if (!InTextureSet || !bInAccepted)
	{
		return;
	}

	IDynamicMaterialEditorModule& DynamicMaterialEditorModule = IDynamicMaterialEditorModule::Get();

	UDynamicMaterialModel* Model = DynamicMaterialEditorModule.GetOpenedMaterialModel(nullptr);

	if (!Model)
	{
		return;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(Model);

	if (!EditorOnlyData)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("AddTextureSet", "Material Designer Add Texture Set"));
	EditorOnlyData->Modify();

	const bool bSuccess = EditorOnlyData->AddTextureSet(InTextureSet, bInReplace);

	if (!bSuccess)
	{
		Transaction.Cancel();
	}
}

#undef LOCTEXT_NAMESPACE
