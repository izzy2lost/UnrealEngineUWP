// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMContentBrowserIntegration.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "CoreGlobals.h"
#include "DMTextureSet.h"
#include "DMTextureSetBlueprintFunctionLibrary.h"
#include "DMTextureSetContentBrowserIntegration.h"
#include "Engine/Texture.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IContentBrowserSingleton.h"
#include "IDynamicMaterialEditorModule.h"
#include "Material/DynamicMaterialInstance.h"
#include "Material/DynamicMaterialInstanceFactory.h"
#include "Materials/Material.h"
#include "Misc/MessageDialog.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelDynamic.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "PackageTools.h"
#include "ScopedTransaction.h"
#include "Styling/SlateIconFinder.h"

#define LOCTEXT_NAMESPACE "FDMContentBrowserIntegration"

FDelegateHandle FDMContentBrowserIntegration::TextureSetPopulateHandle;
FDelegateHandle FDMContentBrowserIntegration::ContentBrowserAssetHandle;

void FDMContentBrowserIntegration::Integrate()
{
	Disintegrate();

	TextureSetPopulateHandle = FDMTextureSetContentBrowserIntegration::GetPopulateExtenderDelegate().AddStatic(&FDMContentBrowserIntegration::ExtendMenu);

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
	CBMenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FDMContentBrowserIntegration::OnExtendContentBrowserAssetSelectionMenu));
	ContentBrowserAssetHandle = CBMenuExtenderDelegates.Last().GetHandle();
}

void FDMContentBrowserIntegration::Disintegrate()
{
	if (TextureSetPopulateHandle.IsValid())
	{
		FDMTextureSetContentBrowserIntegration::GetPopulateExtenderDelegate().Remove(TextureSetPopulateHandle);
		TextureSetPopulateHandle.Reset();
	}

	if (ContentBrowserAssetHandle.IsValid())
	{
		if (FContentBrowserModule* ContentBrowserModule = FModuleManager::GetModulePtr<FContentBrowserModule>("ContentBrowser"))
		{
			TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule->GetAllAssetViewContextMenuExtenders();

			CBMenuExtenderDelegates.RemoveAll(
				[](const FContentBrowserMenuExtender_SelectedAssets& InElement)
				{
					return InElement.GetHandle() == ContentBrowserAssetHandle;
				});

			ContentBrowserAssetHandle.Reset();
		}
	}
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

	EditorOnlyData->SetChannelListPreset("All");

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

	UDynamicMaterialModelBase* Model = DynamicMaterialEditorModule.GetOpenedMaterialModel(nullptr);

	if (!Model)
	{
		return;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(Model);

	if (!EditorOnlyData)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("AddTextureSet", "Add Texture Set"));
	EditorOnlyData->Modify();

	const bool bSuccess = EditorOnlyData->AddTextureSet(InTextureSet, bInReplace);

	if (!bSuccess)
	{
		Transaction.Cancel();
	}
}

TSharedRef<FExtender> FDMContentBrowserIntegration::OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& InSelectedAssets)
{
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();
	bool bHasMaterialDesignerAsset = false;

	for (const FAssetData& SelectedAsset : InSelectedAssets)
	{
		if (UClass* AssetClass = SelectedAsset.GetClass(EResolveClass::Yes))
		{
			if (AssetClass->IsChildOf<UDynamicMaterialInstance>() || AssetClass->IsChildOf<UDynamicMaterialModel>())
			{
				bHasMaterialDesignerAsset = true;
				break;
			}
		}
	}

	if (!bHasMaterialDesignerAsset)
	{
		return Extender;
	}

	Extender->AddMenuExtension(
		"GetAssetActions",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateLambda(
			[InSelectedAssets](FMenuBuilder& InMenuBuilder)
			{
				InMenuBuilder.AddMenuEntry(
					LOCTEXT("CreateDynamic", "Create Material Designer Dynamic"),
					LOCTEXT("CreateDynamicooltip", "Create a dynamic instance from a Material Designer Instance or Model."),
					FSlateIconFinder::FindIconForClass(UMaterial::StaticClass()),
					FUIAction(FExecuteAction::CreateStatic(&FDMContentBrowserIntegration::CreateDynamic, InSelectedAssets))
				);
			}
		)
	);

	return Extender;
}

void FDMContentBrowserIntegration::CreateDynamic(TArray<FAssetData> InSelectedAssets)
{
	for (const FAssetData& SelectedAsset : InSelectedAssets)
	{
		if (UClass* AssetClass = SelectedAsset.GetClass(EResolveClass::Yes))
		{
			if (AssetClass->IsChildOf<UDynamicMaterialModel>())
			{
				CreateModelDynamic(Cast<UDynamicMaterialModel>(SelectedAsset.GetAsset()));
				break;
			}

			if (AssetClass->IsChildOf<UDynamicMaterialInstance>())
			{
				CreateInstanceDynamic(Cast<UDynamicMaterialInstance>(SelectedAsset.GetAsset()));
				break;
			}
		}
	}
}

