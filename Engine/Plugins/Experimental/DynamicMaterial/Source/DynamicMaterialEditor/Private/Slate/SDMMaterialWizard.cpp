// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/SDMMaterialWizard.h"

#include "DMDefs.h"
#include "DynamicMaterialEditorSettings.h"
#include "DynamicMaterialEditorStyle.h"
#include "Engine/EngineTypes.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "SDMEditor.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMMaterialWizard"

namespace UE::DynamicMaterialEditor::Private
{
	constexpr float SeparationDistance = 20.f;
	constexpr float TitleContentDistance = 5.f;
	static const FMargin ButtonPadding = FMargin(10.f, 5.f);
	static const FMargin TextPadding = FMargin(5.f, 2.f);
	static const FVector2D WrapBoxSlotPadding = FVector2D(5, 5);
}

void SDMMaterialWizard::Construct(const FArguments& InArgs, const TSharedRef<SDMEditor>& InEditor)
{
	EditorWeak = InEditor;

	if (const UDynamicMaterialEditorSettings* Settings = GetDefault<UDynamicMaterialEditorSettings>())
	{
		if (!Settings->MaterialChannelPresets.IsEmpty())
		{
			CurrentPreset = Settings->MaterialChannelPresets[0].Name;
		}
	}	

	ChildSlot
		[
			CreateLayout()
		];
}

TSharedPtr<SDMEditor> SDMMaterialWizard::GetEditor() const
{
	return EditorWeak.Pin();
}

UDynamicMaterialModel* SDMMaterialWizard::GetMaterialModel() const
{
	if (TSharedPtr<SDMEditor> Editor = EditorWeak.Pin())
	{
		return Editor->GetMaterialModel();
	}

	return nullptr;
}

