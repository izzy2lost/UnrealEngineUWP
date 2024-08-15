// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/Editor/SDMMaterialPropertyPreviews.h"

#include "Components/DMMaterialProperty.h"
#include "Components/DMMaterialSlot.h"
#include "DetailLayoutBuilder.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "UI/Widgets/Editor/SDMMaterialPropertySelector.h"
#include "UI/Widgets/SDMMaterialEditor.h"
#include "UI/Widgets/Visualizers/SDMMaterialComponentPreview.h"
#include "Utils/DMPrivate.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMMaterialPropertyPreviews"

void SDMMaterialPropertyPreviews::PrivateRegisterAttributes(FSlateAttributeDescriptor::FInitializer&)
{
}

void SDMMaterialPropertyPreviews::Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialEditor>& InEditorWidget)
{
	EditorWidgetWeak = InEditorWidget;

	Content = TDMWidgetSlot<SWidget>(SharedThis(this), 0, CreateSlot_Content());
}

void SDMMaterialPropertyPreviews::Validate()
{
	if (Content.HasBeenInvalidated())
	{
		Content << CreateSlot_Content();
	}
}

TSharedRef<SWidget> SDMMaterialPropertyPreviews::CreateSlot_Content()
{
	TSharedPtr<SDMMaterialEditor> EditorWidget = EditorWidgetWeak.Pin();

	if (!EditorWidget.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	UDynamicMaterialModel* MaterialModel = EditorWidget->GetMaterialModel();

	if (!MaterialModel)
	{
		return SNullWidget::NullWidget;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);

	if (!EditorOnlyData)
	{
		return SNullWidget::NullWidget;
	}

	TSharedRef<SWrapBox> WrapBox = SNew(SWrapBox)
		.InnerSlotPadding(FVector2D(5.f, 5.f))
		.UseAllottedSize(true);

	// Active properties first.
	UE::DynamicMaterial::ForEachMaterialPropertyType(
		[this, &WrapBox, EditorOnlyData](EDMMaterialPropertyType InMaterialProperty)
		{
			if (UE::DynamicMaterialEditor::Private::IsCustomMaterialProperty(InMaterialProperty))
			{
				return EDMIterationResult::Continue;
			}

			if (UDMMaterialProperty* MaterialProperty = EditorOnlyData->GetMaterialProperty(InMaterialProperty))
			{
				UDMMaterialSlot* Slot = EditorOnlyData->GetSlotForEnabledMaterialProperty(InMaterialProperty);

				const bool bIsActive = Slot && MaterialProperty->IsEnabled() && MaterialProperty->IsValidForModel(*EditorOnlyData);

				if (bIsActive)
				{
					AddPropertyPreview(WrapBox, InMaterialProperty, Slot);
				}
			}

			return EDMIterationResult::Continue;
		}
	);

	// Now inactive properties
	UE::DynamicMaterial::ForEachMaterialPropertyType(
		[this, &WrapBox, EditorOnlyData](EDMMaterialPropertyType InMaterialProperty)
		{
			if (UE::DynamicMaterialEditor::Private::IsCustomMaterialProperty(InMaterialProperty))
			{
				return EDMIterationResult::Continue;
			}

			if (UDMMaterialProperty* MaterialProperty = EditorOnlyData->GetMaterialProperty(InMaterialProperty))
			{
				UDMMaterialSlot* Slot = EditorOnlyData->GetSlotForEnabledMaterialProperty(InMaterialProperty);

				const bool bIsActive = Slot && MaterialProperty->IsEnabled();
				const bool bIsValid = MaterialProperty->IsValidForModel(*EditorOnlyData);

				if (!bIsActive && bIsValid)
				{
					AddPropertyPreview(WrapBox, InMaterialProperty, Slot);
				}
			}

			return EDMIterationResult::Continue;
		}
	);

	// Now invalid properties
	UE::DynamicMaterial::ForEachMaterialPropertyType(
		[this, &WrapBox, EditorOnlyData](EDMMaterialPropertyType InMaterialProperty)
		{
			if (UE::DynamicMaterialEditor::Private::IsCustomMaterialProperty(InMaterialProperty))
			{
				return EDMIterationResult::Continue;
			}

			if (UDMMaterialProperty* MaterialProperty = EditorOnlyData->GetMaterialProperty(InMaterialProperty))
			{
				UDMMaterialSlot* Slot = EditorOnlyData->GetSlotForEnabledMaterialProperty(InMaterialProperty);

				const bool bIsActive = Slot && MaterialProperty->IsEnabled();
				const bool bIsValid = MaterialProperty->IsValidForModel(*EditorOnlyData);

				if (!bIsActive && !bIsValid)
				{
					AddPropertyPreview(WrapBox, InMaterialProperty, Slot);
				}
			}

			return EDMIterationResult::Continue;
		}
	);

	return SNew(SBox)
		.Padding(5.f)
		[
			WrapBox
		];
}

void SDMMaterialPropertyPreviews::AddPropertyPreview(const TSharedRef<SWrapBox>& InContainer, EDMMaterialPropertyType InMaterialProperty,
	UDMMaterialSlot* InSlot)
{
	TSharedPtr<SDMMaterialEditor> EditorWidget = EditorWidgetWeak.Pin();

	if (!EditorWidget.IsValid())
	{
		return;
	}

	TSharedPtr<SWidget> PreviewWidget;

	if (InSlot)
	{
		PreviewWidget = SNew(SDMMaterialComponentPreview, EditorWidget.ToSharedRef(), InSlot)
			.PreviewSize(FVector2D(60.f, 60.f));

		PreviewWidget->SetCursor(EMouseCursor::Hand);
		PreviewWidget->SetOnMouseButtonUp(FPointerEventHandler::CreateSP(this, &SDMMaterialPropertyPreviews::OnPreviewClicked, InMaterialProperty));
	}
	else
	{
		PreviewWidget = SNew(SBox)
			.WidthOverride(60.f)
			.HeightOverride(60.f);
	}

	InContainer->AddSlot()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				PreviewWidget.ToSharedRef()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 3.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					CreateSlot_EnabledButton(InMaterialProperty)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.MaxWidth(40.f)
				.VAlign(EVerticalAlignment::VAlign_Center)
				.Padding(3.f, 0.f, 0.f, 0.f)
				[
					CreateSlot_PropertyName(InMaterialProperty)
				]
			]
		];
}

