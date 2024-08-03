// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utils/DMMaterialModelFunctionLibrary.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Components/DMMaterialLayer.h"
#include "Components/PrimitiveComponent.h"
#include "ContentBrowserModule.h"
#include "DMObjectMaterialProperty.h"
#include "DMWorldSubsystem.h"
#include "DynamicMaterialEditorModule.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineAnalytics.h"
#include "GameFramework/Actor.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "Material/DynamicMaterialInstance.h"
#include "Material/DynamicMaterialInstanceFactory.h"
#include "Materials/Material.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelDynamic.h"
#include "Model/IDynamicMaterialModelEditorOnlyDataInterface.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Utils/DMPrivate.h"

#define LOCTEXT_NAMESPACE "DMMaterialModelFunctionLibrary"

TArray<FDMObjectMaterialProperty> UDMMaterialModelFunctionLibrary::GetActorMaterialProperties(AActor* InActor)
{
	TArray<FDMObjectMaterialProperty> ActorProperties;

	if (!IsValid(InActor))
	{
		return ActorProperties;
	}

	FDMGetObjectMaterialPropertiesDelegate PropertyGenerator = FDynamicMaterialEditorModule::GetCustomMaterialPropertyGenerator(InActor->GetClass());

	if (PropertyGenerator.IsBound())
	{
		ActorProperties = PropertyGenerator.Execute(InActor);

		if (!ActorProperties.IsEmpty())
		{
			return ActorProperties;
		}
	}

	InActor->ForEachComponent<UPrimitiveComponent>(false, [&ActorProperties](UPrimitiveComponent* InComp)
		{
			for (int32 MaterialIdx = 0; MaterialIdx < InComp->GetNumMaterials(); ++MaterialIdx)
			{
				ActorProperties.Add({InComp, MaterialIdx});
			}
		});

	return ActorProperties;
}

UDynamicMaterialModel* UDMMaterialModelFunctionLibrary::CreateDynamicMaterialInObject(FDMObjectMaterialProperty& InMaterialProperty)
{
	if (!InMaterialProperty.IsValid())
	{
		return nullptr;
	}

	UObject* const Outer = InMaterialProperty.GetOuter();

	UDynamicMaterialInstanceFactory* const InstanceFactory = NewObject<UDynamicMaterialInstanceFactory>();
	check(InstanceFactory);

	UDynamicMaterialInstance* const NewInstance = Cast<UDynamicMaterialInstance>(InstanceFactory->FactoryCreateNew(UDynamicMaterialInstance::StaticClass(),
		Outer, NAME_None, RF_Transactional, nullptr, GWarn));
	check(NewInstance);

	bool bSubsystemTakenOver = false;

	if (const UWorld* const World = Outer->GetWorld())
	{
		if (IsValid(World))
		{
			if (UDMWorldSubsystem* const WorldSubsystem = World->GetSubsystem<UDMWorldSubsystem>())
			{
				bSubsystemTakenOver = WorldSubsystem->ExecuteMaterialValueSetterDelegate(InMaterialProperty, NewInstance);
			}
		}
	}

	if (!bSubsystemTakenOver)
	{
		InMaterialProperty.SetMaterial(NewInstance);
	}

	if (UDynamicMaterialModel* MaterialModel = NewInstance->GetMaterialModel())
	{
		if (IDynamicMaterialModelEditorOnlyDataInterface* EditorOnlyData = MaterialModel->GetEditorOnlyData())
		{
			EditorOnlyData->RequestMaterialBuild();
		}

		return MaterialModel;
	}

	return nullptr;
}

