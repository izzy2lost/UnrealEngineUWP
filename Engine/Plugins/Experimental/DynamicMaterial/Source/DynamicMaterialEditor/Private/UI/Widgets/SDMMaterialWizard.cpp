// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/SDMMaterialWizard.h"

#include "DMDefs.h"
#include "DynamicMaterialEditorSettings.h"
#include "DynamicMaterialEditorStyle.h"
#include "DynamicMaterialModule.h"
#include "Engine/EngineTypes.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"
#include "UI/Widgets/SDMMaterialDesigner.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMMaterialWizard"

namespace UE::DynamicMaterialDesigner::Private
{
	constexpr float SeparationDistance = 20.f;
	constexpr float TitleContentDistance = 5.f;
	static const FMargin ButtonPadding = FMargin(10.f, 5.f);
	static const FMargin TextPadding = FMargin(5.f, 2.f);
	static const FVector2D WrapBoxSlotPadding = FVector2D(5, 5);
}

void SDMMaterialWizard::PrivateRegisterAttributes(FSlateAttributeDescriptor::FInitializer&)
{
}

SDMMaterialWizard::~SDMMaterialWizard()
{
	if (!FDynamicMaterialModule::AreUObjectsSafe())
	{
		return;
	}

	if (TSharedPtr<SDMMaterialDesigner> DesignerWidget = DesignerWidgetWeak.Pin())
	{
		if (UDynamicMaterialModel* MaterialModel = Cast<UDynamicMaterialModel>(DesignerWidget->GetMaterialModelBase()))
		{
			if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
			{
				EditorOnlyData->GetOnMaterialBuiltDelegate().RemoveAll(this);
			}
		}
	}
}

void SDMMaterialWizard::Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialDesigner>& InDesignerWidget)
{
	DesignerWidgetWeak = InDesignerWidget;
	MaterialModelWeak = InArgs._MaterialModel;
	MaterialObjectProperty = InArgs._MaterialProperty;

	SetCanTick(false);

	if (MaterialObjectProperty.IsSet())
	{
		if (UDynamicMaterialModelBase* MaterialModelBase = MaterialObjectProperty.GetValue().GetMaterialModelBase())
		{
			if (UDynamicMaterialModel* MaterialModel = Cast<UDynamicMaterialModel>(MaterialModelBase))
			{
				// Override any parameter given.
				MaterialModelWeak = MaterialModel;
			}
		}
	}

	if (const UDynamicMaterialEditorSettings* Settings = GetDefault<UDynamicMaterialEditorSettings>())
	{
		if (!Settings->MaterialChannelPresets.IsEmpty())
		{
			CurrentPreset = Settings->MaterialChannelPresets[0].Name;
		}
	}

	if (UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get())
	{
		if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
		{
			// Subscribe to this in case the wizard completes externally and this widget is no longer needed.
			EditorOnlyData->GetOnMaterialBuiltDelegate().AddSP(this, &SDMMaterialWizard::OnMaterialBuilt);
		}
	}

	ChildSlot
	[
		CreateLayout()
	];
}

TSharedPtr<SDMMaterialDesigner> SDMMaterialWizard::GetDesignerWidget() const
{
	return DesignerWidgetWeak.Pin();
}

UDynamicMaterialModel* SDMMaterialWizard::GetMaterialModel() const
{
	return MaterialModelWeak.Get();
}

TSharedRef<SWidget> SDMMaterialWizard::CreateLayout()
{
	using namespace UE::DynamicMaterialDesigner::Private;

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
	using namespace UE::DynamicMaterialDesigner::Private;

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
	using namespace UE::DynamicMaterialDesigner::Private;

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
	using namespace UE::DynamicMaterialDesigner::Private;

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
	UDynamicMaterialModel* MaterialModel = GetMaterialModel();

	if (!MaterialModel)
	{
		return FReply::Handled();
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);

	if (!EditorOnlyData)
	{
		return FReply::Handled();
	}

	EditorOnlyData->GetOnMaterialBuiltDelegate().RemoveAll(this);
	EditorOnlyData->SetChannelListPreset(CurrentPreset);
	EditorOnlyData->OnWizardComplete();

	OpenMaterialInEditor();

	return FReply::Handled();
}

void SDMMaterialWizard::OnMaterialBuilt(UDynamicMaterialModelBase* InMaterialModel)
{
	UDynamicMaterialModel* MaterialModel = GetMaterialModel();

	if (!MaterialModel)
	{
		return;
	}

	if (InMaterialModel != MaterialModel)
	{
		return;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);

	if (!EditorOnlyData)
	{
		return;
	}

	if (!EditorOnlyData->NeedsWizard())
	{
		EditorOnlyData->GetOnMaterialBuiltDelegate().RemoveAll(this);
		OpenMaterialInEditor();
	}
}

void SDMMaterialWizard::OpenMaterialInEditor()
{
	TSharedPtr<SDMMaterialDesigner> DesignerWidget = GetDesignerWidget();

	if (!DesignerWidget.IsValid())
	{
		return;
	}

	UDynamicMaterialModel* MaterialModel = GetMaterialModel();

	if (!MaterialModel)
	{
		return;
	}

	DesignerWidget->Empty();

	if (MaterialObjectProperty.IsSet())
	{
		DesignerWidget->OpenObjectMaterialProperty(MaterialObjectProperty.GetValue());
	}
	else
	{
		DesignerWidget->OpenMaterialModelBase(MaterialModel);
	}
}

#undef LOCTEXT_NAMESPACE
