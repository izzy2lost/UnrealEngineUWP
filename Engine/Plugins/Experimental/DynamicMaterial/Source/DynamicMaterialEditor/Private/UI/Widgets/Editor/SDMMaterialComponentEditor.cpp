// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/Editor/SDMMaterialComponentEditor.h"

#include "Components/DMMaterialComponent.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialStage.h"
#include "DetailLayoutBuilder.h"
#include "DMEDefs.h"
#include "DynamicMaterialEditorModule.h"
#include "DynamicMaterialEditorStyle.h"
#include "DynamicMaterialModule.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelBase.h"
#include "UI/Menus/DMMaterialStageSourceMenus.h"
#include "UI/Widgets/Editor/SDMMaterialSlotEditor.h"
#include "UI/Widgets/Editor/SlotEditor/SDMMaterialSlotLayerItem.h"
#include "UI/Widgets/Editor/SlotEditor/SDMMaterialSlotLayerView.h"
#include "UI/Widgets/Editor/SlotEditor/SDMMaterialStage.h"
#include "UI/Widgets/SDMMaterialEditor.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "SDMMaterialComponentEditor"

void SDMMaterialComponentEditor::PrivateRegisterAttributes(FSlateAttributeDescriptor::FInitializer&)
{
}

SDMMaterialComponentEditor::~SDMMaterialComponentEditor()
{
	if (!FDynamicMaterialModule::AreUObjectsSafe())
	{
		return;
	}

	if (UDMMaterialComponent* Component = GetComponent())
	{
		Component->GetOnUpdate().RemoveAll(this);
	}
}

void SDMMaterialComponentEditor::Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialEditor>& InEditorWidget, 
	UDMMaterialComponent* InMaterialComponent)
{
	SetCanTick(false);

	SDMObjectEditorWidgetBase::Construct(
		SDMObjectEditorWidgetBase::FArguments(), 
		InEditorWidget, 
		InMaterialComponent
	);

	if (InMaterialComponent)
	{
		InMaterialComponent->GetOnUpdate().AddSP(this, &SDMMaterialComponentEditor::OnComponentUpdated);
	}
}

UDMMaterialComponent* SDMMaterialComponentEditor::GetComponent() const
{
	return Cast<UDMMaterialComponent>(ObjectWeak.Get());
}

TSharedRef<SWidget> SDMMaterialComponentEditor::CreateSourceTypeEditWidget()
{
	TWeakPtr<SDMMaterialComponentEditor> ThisWeak = SharedThis(this);

	return SNew(SBox)
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(0.f, 0.f, 0.f, 0.2f)
		[
			SNew(SComboButton)
			.HasDownArrow(false)
			.IsFocusable(true)
			.ContentPadding(4.0f)
			.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly.Bordered.Dark")
			.ToolTipText(LOCTEXT("ChangeLayer", "Change Stage Type"))
			.OnGetMenuContent(this, &SDMMaterialComponentEditor::MakeSourceTypeEditWidgetMenuContent)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(this, &SDMMaterialComponentEditor::GetSourceTypeEditWidgetText)
			]
		];
}

TSharedRef<SWidget> SDMMaterialComponentEditor::MakeSourceTypeEditWidgetMenuContent()
{
	if (UDMMaterialStage* Stage = Cast<UDMMaterialStage>(ObjectWeak.Get()))
	{
		if (UDMMaterialLayerObject* Layer = Stage->GetLayer())
		{
			if (UDMMaterialSlot* Slot = Layer->GetSlot())
			{
				if (TSharedPtr<SDMMaterialEditor> EditorWidget = GetEditorWidget())
				{
					if (TSharedPtr<SDMMaterialSlotEditor> SlotEditorWidget = EditorWidget->GetSlotEditorWidget())
					{
						if (SlotEditorWidget->GetSlot() == Slot)
						{
							if (TSharedPtr<SDMMaterialSlotLayerView> SlotLayerView = SlotEditorWidget->GetLayerView())
							{
								if (TSharedPtr<SDMMaterialSlotLayerItem> SlotLayerItem = SlotLayerView->GetWidgetForLayer(Layer))
								{
									if (TSharedPtr<SDMMaterialStage> StageWidget = SlotLayerItem->GetWidgetForStage(Stage))
									{
										return FDMMaterialStageSourceMenus::MakeChangeSourceMenu(SlotEditorWidget, StageWidget);
									}
								}
							}
						}
					}
				}
			}
		}
	}

	return SNullWidget::NullWidget;
}

FText SDMMaterialComponentEditor::GetSourceTypeEditWidgetText() const
{
	if (UDMMaterialStage* Stage = Cast<UDMMaterialStage>(ObjectWeak.Get()))
	{
		if (UDMMaterialStageSource* Source = Stage->GetSource())
		{
			return Source->GetStageDescription();
		}
	}

	return FText::GetEmpty();
}

void SDMMaterialComponentEditor::OnComponentUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType)
{
	if (EnumHasAnyFlags(InUpdateType, EDMUpdateType::Structure))
	{
		if (TSharedPtr<SDMMaterialEditor> EditorWidget = GetEditorWidget())
		{
			EditorWidget->EditComponent(GetComponent(), /* Force refresh */ true);
		}
	}
}

TArray<FDMPropertyHandle> SDMMaterialComponentEditor::GetPropertyRows()
{
	TArray<FDMPropertyHandle> PropertyRows;
	TSet<UDMMaterialComponent*> ProcessedObjects;

	bool bIsDynamic = false;

	if (TSharedPtr<SDMMaterialEditor> EditorWidget = GetEditorWidget())
	{
		if (UDynamicMaterialModelBase* MaterialModelBase = EditorWidget->GetMaterialModelBase())
		{
			bIsDynamic = !MaterialModelBase->IsA<UDynamicMaterialModel>();
		}
	}

	if (UDMMaterialStage* Stage = Cast<UDMMaterialStage>(ObjectWeak.Get()))
	{
		FDMPropertyHandle& SourceHandle = PropertyRows.AddDefaulted_GetRef();
		SourceHandle.ValueName = TEXT("SourceType");
		SourceHandle.NameOverride = LOCTEXT("SourceType", "Source Type");
		SourceHandle.bEnabled = !bIsDynamic;
		SourceHandle.ValueWidget = CreateSourceTypeEditWidget();
		SourceHandle.ResetToDefaultOverride = FResetToDefaultOverride::Hide(true);
		SourceHandle.Priority = EDMPropertyHandlePriority::High;
	}

	FDynamicMaterialEditorModule::GeneratorComponentPropertyRows(
		SharedThis(this), 
		GetComponent(), 
		PropertyRows, 
		ProcessedObjects
	);

	return PropertyRows;
}

void SDMMaterialComponentEditor::OnUndo()
{
	if (TSharedPtr<SDMMaterialEditor> EditorWidget = GetEditorWidget())
	{
		EditorWidget->EditComponent(GetComponent(), /* Force refresh */ true);
	}
}

#undef LOCTEXT_NAMESPACE