TSharedRef<SWidget> SDMMaterialPropertyPreviews::CreateSlot_EnabledButton(EDMMaterialPropertyType InMaterialProperty)
{
	const FText Format = LOCTEXT("PropertyEnableFormat", "Toggle the {0} property.\n\nProperty must be valid for the Material Type.");
	const FText ToolTip = FText::Format(Format, SDMMaterialPropertySelector::GetSelectButtonText(EDMMaterialEditorMode::EditSlot, InMaterialProperty, /* Short Name */ false));

	return SNew(SCheckBox)
		.IsEnabled(this, &SDMMaterialPropertyPreviews::GetPropertyEnabledEnabled, InMaterialProperty)
		.IsChecked(this, &SDMMaterialPropertyPreviews::GetPropertyEnabledState, InMaterialProperty)
		.OnCheckStateChanged(this, &SDMMaterialPropertyPreviews::OnPropertyEnabledStateChanged, InMaterialProperty)
		.ToolTipText(ToolTip);
}

TSharedRef<SWidget> SDMMaterialPropertyPreviews::CreateSlot_PropertyName(EDMMaterialPropertyType InMaterialProperty)
{
	return SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(SDMMaterialPropertySelector::GetSelectButtonText(EDMMaterialEditorMode::EditSlot, InMaterialProperty, /* Short Name */ true))
		.ToolTipText(SDMMaterialPropertySelector::GetSelectButtonText(EDMMaterialEditorMode::EditSlot, InMaterialProperty, /* Short Name */ false));
}

