// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/SDMMaterialWizard.h"

#include "DMDefs.h"
#include "DynamicMaterialEditorStyle.h"
#include "Engine/EngineTypes.h"
#include "MaterialDomain.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "SDMEditor.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMMaterialWizard"

namespace UE::DynamicMaterialEditor::Private
{
	constexpr float SeparationDistance = 20.f;
	constexpr float TitleContentDistance = 5.f;
	static const FMargin ButtonPadding = FMargin(10.f, 5.f);
	static const FVector2D WrapBoxSlotPadding = FVector2D(5, 5);
}

void SDMMaterialWizard::Construct(const FArguments& InArgs, const TSharedRef<SDMEditor>& InEditor)
{
	EditorWeak = InEditor;

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
				.Text(LOCTEXT("MaterialDomain", "Material Domain"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(EHorizontalAlignment::HAlign_Fill)
			[
				CreateMaterialDomainOptions()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(0.0f, SeparationDistance, 0.0f, TitleContentDistance)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BlendMode", "Blend Mode"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(EHorizontalAlignment::HAlign_Fill)
			[
				CreateBlendModeOptions()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(0.0f, SeparationDistance, 0.0f, TitleContentDistance)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ShadingModel", "Shading Model"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(EHorizontalAlignment::HAlign_Fill)
			[
				CreateShadingModelOptions()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(0.0f, SeparationDistance, 0.0f, TitleContentDistance)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MotionVectors", "Motion Vectors"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(EHorizontalAlignment::HAlign_Fill)
			[
				CreateAnimationOptions()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(0.0f, SeparationDistance, 0.0f, TitleContentDistance)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Geometry", "Geometry"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(EHorizontalAlignment::HAlign_Fill)
			[
				CreateTwoSidedOptions()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(0.0f, SeparationDistance, 0.0f, TitleContentDistance)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Channels", "Channels"))
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
			.HAlign(EHorizontalAlignment::HAlign_Fill)
			[
				CreateChannelList()
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

TSharedRef<SWidget> SDMMaterialWizard::CreateMaterialDomainOptions()
{
	using namespace UE::DynamicMaterialEditor::Private;

	UEnum* DomainEnum = StaticEnum<EMaterialDomain>();

	TSharedRef<SWrapBox> DomainOptions = SNew(SWrapBox)
		.UseAllottedSize(true)
		.InnerSlotPadding(WrapBoxSlotPadding)
		.Orientation(EOrientation::Orient_Horizontal);

	for (EMaterialDomain Domain : UDynamicMaterialModelEditorOnlyData::SupportedDomains)
	{
		DomainOptions->AddSlot()
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.Padding(ButtonPadding)
				.IsEnabled(this, &SDMMaterialWizard::Domain_IsEnabled)
				.IsChecked(this, &SDMMaterialWizard::Domain_GetState, Domain)
				.OnCheckStateChanged(this, &SDMMaterialWizard::Domain_OnChange, Domain)
				[
					SNew(STextBlock)
					.Text(DomainEnum->GetDisplayNameTextByValue(Domain))
				]
			];
	}

	return DomainOptions;
}

TSharedRef<SWidget> SDMMaterialWizard::CreateBlendModeOptions()
{
	using namespace UE::DynamicMaterialEditor::Private;

	UEnum* BlendModeEnum = StaticEnum<EBlendMode>();

	TSharedRef<SWrapBox> BlendModeOptions = SNew(SWrapBox)
		.UseAllottedSize(true)
		.InnerSlotPadding(WrapBoxSlotPadding)
		.Orientation(EOrientation::Orient_Horizontal);

	for (EBlendMode BlendMode : UDynamicMaterialModelEditorOnlyData::SupportedBlendModes)
	{
		BlendModeOptions->AddSlot()
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.Padding(ButtonPadding)
				.IsEnabled(this, &SDMMaterialWizard::BlendMode_IsEnabled)
				.IsChecked(this, &SDMMaterialWizard::BlendMode_GetState, BlendMode)
				.OnCheckStateChanged(this, &SDMMaterialWizard::BlendMode_OnChange, BlendMode)
				[
					SNew(STextBlock)
					.Text(BlendModeEnum->GetDisplayNameTextByValue(BlendMode))
				]
			];
	}

	return BlendModeOptions;
}

TSharedRef<SWidget> SDMMaterialWizard::CreateShadingModelOptions()
{
	using namespace UE::DynamicMaterialEditor::Private;

	UEnum* MaterialShadingModelEnum = StaticEnum<EMaterialShadingModel>();

	TSharedRef<SWrapBox> MaterialShadingModelOptions = SNew(SWrapBox)
		.UseAllottedSize(true)
		.InnerSlotPadding(WrapBoxSlotPadding)
		.Orientation(EOrientation::Orient_Horizontal);

	for (EDMMaterialShadingModel MaterialShadingModel : {EDMMaterialShadingModel::Unlit, EDMMaterialShadingModel::DefaultLit})
	{
		MaterialShadingModelOptions->AddSlot()
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.Padding(ButtonPadding)
				.IsEnabled(this, &SDMMaterialWizard::Unlit_IsEnabled)
				.IsChecked(this, &SDMMaterialWizard::Unlit_GetState, MaterialShadingModel)
				.OnCheckStateChanged(this, &SDMMaterialWizard::Unlit_OnChange, MaterialShadingModel)
				[
					SNew(STextBlock)
					.Text(MaterialShadingModelEnum->GetDisplayNameTextByValue(static_cast<int64>(MaterialShadingModel)))
				]
			];
	}

	return MaterialShadingModelOptions;
}

TSharedRef<SWidget> SDMMaterialWizard::CreateAnimationOptions()
{
	using namespace UE::DynamicMaterialEditor::Private;

	TSharedRef<SWrapBox> AnimatedOptions = SNew(SWrapBox)
		.UseAllottedSize(true)
		.InnerSlotPadding(WrapBoxSlotPadding)
		.Orientation(EOrientation::Orient_Horizontal);

	AnimatedOptions->AddSlot()
		[
			SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.Padding(ButtonPadding)
				.IsEnabled(this, &SDMMaterialWizard::Animated_IsEnabled)
				.IsChecked(this, &SDMMaterialWizard::Animated_GetState, true)
				.OnCheckStateChanged(this, &SDMMaterialWizard::Animated_OnChange, true)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Animated", "Animated"))
				]
		];

	AnimatedOptions->AddSlot()
		[
			SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.Padding(ButtonPadding)
				.IsEnabled(this, &SDMMaterialWizard::Animated_IsEnabled)
				.IsChecked(this, &SDMMaterialWizard::Animated_GetState, false)
				.OnCheckStateChanged(this, &SDMMaterialWizard::Animated_OnChange, false)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Static", "Static"))
				]
		];

	return AnimatedOptions;
}

TSharedRef<SWidget> SDMMaterialWizard::CreateTwoSidedOptions()
{
	using namespace UE::DynamicMaterialEditor::Private;

	TSharedRef<SWrapBox> TwoSidedOptions = SNew(SWrapBox)
		.UseAllottedSize(true)
		.InnerSlotPadding(WrapBoxSlotPadding)
		.Orientation(EOrientation::Orient_Horizontal);

	TwoSidedOptions->AddSlot()
		[
			SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.Padding(ButtonPadding)
				.IsEnabled(this, &SDMMaterialWizard::TwoSided_IsEnabled)
				.IsChecked(this, &SDMMaterialWizard::TwoSided_GetState, true)
				.OnCheckStateChanged(this, &SDMMaterialWizard::TwoSided_OnChange, true)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TwoSided", "Two Sided"))
				]
		];

	TwoSidedOptions->AddSlot()
		[
			SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.Padding(ButtonPadding)
				.IsEnabled(this, &SDMMaterialWizard::TwoSided_IsEnabled)
				.IsChecked(this, &SDMMaterialWizard::TwoSided_GetState, false)
				.OnCheckStateChanged(this, &SDMMaterialWizard::TwoSided_OnChange, false)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("OneSided", "One Sided"))
				]
		];

	return TwoSidedOptions;
}

TSharedRef<SWidget> SDMMaterialWizard::CreateChannelPresets()
{
	using namespace UE::DynamicMaterialEditor::Private;

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.f, 0.f, TitleContentDistance, 0.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Presets", "Presets"))
		]
		+ SHorizontalBox::Slot()
		[
			SNew(STextBlock)
			.Text(INVTEXT("-"))
		];
}

