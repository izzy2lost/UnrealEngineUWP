// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PropertyGenerators/DMStagePropertyRowGenerator.h"

#include "Components/DMMaterialComponent.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageSource.h"
#include "DetailLayoutBuilder.h"
#include "DMEDefs.h"
#include "DynamicMaterialEditorModule.h"
#include "Internationalization/Text.h"
#include "Internationalization/Text.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelBase.h"
#include "UI/Menus/DMMaterialStageSourceMenus.h"
#include "UI/Widgets/Editor/SDMMaterialComponentEditor.h"
#include "UI/Widgets/Editor/SDMMaterialSlotEditor.h"
#include "UI/Widgets/Editor/SlotEditor/SDMMaterialSlotLayerItem.h"
#include "UI/Widgets/Editor/SlotEditor/SDMMaterialSlotLayerView.h"
#include "UI/Widgets/Editor/SlotEditor/SDMMaterialStage.h"
#include "UI/Widgets/SDMMaterialEditor.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "DMStagePropertyRowGenerator"

const TSharedRef<FDMStagePropertyRowGenerator>& FDMStagePropertyRowGenerator::Get()
{
	static TSharedRef<FDMStagePropertyRowGenerator> Generator = MakeShared<FDMStagePropertyRowGenerator>();
	return Generator;
}

void FDMStagePropertyRowGenerator::AddComponentProperties(const TSharedRef<SDMMaterialComponentEditor>& InComponentEditorWidget, UDMMaterialComponent* InComponent,
	TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects)
{
	if (!IsValid(InComponent))
	{
		return;
	}

	if (InOutProcessedObjects.Contains(InComponent))
	{
		return;
	}

	UDMMaterialStage* Stage = Cast<UDMMaterialStage>(InComponent);

	if (!Stage)
	{
		return;
	}

	UDMMaterialStageSource* Source = Stage->GetSource();

	if (!Source)
	{
		return;
	}

	bool bIsDynamic = false;

	if (TSharedPtr<SDMMaterialEditor> EditorWidget = InComponentEditorWidget->GetEditorWidget())
	{
		if (UDynamicMaterialModelBase* MaterialModelBase = EditorWidget->GetMaterialModelBase())
		{
			bIsDynamic = !MaterialModelBase->IsA<UDynamicMaterialModel>();
		}
	}

	FDMPropertyHandle& SourceHandle = InOutPropertyRows.AddDefaulted_GetRef();
	SourceHandle.ValueName = TEXT("SourceType");
	SourceHandle.NameOverride = LOCTEXT("SourceType", "Source Type");
	SourceHandle.bEnabled = !bIsDynamic;
	SourceHandle.ValueWidget = CreateSourceTypeEditWidget(InComponentEditorWidget, Stage);
	SourceHandle.ResetToDefaultOverride = FResetToDefaultOverride::Hide(true);
	SourceHandle.Priority = EDMPropertyHandlePriority::High;

	FDynamicMaterialEditorModule::GeneratorComponentPropertyRows(InComponentEditorWidget, Source, InOutPropertyRows, InOutProcessedObjects);
	FDMComponentPropertyRowGenerator::AddComponentProperties(InComponentEditorWidget, Stage, InOutPropertyRows, InOutProcessedObjects);
}

TSharedRef<SWidget> FDMStagePropertyRowGenerator::CreateSourceTypeEditWidget(const TSharedRef<SDMMaterialComponentEditor>& InComponentEditorWidget, 
	UDMMaterialStage* InStage)
{
	constexpr float Padding = 2.f;

	const FComboButtonStyle& ComboStyle = FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("ComboButton");

	return SNew(SComboButton)
		.HasDownArrow(false)
		.IsFocusable(true)
		.ContentPadding(FMargin(0.f, Padding, Padding, 0.f))
		.ToolTipText(LOCTEXT("ChangeStageSourceToolTip", "Change Stage Source"))
		.OnGetMenuContent_Static(&FDMStagePropertyRowGenerator::MakeSourceTypeEditWidgetMenuContent, InComponentEditorWidget.ToWeakPtr(), InStage)
		.ButtonContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(GetSourceTypeEditWidgetText(InStage))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(EVerticalAlignment::VAlign_Center)
			.Padding(2.f, 0.f, 0.f, 0.f)
			[
				SNew(SImage)
				.Image(ComboStyle.DownArrowImage.IsSet() ? &ComboStyle.DownArrowImage : nullptr)
			]
		];
}

TSharedRef<SWidget> FDMStagePropertyRowGenerator::MakeSourceTypeEditWidgetMenuContent(TWeakPtr<SDMMaterialComponentEditor> InComponentEditorWidgetWeak, 
	UDMMaterialStage* InStage)
{
	if (IsValid(InStage))
	{
		if (UDMMaterialLayerObject* Layer = InStage->GetLayer())
		{
			if (UDMMaterialSlot* Slot = Layer->GetSlot())
			{
				if (TSharedPtr<SDMMaterialComponentEditor> ComponentEditor = InComponentEditorWidgetWeak.Pin())
				{
					if (TSharedPtr<SDMMaterialEditor> EditorWidget = ComponentEditor->GetEditorWidget())
					{
						if (TSharedPtr<SDMMaterialSlotEditor> SlotEditorWidget = EditorWidget->GetSlotEditorWidget())
						{
							if (SlotEditorWidget->GetSlot() == Slot)
							{
								if (TSharedPtr<SDMMaterialSlotLayerView> SlotLayerView = SlotEditorWidget->GetLayerView())
								{
									if (TSharedPtr<SDMMaterialSlotLayerItem> SlotLayerItem = SlotLayerView->GetWidgetForLayer(Layer))
									{
										if (TSharedPtr<SDMMaterialStage> StageWidget = SlotLayerItem->GetWidgetForStage(InStage))
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
	}

	return SNullWidget::NullWidget;
}

FText FDMStagePropertyRowGenerator::GetSourceTypeEditWidgetText(UDMMaterialStage* InStage)
{
	if (IsValid(InStage))
	{
		if (UDMMaterialStageSource* Source = InStage->GetSource())
		{
			return Source->GetStageDescription();
		}
	}

	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
