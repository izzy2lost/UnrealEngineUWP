// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMTextureSetContentBrowserIntegration.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "CoreGlobals.h"
#include "DMTextureSet.h"
#include "DMTextureSetBlueprintFunctionLibrary.h"
#include "DMTextureSetStyle.h"
#include "Engine/Texture.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDynamicMaterialEditorModule.h"
#include "Material/DynamicMaterialInstance.h"
#include "Material/DynamicMaterialInstanceFactory.h"
#include "Materials/Material.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "ScopedTransaction.h"
#include "Styling/SlateIconFinder.h"

#define LOCTEXT_NAMESPACE "FDMTextureSetContentBrowserIntegration"

FDelegateHandle FDMTextureSetContentBrowserIntegration::ExtenderDelegateHandle;

void FDMTextureSetContentBrowserIntegration::Integrate()
{
	FDMTextureSetStyle::Get();

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
	CBMenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FDMTextureSetContentBrowserIntegration::OnExtendContentBrowserAssetSelectionMenu));
	ExtenderDelegateHandle = CBMenuExtenderDelegates.Last().GetHandle();
}

void FDMTextureSetContentBrowserIntegration::Disintegrate()
{
	if (!ExtenderDelegateHandle.IsValid())
	{
		return;
	}

	if (FContentBrowserModule* ContentBrowserModule = FModuleManager::GetModulePtr<FContentBrowserModule>("ContentBrowser"))
	{
		TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule->GetAllAssetViewContextMenuExtenders();

		CBMenuExtenderDelegates.RemoveAll(
			[](const FContentBrowserMenuExtender_SelectedAssets& InElement)
			{
				return InElement.GetHandle() == ExtenderDelegateHandle;
			}
		);
	}

	ExtenderDelegateHandle.Reset();
}

TSharedRef<FExtender> FDMTextureSetContentBrowserIntegration::OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& InSelectedAssets)
{
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();
	bool bHasTexture = false;

	for (const FAssetData& SelectedAsset : InSelectedAssets)
	{
		if (UClass* AssetClass = SelectedAsset.GetClass(EResolveClass::Yes))
		{
			if (AssetClass->IsChildOf<UTexture>())
			{
				bHasTexture = true;
				break;
			}
		}
	}

	if (!bHasTexture)
	{
		return Extender;
	}

	Extender->AddMenuExtension(
		"GetAssetActions",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateLambda(
			[InSelectedAssets](FMenuBuilder& InMenuBuilder) {
				InMenuBuilder.AddMenuEntry(
					LOCTEXT("CreateTextureSet", "Create Texture Set"),
					LOCTEXT("CreateTextureSetTooltip", "Creates an asset listing a group of textures and the material properties they are associated with."),
					FSlateIconFinder::FindIconForClass(UTexture::StaticClass()),
					FUIAction(FExecuteAction::CreateStatic(&FDMTextureSetContentBrowserIntegration::CreateTextureSet, InSelectedAssets))
				);
			})
	);

#if 0
	Extender->AddMenuExtension(
		"GetAssetActions",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateLambda(
			[InSelectedAssets](FMenuBuilder& InMenuBuilder) {
				InMenuBuilder.AddMenuEntry(
					LOCTEXT("CreateMaterialDesignerInstanceFromTextureSet", "Create Material Designer Instance"),
					LOCTEXT("CreateMaterialDesignerInstanceFromTextureSetTooltip", "Creates a Material Designer Instance in the content browser using a Texture Set."),
					FSlateIconFinder::FindIconForClass(UMaterial::StaticClass()),
					FUIAction(FExecuteAction::CreateStatic(&FDMTextureSetContentBrowserIntegration::CreateMaterialDesignerInstanceFromTextureSet, InSelectedAssets))
				);
			})
	);

	Extender->AddMenuExtension(
		"GetAssetActions",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateLambda(
			[InSelectedAssets](FMenuBuilder& InMenuBuilder) {
				InMenuBuilder.AddMenuEntry(
					LOCTEXT("UpdateMaterialDesignerInstanceFromTextureSetAdd", "Update Material Designer Instance (Add)"),
					LOCTEXT("UpdateMaterialDesignerInstanceFromTextureAddSetTooltip", "Updates the opened Material Designer Instance using a Texture Set, adding new layers to the Model."),
					FSlateIconFinder::FindIconForClass(UMaterial::StaticClass()),
					FUIAction(FExecuteAction::CreateStatic(&FDMTextureSetContentBrowserIntegration::UpdateMaterialDesignerInstanceFromTextureSet, InSelectedAssets, /* Replace */ false))
				);
			})
	);

	Extender->AddMenuExtension(
		"GetAssetActions",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateLambda(
			[InSelectedAssets](FMenuBuilder& InMenuBuilder) {
				InMenuBuilder.AddMenuEntry(
					LOCTEXT("UpdateMaterialDesignerInstanceFromTextureSetReplace", "Update Material Designer Instance (Replace)"),
					LOCTEXT("UpdateMaterialDesignerInstanceFromTextureSetReplaceTooltip", "Updates the opened Material Designer Instance using a Texture Set, replacing slots in the Model."),
					FSlateIconFinder::FindIconForClass(UMaterial::StaticClass()),
					FUIAction(FExecuteAction::CreateStatic(&FDMTextureSetContentBrowserIntegration::UpdateMaterialDesignerInstanceFromTextureSet, InSelectedAssets, /* Replace */ true))
				);
			})
	);
#endif

	return Extender;
}