bool SDMMaterialPropertyPreviews::GetPropertyEnabledEnabled(EDMMaterialPropertyType InMaterialProperty) const
{
	TSharedPtr<SDMMaterialEditor> EditorWidget = EditorWidgetWeak.Pin();

	if (!EditorWidget.IsValid())
	{
		return false;
	}

	UDynamicMaterialModel* MaterialModel = EditorWidget->GetMaterialModel();

	if (!MaterialModel)
	{
		return false;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);

	if (!EditorOnlyData)
	{
		return false;
	}

	UDMMaterialProperty* Property = EditorOnlyData->GetMaterialProperty(InMaterialProperty);

	if (!Property)
	{
		return false;
	}

	return Property->IsValidForModel(*EditorOnlyData);
}

ECheckBoxState SDMMaterialPropertyPreviews::GetPropertyEnabledState(EDMMaterialPropertyType InMaterialProperty) const
{
	TSharedPtr<SDMMaterialEditor> EditorWidget = EditorWidgetWeak.Pin();

	if (!EditorWidget.IsValid())
	{
		return ECheckBoxState::Unchecked;
	}

	UDynamicMaterialModel* MaterialModel = EditorWidget->GetMaterialModel();

	if (!MaterialModel)
	{
		return ECheckBoxState::Unchecked;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);

	if (!EditorOnlyData)
	{
		return ECheckBoxState::Unchecked;
	}

	UDMMaterialProperty* Property = EditorOnlyData->GetMaterialProperty(InMaterialProperty);

	if (!Property)
	{
		return ECheckBoxState::Unchecked;
	}

	return (Property->IsEnabled() && EditorOnlyData->GetSlotForMaterialProperty(InMaterialProperty))
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

void SDMMaterialPropertyPreviews::OnPropertyEnabledStateChanged(ECheckBoxState InState, EDMMaterialPropertyType InMaterialProperty)
{
	TSharedPtr<SDMMaterialEditor> EditorWidget = EditorWidgetWeak.Pin();

	if (!EditorWidget.IsValid())
	{
		return;
	}

	UDynamicMaterialModel* MaterialModel = EditorWidget->GetMaterialModel();

	if (!MaterialModel)
	{
		return;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);

	if (!EditorOnlyData)
	{
		return;
	}

	UDMMaterialProperty* MaterialProperty = EditorOnlyData->GetMaterialProperty(InMaterialProperty);

	if (!MaterialProperty)
	{
		return;
	}

	const bool bEnabled = InState == ECheckBoxState::Checked;

	MaterialProperty->SetEnabled(bEnabled);

	if (bEnabled)
	{
		UDMMaterialSlot* Slot = EditorOnlyData->GetSlotForMaterialProperty(InMaterialProperty);

		if (!Slot)
		{
			EditorOnlyData->AddSlotForMaterialProperty(InMaterialProperty);
		}
	}

	Content.Invalidate();

	// Make sure we go back to the property previews
	EditorWidget->ShowPropertyPreviews();
}

FReply SDMMaterialPropertyPreviews::OnPreviewClicked(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent, 
	EDMMaterialPropertyType InMaterialProperty)
{
	TSharedPtr<SDMMaterialEditor> EditorWidget = EditorWidgetWeak.Pin();

	if (!EditorWidget.IsValid())
	{
		return FReply::Handled();
	}

	UDynamicMaterialModel* MaterialModel = EditorWidget->GetMaterialModel();

	if (!MaterialModel)
	{
		return FReply::Handled();
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);

	if (!EditorOnlyData)
	{
		return FReply::Handled();
	}

	UDMMaterialSlot* Slot = EditorOnlyData->GetSlotForMaterialProperty(InMaterialProperty);

	if (!Slot)
	{
		return FReply::Handled();
	}

	EditorWidget->SelectProperty(InMaterialProperty);

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
