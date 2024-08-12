// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/SDMMaterialDesigner.h"

#include "Containers/Set.h"
#include "DynamicMaterialEditorSettings.h"
#include "GameFramework/Actor.h"
#include "Material/DynamicMaterialInstance.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelBase.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "SAssetDropTarget.h"
#include "UI/Utils/DMDropTargetPrivateSetter.h"
#include "UI/Widgets/SDMActorMaterialSelector.h"
#include "UI/Widgets/SDMMaterialEditor.h"
#include "UI/Widgets/SDMMaterialSelectPrompt.h"
#include "UI/Widgets/SDMMaterialWizard.h"
#include "Utils/DMMaterialModelFunctionLibrary.h"
#include "Widgets/SNullWidget.h"

void SDMMaterialDesigner::PrivateRegisterAttributes(FSlateAttributeDescriptor::FInitializer&)
{
}

void SDMMaterialDesigner::Construct(const FArguments& InArgs)
{
	SetCanTick(true);

	ContentSlot = TDMWidgetSlot<SWidget>(SharedThis(this), 0, SNullWidget::NullWidget);

	SetSelectPromptView();
}

bool SDMMaterialDesigner::OpenMaterialModelBase(UDynamicMaterialModelBase* InMaterialModelBase)
{
	if (IsValid(InMaterialModelBase) && UDMMaterialModelFunctionLibrary::IsModelValid(InMaterialModelBase))
	{
		OpenMaterialModelBase_Internal(InMaterialModelBase);
		return true;
	}

	return false;
}

bool SDMMaterialDesigner::OpenMaterialInstance(UDynamicMaterialInstance* InMaterialInstance)
{
	if (IsValid(InMaterialInstance))
	{
		if (UDynamicMaterialModelBase* MaterialModelBase = InMaterialInstance->GetMaterialModelBase())
		{
			OpenMaterialModelBase(MaterialModelBase);
			return true;
		}
	}

	return false;
}

bool SDMMaterialDesigner::OpenObjectMaterialProperty(const FDMObjectMaterialProperty& InObjectMaterialProperty)
{
	if (InObjectMaterialProperty.IsValid())
	{
		OpenObjectMaterialProperty_Internal(InObjectMaterialProperty);
		return true;
	}

	return false;
}

bool SDMMaterialDesigner::OpenActor(AActor* InActor)
{
	if (IsValid(InActor))
	{
		OpenActor_Internal(InActor);
		return true;
	}

	return false;
}

void SDMMaterialDesigner::ShowSelectPrompt()
{
	SetSelectPromptView();	
}

void SDMMaterialDesigner::Empty()
{
	SetEmptyView();
}

void SDMMaterialDesigner::OnMaterialModelBaseSelected(UDynamicMaterialModelBase* InMaterialModelBase)
{
	if (IsFollowingSelection())
	{
		OpenMaterialModelBase(InMaterialModelBase);
	}
}

void SDMMaterialDesigner::OnMaterialInstanceSelected(UDynamicMaterialInstance* InMaterialInstance)
{
	if (IsFollowingSelection())
	{
		OpenMaterialModelBase(InMaterialInstance->GetMaterialModel());
	}
}

void SDMMaterialDesigner::OnObjectMaterialPropertySelected(const FDMObjectMaterialProperty& InObjectMaterialProperty)
{
	if (IsFollowingSelection())
	{
		OpenObjectMaterialProperty(InObjectMaterialProperty);
	}
}

void SDMMaterialDesigner::OnActorSelected(AActor* InActor)
{
	if (IsFollowingSelection())
	{
		OpenActor(InActor);
	}
}

UDynamicMaterialModelBase* SDMMaterialDesigner::GetMaterialModelBase() const
{
	if (!Content.IsValid())
	{
		return nullptr;
	}

	if (Content->GetWidgetClass().GetWidgetType() == SDMMaterialEditor::StaticWidgetClass().GetWidgetType())
	{
		return StaticCastSharedPtr<SDMMaterialEditor>(Content)->GetMaterialModelBase();
	}

	if (Content->GetWidgetClass().GetWidgetType() == SDMMaterialWizard::StaticWidgetClass().GetWidgetType())
	{
		return StaticCastSharedPtr<SDMMaterialWizard>(Content)->GetMaterialModel();
	}

	return nullptr;
}

