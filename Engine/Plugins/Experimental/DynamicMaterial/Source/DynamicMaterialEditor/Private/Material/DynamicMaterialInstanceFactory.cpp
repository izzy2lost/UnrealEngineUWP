// Copyright Epic Games, Inc. All Rights Reserved.

#include "Material/DynamicMaterialInstanceFactory.h"
#include "AssetToolsModule.h"
#include "DMPrivate.h"
#include "IAssetTools.h"
#include "Material/DynamicMaterialInstance.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/IDynamicMaterialModelEditorOnlyDataInterface.h"
#include "Model/DynamicMaterialModelFactory.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "MaterialDesignerInstanceFactory"

const FString UDynamicMaterialInstanceFactory::BaseDirectory = FString("/Game/DynamicMaterials");
const FString UDynamicMaterialInstanceFactory::BaseName = FString("M_DynMatInst_");

UDynamicMaterialInstanceFactory::UDynamicMaterialInstanceFactory()
{
	SupportedClass = UDynamicMaterialInstance::StaticClass();

	bCreateNew = true;
	bEditAfterNew = true;
	bEditorImport = false;
	bText = false;
}

UObject* UDynamicMaterialInstanceFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, 
	EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UDynamicMaterialInstance::StaticClass()));

	UDynamicMaterialInstance* NewInstance = NewObject<UDynamicMaterialInstance>(InParent, Class, Name, Flags | RF_Transactional);
	check(NewInstance);

	UDynamicMaterialModelFactory* EditorFactory = NewObject<UDynamicMaterialModelFactory>();
	check(EditorFactory);

	UDynamicMaterialModel* NewModel = Cast<UDynamicMaterialModel>(EditorFactory->FactoryCreateNew(
		UDynamicMaterialModel::StaticClass(), NewInstance, NAME_None, RF_Transactional, nullptr, GWarn));
	check(NewModel);

	const FDMInitializationGuard InitGuard;

	NewInstance->SetMaterialModel(NewModel);

	NewModel->SetDynamicMaterialInstance(NewInstance);

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = NewModel->GetEditorOnlyData())
	{
		ModelEditorOnlyData->RequestMaterialBuild();
	}

	NewInstance->InitializeMIDPublic();

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.OpenEditorForAssets({NewInstance});

	return NewInstance;
}

FText UDynamicMaterialInstanceFactory::GetDisplayName() const
{
	static const FText DisplayName = LOCTEXT("MaterialDesignerInstance", "Material Designer Instance");
	return DisplayName;
}

FText UDynamicMaterialInstanceFactory::GetToolTip() const
{
	static const FText Tooltip = LOCTEXT("MaterialDesignerInstanceTooltip", "The Material Designer Instance is a combination of a Material Instance Dyanmic and a Material Designer Model.");
	return Tooltip;
}

#undef LOCTEXT_NAMESPACE