UDynamicMaterialInstance* UDMMaterialModelFunctionLibrary::ExportMaterialInstance(UDynamicMaterialModelBase* InMaterialModelBase)
{
	if (!IsValid(InMaterialModelBase))
	{
		return nullptr;
	}

	UDynamicMaterialInstance* MaterialInstance = InMaterialModelBase->GetDynamicMaterialInstance();

	if (!IsValid(MaterialInstance))
	{
		return nullptr;
	}

	FString CurrentName = MaterialInstance->GetName();

	if (InMaterialModelBase->IsA<UDynamicMaterialModel>())
	{
		CurrentName = CurrentName.StartsWith(TEXT("MDI_"))
			? CurrentName
			: (TEXT("MDI_") + CurrentName);
	}
	else
	{
		CurrentName = CurrentName.StartsWith(TEXT("MDD_"))
			? CurrentName
			: (TEXT("MDD_") + CurrentName);
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	FString PackageName, AssetName;
	AssetTools.CreateUniqueAssetName(CurrentName, TEXT(""), PackageName, AssetName);

	IContentBrowserSingleton& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
	const FContentBrowserItemPath CurrentPath = ContentBrowserModule.GetCurrentPath();
	const FString PathStr = CurrentPath.HasInternalPath() ? CurrentPath.GetInternalPathString() : TEXT("/Game");

	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
	SaveAssetDialogConfig.DefaultPath = PathStr;
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::Disallow;
	SaveAssetDialogConfig.DefaultAssetName = AssetName;

	const FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

	if (SaveObjectPath.IsEmpty())
	{
		return nullptr;
	}

	return ExportMaterialInstance(MaterialInstance->GetMaterialModelBase(), SaveObjectPath);
}

UDynamicMaterialInstance* UDMMaterialModelFunctionLibrary::ExportMaterialInstance(UDynamicMaterialModelBase* InMaterialModel, const FString& InSavePath)
{
	if (!IsValid(InMaterialModel))
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Invalid material to export."));
		return nullptr;
	}

	if (InSavePath.IsEmpty())
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Invalid material save path to export."));
		return nullptr;
	}

	UDynamicMaterialInstance* Instance = InMaterialModel->GetDynamicMaterialInstance();
	if (!Instance)
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Failed to find a Material Designer Instance to export."), true, InMaterialModel);
		return nullptr;
	}

	const FString PackagePath = FPaths::GetBaseFilename(*InSavePath, false);

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		UE::DynamicMaterialEditor::Private::LogError(FString::Printf(TEXT("Failed to create package for Material Designer Instance (%s)."), *PackagePath));
		return nullptr;
	}

	const FString AssetName = FPaths::GetBaseFilename(*InSavePath, true);

	FObjectDuplicationParameters Params = InitStaticDuplicateObjectParams(Instance, Package, *AssetName,
		RF_Public | RF_Standalone, nullptr, EDuplicateMode::Normal, EInternalObjectFlags::None);

	UObject* NewAsset = StaticDuplicateObjectEx(Params);
	if (!NewAsset)
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Failed to create new Material Designer Instance asset."));
		return nullptr;
	}

	// Not sure why these need to be set again!
	NewAsset->SetFlags(RF_Public | RF_Standalone);

	UDynamicMaterialInstance* NewInstance = Cast<UDynamicMaterialInstance>(NewAsset);

	if (NewInstance)
	{
		if (InMaterialModel->IsA<UDynamicMaterialModel>())
		{
			if (UDynamicMaterialModel* NewModel = NewInstance->GetMaterialModel())
			{
				if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = NewModel->GetEditorOnlyData())
				{
					ModelEditorOnlyData->RequestMaterialBuild();
				}
			}
		}
		else if (InMaterialModel->IsA<UDynamicMaterialModelDynamic>())
		{
			NewInstance->InitializeMIDPublic();
		}
	}

	FAssetRegistryModule::AssetCreated(NewAsset);

	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MaterialDesigner.ExportedMaterialInstance"));
	}

	return NewInstance;
}

UMaterial* UDMMaterialModelFunctionLibrary::ExportGeneratedMaterial(UDynamicMaterialModelBase* InMaterialModelBase)
{
	return nullptr;
}