void SDMMaterialDesigner::Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (Content.IsValid() && Content->GetWidgetClass().GetWidgetType() == SDMMaterialEditor::StaticWidgetClass().GetWidgetType())
	{
		StaticCastSharedPtr<SDMMaterialEditor>(Content)->Validate();
	}

	SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
}

void SDMMaterialDesigner::OpenMaterialModelBase_Internal(UDynamicMaterialModelBase* InMaterialModelBase)
{
	if (NeedsWizard(InMaterialModelBase))
	{
		SetWizardView(Cast<UDynamicMaterialModel>(InMaterialModelBase));
		return;
	}

	SetEditorView(InMaterialModelBase);
}

void SDMMaterialDesigner::OpenObjectMaterialProperty_Internal(const FDMObjectMaterialProperty& InObjectMaterialProperty)
{
	if (UDynamicMaterialModelBase* ModelBase = InObjectMaterialProperty.GetMaterialModelBase())
	{
		if (NeedsWizard(ModelBase))
		{
			SetWizardView(InObjectMaterialProperty);
		}
		else
		{
			SetEditorView(InObjectMaterialProperty);
		}

		return;
	}

	if (AActor* MaterialActor = InObjectMaterialProperty.GetTypedOuter<AActor>())
	{
		TArray<FDMObjectMaterialProperty> ActorProperties = UDMMaterialModelFunctionLibrary::GetActorMaterialProperties(MaterialActor);
		SetMaterialSelectorView(MaterialActor, MoveTemp(ActorProperties));
		return;
	}

	SetSelectPromptView();
}

void SDMMaterialDesigner::OpenActor_Internal(AActor* InActor)
{
	SetWidget(SNullWidget::NullWidget, /* Include Drop Target */ true);

	TArray<FDMObjectMaterialProperty> ActorProperties = UDMMaterialModelFunctionLibrary::GetActorMaterialProperties(InActor);

	if (ActorProperties.IsEmpty())
	{
		SetSelectPromptView();
		return;
	}

	for (const FDMObjectMaterialProperty& MaterialProperty : ActorProperties)
	{
		if (MaterialProperty.GetMaterialModelBase())
		{
			OpenObjectMaterialProperty(MaterialProperty);
			return;
		}
	}

	SetMaterialSelectorView(InActor, MoveTemp(ActorProperties));
}

void SDMMaterialDesigner::SetEmptyView()
{
	SetWidget(SNullWidget::NullWidget, /* Include drop target */ true);
}

void SDMMaterialDesigner::SetSelectPromptView()
{
	SetWidget(SNew(SDMMaterialSelectPrompt), /* Include Drop Target */ true);
}

void SDMMaterialDesigner::SetMaterialSelectorView(AActor* InActor, TArray<FDMObjectMaterialProperty>&& InActorProperties)
{
	TSharedRef<SDMActorMaterialSelector> Selector = SNew(
		SDMActorMaterialSelector, 
		SharedThis(this), 
		InActor, 
		Forward<TArray<FDMObjectMaterialProperty>>(InActorProperties)
	);

	SetWidget(Selector, /* Include Drop Target */ true);
}

void SDMMaterialDesigner::SetWizardView(UDynamicMaterialModel* InMaterialModel)
{
	TSharedRef<SDMMaterialWizard> Wizard = SNew(SDMMaterialWizard, SharedThis(this))
		.MaterialModel(InMaterialModel);

	SetWidget(Wizard, /* Include Drop Target */ true);
}

void SDMMaterialDesigner::SetWizardView(const FDMObjectMaterialProperty& InObjectMaterialProperty)
{
	TSharedRef<SDMMaterialWizard> Wizard = SNew(SDMMaterialWizard, SharedThis(this))
		.MaterialProperty(InObjectMaterialProperty);

	SetWidget(Wizard, /* Include Drop Target */ true);
}