void FDMTextureSetContentBrowserIntegration::CreateTextureSet(TArray<FAssetData> InSelectedAssets)
{
	if (InSelectedAssets.IsEmpty())
	{
		return;
	}

	UDMTextureSetBlueprintFunctionLibrary::CreateTextureSetFromAssetsInteractive(
		InSelectedAssets,
		FDMTextureSetBuilderOnComplete::CreateStatic(
			&FDMTextureSetContentBrowserIntegration::OnCreateTextureSetComplete,
			InSelectedAssets[0].PackagePath.ToString()
		)
	);
}

void FDMTextureSetContentBrowserIntegration::OnCreateTextureSetComplete(UDMTextureSet* InTextureSet, bool bInAccepted, FString InPath)
{
	if (!InTextureSet || !bInAccepted)
	{
		return;
	}

	const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	FString UniquePackageName;
	FString UniqueAssetName;

	const FString BasePackageName = InPath / TEXT("NewTextureSet");
	AssetToolsModule.Get().CreateUniqueAssetName(BasePackageName, TEXT(""), UniquePackageName, UniqueAssetName);

	UPackage* Package = CreatePackage(*UniquePackageName);

	if (!Package)
	{
		return;
	}

	InTextureSet->SetFlags(RF_Standalone | RF_Public);
	InTextureSet->Rename(*UniqueAssetName, Package, REN_DontCreateRedirectors);

	FAssetRegistryModule::AssetCreated(InTextureSet);
}

void FDMTextureSetContentBrowserIntegration::CreateMaterialDesignerInstanceFromTextureSet(TArray<FAssetData> InSelectedAssets)
{
	if (InSelectedAssets.IsEmpty())
	{
		return;
	}

	UDMTextureSetBlueprintFunctionLibrary::CreateTextureSetFromAssetsInteractive(
		InSelectedAssets,
		FDMTextureSetBuilderOnComplete::CreateStatic(
			&FDMTextureSetContentBrowserIntegration::OnCreateMaterialDesignerInstanceFromTextureSetComplete,
			InSelectedAssets[0].PackagePath.ToString()
		)
	);
}

void FDMTextureSetContentBrowserIntegration::OnCreateMaterialDesignerInstanceFromTextureSetComplete(UDMTextureSet* InTextureSet, bool bInAccepted,
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
		RF_Transient,
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

	const FString BasePackageName = InPath / TEXT("NewMaterialDesignerInstance");
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

void FDMTextureSetContentBrowserIntegration::UpdateMaterialDesignerInstanceFromTextureSet(TArray<FAssetData> InSelectedAssets, bool bInReplace)
{
	if (InSelectedAssets.IsEmpty())
	{
		return;
	}

	UDMTextureSetBlueprintFunctionLibrary::CreateTextureSetFromAssetsInteractive(
		InSelectedAssets,
		FDMTextureSetBuilderOnComplete::CreateStatic(
			&FDMTextureSetContentBrowserIntegration::OnUpdateMaterialDesignerInstanceFromTextureSetComplete,
			bInReplace
		)
	);
}

void FDMTextureSetContentBrowserIntegration::OnUpdateMaterialDesignerInstanceFromTextureSetComplete(UDMTextureSet* InTextureSet, bool bInAccepted,
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