UMaterial* UDMMaterialModelFunctionLibrary::ExportGeneratedMaterial(UDynamicMaterialModelBase* InMaterialModelBase, const FString& InSavePath)
{
	if (!IsValid(InMaterialModelBase))
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Invalid material to export."));
		return nullptr;
	}

	if (InSavePath.IsEmpty())
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Invalid material save path to export."));
		return nullptr;
	}

	UMaterial* GeneratedMaterial = InMaterialModelBase->GetGeneratedMaterial();

	if (!GeneratedMaterial)
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Failed to find a generated material to export."));
		return nullptr;
	}

	const FString PackagePath = FPaths::GetBaseFilename(*InSavePath, false);

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		UE::DynamicMaterialEditor::Private::LogError(FString::Printf(TEXT("Failed to create package for exported material (%s)."), *PackagePath));
		return nullptr;
	}

	const FString AssetName = FPaths::GetBaseFilename(*InSavePath, true);

	FObjectDuplicationParameters Params = InitStaticDuplicateObjectParams(GeneratedMaterial, Package, *AssetName,
		RF_Public | RF_Standalone, nullptr, EDuplicateMode::Normal, EInternalObjectFlags::None);

	UObject* NewAsset = StaticDuplicateObjectEx(Params);

	if (!NewAsset)
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Failed to create new material asset."));
		return nullptr;
	}

	// Not sure why these need to be set again!
	NewAsset->SetFlags(RF_Public | RF_Standalone);

	FAssetRegistryModule::AssetCreated(NewAsset);

	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MaterialDesigner.ExportedGeneratedMaterial"));
	}

	return Cast<UMaterial>(NewAsset);
}

UDynamicMaterialModel* UDMMaterialModelFunctionLibrary::ExportToTemplateMaterialModel(UDynamicMaterialModelDynamic* InMaterialModelDynamic)
{
	UDynamicMaterialModel* ParentModel = InMaterialModelDynamic->GetParentModel();

	if (!ParentModel)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Failed to find parent model."));
		return nullptr;
	}

	// Where should we save it? (Always export to CB)
	FString CurrentName = InMaterialModelDynamic->GetName();

	CurrentName = CurrentName.StartsWith(TEXT("MDD_"))
		? (TEXT("MDM_") + CurrentName.RightChop(4))
		: (TEXT("MDM_") + CurrentName);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	FString PackageName, AssetName;
	AssetTools.CreateUniqueAssetName(CurrentName, TEXT(""), PackageName, AssetName);

	IContentBrowserSingleton& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
	const FContentBrowserItemPath CurrentPath = ContentBrowserModule.GetCurrentPath();
	const FString PathStr = CurrentPath.HasInternalPath() ? CurrentPath.GetInternalPathString() : TEXT("/Game");

	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
	SaveAssetDialogConfig.DefaultPath = PathStr;
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::Disallow;
	SaveAssetDialogConfig.DefaultAssetName = AssetName;

	const FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

	if (SaveObjectPath.IsEmpty())
	{
		UE_LOG(LogDynamicMaterialEditor, Warning, TEXT("No path was chosen for saving the new editable asset, cancelling."));
		return nullptr;
	}

	return ExportToTemplateMaterialModel(InMaterialModelDynamic, SaveObjectPath);
}

UDynamicMaterialModel* UDMMaterialModelFunctionLibrary::ExportToTemplateMaterialModel(UDynamicMaterialModelDynamic* InMaterialModelDynamic, const FString& InSavePath)
{
	UDynamicMaterialModel* ParentModel = InMaterialModelDynamic->GetParentModel();

	if (!ParentModel)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Failed to find parent model."));
		return nullptr;
	}

	// Create new model
	UDynamicMaterialModel* NewModel = InMaterialModelDynamic->ToEditable(GetTransientPackage());

	if (!NewModel)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Failed to convert dynamic asset to editable."));
		return nullptr;
	}

	// Create a package for it
	const FString PackageName = FPaths::GetBaseFilename(*InSavePath, false);
	UPackage* Package = CreatePackage(*PackageName);

	if (!Package)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Failed to create new package for editable asset."));
		return nullptr;
	}

	const FString AssetName = FPaths::GetBaseFilename(*InSavePath, true);

	NewModel->Rename(nullptr, Package, UE::DynamicMaterial::RenameFlags);
	NewModel->SetFlags(RF_Transactional | RF_Public | RF_Standalone);

	FAssetRegistryModule::AssetCreated(NewModel);

	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MaterialDesigner.ExportToTemplateMaterialModel"));
	}

	return NewModel;
}