void SDMMaterialDesigner::SetEditorView(UDynamicMaterialModelBase* InMaterialModelBase)
{
	TSharedRef<SDMMaterialEditor> Editor = SNew(SDMMaterialEditor, SharedThis(this))
		.MaterialModelBase(InMaterialModelBase);

	SetWidget(Editor, /* Include Drop Target */ true);
}

void SDMMaterialDesigner::SetEditorView(const FDMObjectMaterialProperty& InObjectMaterialProperty)
{
	TSharedRef<SDMMaterialEditor> Editor = SNew(SDMMaterialEditor, SharedThis(this))
		.MaterialProperty(InObjectMaterialProperty);

	SetWidget(Editor, /* Include Drop Target */ true);
}

void SDMMaterialDesigner::SetWidget(const TSharedRef<SWidget>& InWidget, bool bInIncludeAssetDropTarget)
{
	Content = InWidget;

	if (!bInIncludeAssetDropTarget)
	{
		ContentSlot << InWidget;
	}
	else
	{
		TSharedRef<SAssetDropTarget> DropTarget = SNew(SAssetDropTarget)
			.OnAreAssetsAcceptableForDrop(this, &SDMMaterialDesigner::OnAssetDraggedOver)
			.OnAssetsDropped(this, &SDMMaterialDesigner::OnAssetsDropped)
			[
				InWidget
			];

		using namespace UE::DynamicMaterialEditor::Private;
		DropTarget::SetInvalidColor(&DropTarget.Get(), FStyleColors::Transparent);

		ContentSlot << DropTarget;
	}
}

bool SDMMaterialDesigner::IsFollowingSelection()
{
	if (UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get())
	{
		return Settings->bFollowSelection;
	}

	return false;
}

bool SDMMaterialDesigner::NeedsWizard(UDynamicMaterialModelBase* InMaterialModelBase) const
{
	if (UDynamicMaterialModel* MaterialModel = Cast<UDynamicMaterialModel>(InMaterialModelBase))
	{
		if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
		{
			if (EditorOnlyData->NeedsWizard())
			{
				return true;
			}
		}
	}

	return false;
}

bool SDMMaterialDesigner::OnAssetDraggedOver(TArrayView<FAssetData> InAssets)
{
	const TArray<UClass*> AllowedClasses = {
		AActor::StaticClass(),
		UDynamicMaterialModelBase::StaticClass(),
		UDynamicMaterialInstance::StaticClass()
	};

	for (const FAssetData& Asset : InAssets)
	{
		UClass* AssetClass = Asset.GetClass(EResolveClass::Yes);

		if (!AssetClass)
		{
			continue;
		}

		for (UClass* AllowedClass : AllowedClasses)
		{
			if (AssetClass->IsChildOf(AllowedClass))
			{
				return true;
			}
		}
	}

	return false;
}

void SDMMaterialDesigner::OnAssetsDropped(const FDragDropEvent& InDragDropEvent, TArrayView<FAssetData> InAssets)
{
	for (const FAssetData& Asset : InAssets)
	{
		UClass* AssetClass = Asset.GetClass(EResolveClass::Yes);

		if (!AssetClass)
		{
			continue;
		}

		if (AssetClass->IsChildOf(AActor::StaticClass()))
		{
			if (OpenActor(Cast<AActor>(Asset.GetAsset())))
			{
				return;
			}
		}
		else if (AssetClass->IsChildOf(UDynamicMaterialModelBase::StaticClass()))
		{
			if (OpenMaterialModelBase(Cast<UDynamicMaterialModelBase>(Asset.GetAsset())))
			{
				return;
			}
		}
		else if (AssetClass->IsChildOf(UDynamicMaterialInstance::StaticClass()))
		{
			if (OpenMaterialInstance(Cast<UDynamicMaterialInstance>(Asset.GetAsset())))
			{
				return;
			}
		}
	}
}