TSharedRef<SWidget> SDMMaterialWizard::CreateLayout()
{
	using namespace UE::DynamicMaterialEditor::Private;

	return SNew(SBox)
		.Padding(SeparationDistance)
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.TextStyle(FDynamicMaterialEditorStyle::Get(), "ActorNameBig")
				.Text(LOCTEXT("MaterialWizard", "Material Wizard"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(0.0f, SeparationDistance, 0.0f, TitleContentDistance)
			[
				SNew(STextBlock)
				.TextStyle(FDynamicMaterialEditorStyle::Get(), "BoldFont")
				.Text(LOCTEXT("MaterialType", "Material Type"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(EHorizontalAlignment::HAlign_Fill)
			.Padding(0.0f, 0.f, 0.0f, TitleContentDistance)
			[
				CreateChannelPresets()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(0.0f, SeparationDistance, 0.0f, TitleContentDistance)
			[
				SNew(STextBlock)
				.TextStyle(FDynamicMaterialEditorStyle::Get(), "BoldFont")
				.Text(LOCTEXT("AvailableChannels", "Available Channels"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(EHorizontalAlignment::HAlign_Fill)
			[
				SAssignNew(PresetChannelContainer, SBox)
				.HAlign(EHorizontalAlignment::HAlign_Fill)
				[
					CreateChannelList()
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(EHorizontalAlignment::HAlign_Fill)
			.Padding(0.0f, SeparationDistance, 0.0f, 0.f)
			[
				CreateAcceptButton()
			]
		];
}

TSharedRef<SWidget> SDMMaterialWizard::CreateChannelPresets()
{
	using namespace UE::DynamicMaterialEditor::Private;

	TSharedRef<SWrapBox> ChannelPresets = SNew(SWrapBox)
		.UseAllottedSize(true)
		.InnerSlotPadding(WrapBoxSlotPadding)
		.Orientation(EOrientation::Orient_Horizontal);

	for (const FDMMaterialChannelListPreset& Preset : GetDefault<UDynamicMaterialEditorSettings>()->MaterialChannelPresets)
	{
		ChannelPresets->AddSlot()
			[
				SNew(SCheckBox)
					.Style(FAppStyle::Get(), "DetailsView.SectionButton")
					.HAlign(EHorizontalAlignment::HAlign_Center)
					.Padding(ButtonPadding)
					.IsChecked(this, &SDMMaterialWizard::Preset_GetState, Preset.Name)
					.OnCheckStateChanged(this, &SDMMaterialWizard::Preset_OnChange, Preset.Name)
					[
						SNew(STextBlock)
						.TextStyle(FDynamicMaterialEditorStyle::Get(), "RegularFont")
						.Text(FText::FromName(Preset.Name))
					]
			];
	}

	return ChannelPresets;
}

TSharedRef<SWidget> SDMMaterialWizard::CreateChannelList()
{
	using namespace UE::DynamicMaterialEditor::Private;

	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(GetMaterialModel());

	if (!ModelEditorOnlyData)
	{
		return SNullWidget::NullWidget;
	}

	UEnum* MaterialPropertyEnum = StaticEnum<EDMMaterialPropertyType>();

	TSharedRef<SWrapBox> ChannelPresets = SNew(SWrapBox)
		.UseAllottedSize(true)
		.InnerSlotPadding(WrapBoxSlotPadding)
		.Orientation(EOrientation::Orient_Horizontal);

	if (const FDMMaterialChannelListPreset* Preset = GetDefault<UDynamicMaterialEditorSettings>()->GetPresetByName(CurrentPreset))
	{
		for (const TPair<EDMMaterialPropertyType, UDMMaterialProperty*>& Property : ModelEditorOnlyData->GetMaterialProperties())
		{
			if (Property.Key == EDMMaterialPropertyType::OpacityMask)
			{
				continue;
			}
				
			if (Preset->IsPropertyEnabled(Property.Key))
			{
				constexpr const TCHAR* ShortNameName = TEXT("ShortName");
				const FString ShortName = MaterialPropertyEnum->GetMetaData(ShortNameName, MaterialPropertyEnum->GetIndexByValue(static_cast<int64>(Property.Key)));

				ChannelPresets->AddSlot()
					.Padding(TextPadding)
					[
						SNew(STextBlock)
						.TextStyle(FDynamicMaterialEditorStyle::Get(), "RegularFont")
						.Text(!ShortName.IsEmpty() ? FText::FromString(ShortName) : MaterialPropertyEnum->GetDisplayNameTextByValue(static_cast<int64>(Property.Key)))
					];
			}
		}
	}

	return ChannelPresets;
}

TSharedRef<SWidget> SDMMaterialWizard::CreateAcceptButton()
{
	using namespace UE::DynamicMaterialEditor::Private;

	return SNew(SBox)
		.HAlign(HAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
			.ContentPadding(ButtonPadding)
			.OnClicked(this, &SDMMaterialWizard::Accept_OnClick)
			[
				SNew(STextBlock)
				.TextStyle(FDynamicMaterialEditorStyle::Get(), "RegularFont")
				.Text(LOCTEXT("Continue", "Continue"))
			]
		];
}

ECheckBoxState SDMMaterialWizard::Preset_GetState(FName InPresetName) const
{
	return CurrentPreset == InPresetName
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

void SDMMaterialWizard::Preset_OnChange(ECheckBoxState InState, FName InPresetName)
{
	if (InState == ECheckBoxState::Checked)
	{
		CurrentPreset = InPresetName;

		if (PresetChannelContainer.IsValid())
		{
			PresetChannelContainer->SetContent(CreateChannelList());
		}
	}
}

FReply SDMMaterialWizard::Accept_OnClick()
{
	if (TSharedPtr<SDMEditor> Editor = GetEditor())
	{
		if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
		{
			if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
			{
				AActor* MaterialActor = Editor->GetMaterialActor();
				FDMObjectMaterialProperty MaterialProperty = Editor->GetMaterialObjectProperty();
				Editor->ClearEditor();

				EditorOnlyData->SetChannelListPreset(CurrentPreset);
				EditorOnlyData->OnWizardComplete();

				// Refresh display
				if (MaterialProperty.IsValid())
				{
					Editor->SetMaterialObjectProperty(MaterialProperty);
				}
				else
				{
					Editor->SetMaterialModelBase(MaterialModel);
				}

				Editor->SetMaterialActor(MaterialActor);
			}
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
