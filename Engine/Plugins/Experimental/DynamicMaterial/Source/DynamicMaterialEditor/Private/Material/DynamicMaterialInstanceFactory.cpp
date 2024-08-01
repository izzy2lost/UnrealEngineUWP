// Copyright Epic Games, Inc. All Rights Reserved.

#include "Material/DynamicMaterialInstanceFactory.h"

#include "AssetToolsModule.h"
#include "DMDefs.h"
#include "EngineAnalytics.h"
#include "GameFramework/Actor.h"
#include "IAssetTools.h"
#include "Material/DynamicMaterialInstance.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelFactory.h"
#include "Model/IDynamicMaterialModelEditorOnlyDataInterface.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "MaterialDesignerInstanceFactory"

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

	UDynamicMaterialModelBase* ModelBase = Cast<UDynamicMaterialModelBase>(Context);

	if (!ModelBase)
	{
		ModelBase = Cast<UDynamicMaterialModel>(EditorFactory->FactoryCreateNew(
			UDynamicMaterialModel::StaticClass(), NewInstance, NAME_None, RF_Transactional | RF_Public, nullptr, GWarn));
		check(ModelBase);
	}

	const FDMInitializationGuard InitGuard;

	NewInstance->SetMaterialModel(ModelBase);

	ModelBase->SetDynamicMaterialInstance(NewInstance);

	if (UDynamicMaterialModel* MaterialModel = Cast<UDynamicMaterialModel>(ModelBase))
	{
		if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = MaterialModel->GetEditorOnlyData())
		{
			ModelEditorOnlyData->RequestMaterialBuild();
		}
	}

	NewInstance->InitializeMIDPublic();

	if (InParent)
	{
		if (AActor* Actor = InParent->GetTypedOuter<AActor>())
		{
			if (Actor->bIsEditorPreviewActor)
			{
				// If it is a preview actor do not trigger analytics or open in editor.
				return NewInstance;
			}
		}
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.OpenEditorForAssets({NewInstance});

	if (FEngineAnalytics::IsAvailable())
	{
		static const FString AssetType = TEXT("Asset");
		static const FString SubobjectType = TEXT("Subobject");

		TArray<FAnalyticsEventAttribute> Attribs;
		const bool bIsAsset = NewInstance->IsAsset();

		Attribs.Add(FAnalyticsEventAttribute(TEXT("Type"), bIsAsset ? AssetType : SubobjectType));

		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MaterialDesigner.InstanceCreated"), Attribs);
	}

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