TSharedRef<SWidget> SDMMaterialWizard::CreateChannelList()
{
	return SNew(STextBlock)
		.Text(INVTEXT("Full Channel List"));
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
				.Text(LOCTEXT("Continue", "Continue"))
			]
		];
}

bool SDMMaterialWizard::Domain_IsEnabled() const
{
	if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
	{
		return !!UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);
	}

	return false;
}

ECheckBoxState SDMMaterialWizard::Domain_GetState(EMaterialDomain InDomain) const
{
	if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
	{
		if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
		{
			return EditorOnlyData->GetDomain() == InDomain
				? ECheckBoxState::Checked
				: ECheckBoxState::Unchecked;
		}
	}

	return ECheckBoxState::Undetermined;
}

void SDMMaterialWizard::Domain_OnChange(ECheckBoxState InState, EMaterialDomain InDomain)
{
	if (InState == ECheckBoxState::Checked)
	{
		if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
		{
			if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
			{
				EditorOnlyData->SetDomain(InDomain);
			}
		}
	}
}

bool SDMMaterialWizard::BlendMode_IsEnabled() const
{
	if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
	{
		return !!UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);
	}

	return false;
}

ECheckBoxState SDMMaterialWizard::BlendMode_GetState(EBlendMode InBlendMode) const
{
	if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
	{
		if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
		{
			return EditorOnlyData->GetBlendMode() == InBlendMode
				? ECheckBoxState::Checked
				: ECheckBoxState::Unchecked;
		}
	}

	return ECheckBoxState::Undetermined;
}