UDynamicMaterialInstance* UDMMaterialModelFunctionLibrary::ExportToTemplateMaterialInstance(UDynamicMaterialModelDynamic* InMaterialModelDynamic)
{
	UDynamicMaterialModel* ParentModel = InMaterialModelDynamic->GetParentModel();

	if (!ParentModel)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Failed to find parent model."));
		return nullptr;
	}

	UDynamicMaterialInstance* OldInstance = InMaterialModelDynamic->GetDynamicMaterialInstance();

	if (!OldInstance)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Failed to find material instance."));
		return nullptr;
	}

	// Where should we save it? (Always export to CB)
	FString CurrentName = OldInstance ? OldInstance->GetName() : InMaterialModelDynamic->GetName();

	CurrentName = CurrentName.StartsWith(TEXT("MDD_"))
		? (TEXT("MDI_") + CurrentName.RightChop(4))
		: (TEXT("MDI_") + CurrentName);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	FString PackageName, AssetName;
	AssetTools.CreateUniqueAssetName(CurrentName, TEXT(""), PackageName, AssetName);

	IContentBrowserSingleton& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
	const FContentBrowserItemPath CurrentPath = ContentBrowserModule.GetCurrentPath();
	const FString PathStr = CurrentPath.HasInternalPath() ? CurrentPath.GetInternalPathString() : TEXT("/Game");

	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveAssetDialogTitle", "Save Asset As");
	SaveAssetDialogConfig.DefaultPath = PathStr;
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::Disallow;
	SaveAssetDialogConfig.DefaultAssetName = AssetName;

	const FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

	if (SaveObjectPath.IsEmpty())
	{
		UE_LOG(LogDynamicMaterialEditor, Warning, TEXT("No path was chosen for saving the new editable asset, cancelling."));
		return nullptr;
	}

	return ExportToTemplateMaterialInstance(InMaterialModelDynamic, SaveObjectPath);	
}

UDynamicMaterialInstance* UDMMaterialModelFunctionLibrary::ExportToTemplateMaterialInstance(UDynamicMaterialModelDynamic* InMaterialModelDynamic, const FString& InSavePath)
{
	UDynamicMaterialModel* ParentModel = InMaterialModelDynamic->GetParentModel();

	if (!ParentModel)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Failed to find parent model."));
		return nullptr;
	}

	UDynamicMaterialInstance* OldInstance = InMaterialModelDynamic->GetDynamicMaterialInstance();

	if (!OldInstance)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Failed to find material instance."));
		return nullptr;
	}

	// Create new model
	UDynamicMaterialModel* NewModel = InMaterialModelDynamic->ToEditable(GetTransientPackage());

	if (!NewModel)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Failed to convert dynamic asset to editable."));
		return nullptr;
	}

	// Create a package for it
	const FString PackageName = FPaths::GetBaseFilename(*InSavePath, false);
	UPackage* Package = CreatePackage(*PackageName);

	if (!Package)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Failed to create new package for editable asset."));
		return nullptr;
	}

	const FString AssetName = FPaths::GetBaseFilename(*InSavePath, true);

	UDynamicMaterialInstance* NewInstance = Cast<UDynamicMaterialInstance>(GetMutableDefault<UDynamicMaterialInstanceFactory>()->FactoryCreateNew(
		UDynamicMaterialInstance::StaticClass(),
		Package,
		*AssetName,
		RF_Transactional | RF_Public | RF_Standalone,
		NewModel,
		nullptr
	));

	FAssetRegistryModule::AssetCreated(NewInstance);

	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MaterialDesigner.ExportToTemplateMaterialInstance"));
	}

	return NewInstance;
}

bool UDMMaterialModelFunctionLibrary::IsModelValid(UDynamicMaterialModelBase* InMaterialModelBase)
{
	if (!IsValid(InMaterialModelBase))
	{
		return false;
	}

	if (UWorld* World = InMaterialModelBase->GetWorld())
	{
		if (UDMWorldSubsystem* WorldSubsystem = World->GetSubsystem<UDMWorldSubsystem>())
		{
			if (!WorldSubsystem->ExecuteIsValidDelegate(InMaterialModelBase))
			{
				return false;
			}
		}
	}

	UActorComponent* ComponentOuter = InMaterialModelBase->GetTypedOuter<UActorComponent>();

	if (ComponentOuter && !IsValid(ComponentOuter))
	{
		return false;
	}

	AActor* ActorOuter = InMaterialModelBase->GetTypedOuter<AActor>();

	if (ActorOuter && !IsValid(ActorOuter))
	{
		return false;
	}

	UPackage* PackageOuter = InMaterialModelBase->GetPackage();

	if (PackageOuter && !IsValid(PackageOuter))
	{
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