void FDMContentBrowserIntegration::CreateModelDynamic(UDynamicMaterialModel* InModel)
{
	if (!InModel)
	{
		return;
	}

	UMaterial* ParentMaterial = InModel->GetGeneratedMaterial();

	if (!ParentMaterial)
	{
		return;
	}

	if (!ParentMaterial->HasAnyFlags(RF_Public))
	{
		const EAppReturnType::Type Result = FMessageDialog::Open(
			EAppMsgType::YesNo,
			LOCTEXT("ExportMaterialFromModel", 
				"Generating a Material Designer Dynamic requires that the generated material be exported from its package.\n\n"
				"The package containing the material will be saved. This may be a level.\n\n"
				"Continue?")
		);

		switch (Result)
		{
			case EAppReturnType::Yes:
				ParentMaterial->Modify(/* Always Mark Dirty */ true);
				ParentMaterial->SetFlags(RF_Public);
				UPackageTools::SavePackagesForObjects({InModel});
				break;

			default:
				return;
		}
	}

	UDynamicMaterialModelDynamic* ModelDynamic = UDynamicMaterialModelDynamic::Create(GetTransientPackage(), InModel);

	if (!ModelDynamic)
	{
		return;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	FString PackageName, AssetName;
	AssetTools.CreateUniqueAssetName(InModel->GetName(), TEXT(""), PackageName, AssetName);

	IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
	const FContentBrowserItemPath CurrentPath = ContentBrowser.GetCurrentPath();
	const FString PathStr = CurrentPath.HasInternalPath() ? CurrentPath.GetInternalPathString() : "/Game";

	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
	SaveAssetDialogConfig.DefaultPath = PathStr;
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::Disallow;
	SaveAssetDialogConfig.DefaultAssetName = AssetName;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

	if (SaveObjectPath.IsEmpty())
	{
		return;
	}

	PackageName = FPaths::GetBaseFilename(*SaveObjectPath, false);

	UPackage* Package = CreatePackage(*PackageName);

	if (!Package)
	{
		return;
	}

	AssetName = FPaths::GetBaseFilename(*SaveObjectPath, true);

	ModelDynamic->SetFlags(RF_Standalone | RF_Public);
	ModelDynamic->Rename(*AssetName, Package, REN_DontCreateRedirectors);

	FAssetRegistryModule::AssetCreated(ModelDynamic);
}

void FDMContentBrowserIntegration::CreateInstanceDynamic(UDynamicMaterialInstance* InInstance)
{
	if (!InInstance)
	{
		return;
	}

	UDynamicMaterialModel* Model = InInstance->GetMaterialModel();

	if (!Model)
	{
		return;
	}

	UMaterial* ParentMaterial = Model->GetGeneratedMaterial();

	if (!ParentMaterial)
	{
		return;
	}

	if (!ParentMaterial->HasAnyFlags(RF_Public) || !Model->HasAnyFlags(RF_Public))
	{
		const EAppReturnType::Type Result = FMessageDialog::Open(
			EAppMsgType::YesNo,
			LOCTEXT("ExportMaterialFromInstance",
				"Generating a Material Designer Dynamic requires that the generated material and material model be exported from this package.\n\n"
				"The package containing the material will be saved. This may be a level.\n\n"
				"Continue?")
		);

		switch (Result)
		{
			case EAppReturnType::Yes:
				Model->Modify(/* Always Mark Dirty */ true);
				Model->SetFlags(RF_Public);
				ParentMaterial->Modify(/* Always Mark Dirty */ true);
				ParentMaterial->SetFlags(RF_Public);
				UPackageTools::SavePackagesForObjects({InInstance});
				break;

			default:
				return;
		}
	}

	UDynamicMaterialModelDynamic* ModelDynamic = UDynamicMaterialModelDynamic::Create(GetTransientPackage(), Model);

	if (!ModelDynamic)
	{
		return;
	}

	UDynamicMaterialInstance* Instance = NewObject<UDynamicMaterialInstance>(GetTransientPackage());

	if (!Instance)
	{
		return;
	}

	Instance->SetMaterialModel(ModelDynamic);
	ModelDynamic->SetDynamicMaterialInstance(Instance);
	Instance->InitializeMIDPublic();

	FString CurrentName = InInstance->GetName();
	CurrentName = CurrentName.StartsWith(TEXT("MDI_"))
		? (TEXT("MDD_") + CurrentName.RightChop(4))
		: (TEXT("MDD_") + CurrentName);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	FString PackageName, AssetName;
	AssetTools.CreateUniqueAssetName(CurrentName, TEXT(""), PackageName, AssetName);

	IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
	const FContentBrowserItemPath CurrentPath = ContentBrowser.GetCurrentPath();
	const FString PathStr = CurrentPath.HasInternalPath() ? CurrentPath.GetInternalPathString() : "/Game";

	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
	SaveAssetDialogConfig.DefaultPath = PathStr;
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::Disallow;
	SaveAssetDialogConfig.DefaultAssetName = AssetName;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

	if (SaveObjectPath.IsEmpty())
	{
		return;
	}

	PackageName = FPaths::GetBaseFilename(*SaveObjectPath, false);
	UPackage* Package = CreatePackage(*PackageName);

	if (!Package)
	{
		return;
	}

	AssetName = FPaths::GetBaseFilename(*SaveObjectPath, true);

	Instance->SetFlags(RF_Standalone | RF_Public | RF_Transactional);
	Instance->Rename(*AssetName, Package, REN_DontCreateRedirectors);

	FAssetRegistryModule::AssetCreated(Instance);
}

#undef LOCTEXT_NAMESPACE