void SDMMaterialWizard::BlendMode_OnChange(ECheckBoxState InState, EBlendMode InBlendMode)
{
	if (InState == ECheckBoxState::Checked)
	{
		if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
		{
			if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
			{
				EditorOnlyData->SetBlendMode(InBlendMode);
			}
		}
	}
}

bool SDMMaterialWizard::Unlit_IsEnabled() const
{
	if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
	{
		return !!UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);
	}

	return false;
}

ECheckBoxState SDMMaterialWizard::Unlit_GetState(EDMMaterialShadingModel InValue) const
{
	if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
	{
		if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
		{
			return EditorOnlyData->GetShadingModel() == InValue
				? ECheckBoxState::Checked
				: ECheckBoxState::Unchecked;
		}
	}

	return ECheckBoxState::Undetermined;
}

void SDMMaterialWizard::Unlit_OnChange(ECheckBoxState InState, EDMMaterialShadingModel InValue)
{
	if (InState == ECheckBoxState::Checked)
	{
		if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
		{
			if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
			{
				EditorOnlyData->SetShadingModel(InValue);
			}
		}
	}
}

bool SDMMaterialWizard::Animated_IsEnabled() const
{
	if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
	{
		return !!UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);
	}

	return false;
}

ECheckBoxState SDMMaterialWizard::Animated_GetState(bool bInValue) const
{
	if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
	{
		if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
		{
			return EditorOnlyData->IsPixelAnimationFlagSet() == bInValue
				? ECheckBoxState::Checked
				: ECheckBoxState::Unchecked;
		}
	}

	return ECheckBoxState::Undetermined;
}

void SDMMaterialWizard::Animated_OnChange(ECheckBoxState InState, bool bInValue)
{
	if (InState == ECheckBoxState::Checked)
	{
		if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
		{
			if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
			{
				EditorOnlyData->SetPixelAnimationFlag(bInValue);
			}
		}
	}
}

bool SDMMaterialWizard::TwoSided_IsEnabled() const
{
	if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
	{
		return !!UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);
	}

	return false;
}

ECheckBoxState SDMMaterialWizard::TwoSided_GetState(bool bInValue) const
{
	if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
	{
		if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
		{
			return EditorOnlyData->IsTwoSidedFlagSet() == bInValue
				? ECheckBoxState::Checked
				: ECheckBoxState::Unchecked;
		}
	}

	return ECheckBoxState::Undetermined;
}

void SDMMaterialWizard::TwoSided_OnChange(ECheckBoxState InState, bool bInValue)
{
	if (InState == ECheckBoxState::Checked)
	{
		if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
		{
			if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
			{
				EditorOnlyData->SetTwoSidedFlag(bInValue);
			}
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
				EditorOnlyData->OnWizardComplete();
				Editor->SetMaterialModel(MaterialModel); // Refresh display
			}
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
