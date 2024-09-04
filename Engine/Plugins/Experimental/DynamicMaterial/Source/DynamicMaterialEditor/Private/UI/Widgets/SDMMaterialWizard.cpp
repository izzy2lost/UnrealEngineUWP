// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/SDMMaterialWizard.h"

#include "AssetRegistry/AssetData.h"
#include "AssetThumbnail.h"
#include "DMDefs.h"
#include "DynamicMaterialEditorSettings.h"
#include "DynamicMaterialEditorStyle.h"
#include "DynamicMaterialModule.h"
#include "Engine/EngineTypes.h"
#include "Material/DynamicMaterialInstance.h"
#include "Material/DynamicMaterialInstanceFactory.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "UI/Widgets/SDMMaterialDesigner.h"
#include "Utils/DMMaterialModelFunctionLibrary.h"
#include "Utils/DMPrivate.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
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

	if (UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get())
	{
		if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
		{
			EditorOnlyData->GetOnMaterialBuiltDelegate().RemoveAll(this);
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

TArray<FAssetData> SDMMaterialWizard::GetTemplateMaterials()
{
	if (const UDynamicMaterialEditorSettings* Settings = GetDefault<UDynamicMaterialEditorSettings>())
	{
		return Settings->GetTemplateList();
	}

	return {};
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
			.Padding(0.0f, SeparationDistance, 0.0f, 0.f)
			[
				CreateModeSelector()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, SeparationDistance, 0.0f, 0.f)
			[
				SAssignNew(Switcher, SWidgetSwitcher)
				+ SWidgetSwitcher::Slot()
				[
					CreateTemplateListLayout()
				]
				+ SWidgetSwitcher::Slot()
				[
					CreateSelectPresetLayout()
				]
			]
		];
}

TSharedRef<SWidget> SDMMaterialWizard::CreateModeSelector()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillContentWidth(1.f)
		.HAlign(EHorizontalAlignment::HAlign_Right)
		.Padding(0.f, 0.f, 5.f, 0.f)
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "DetailsView.SectionButton")
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.Padding(FMargin(10.f, 6.f))
			.IsChecked(this, &SDMMaterialWizard::IsModeSelected, 0)
			.OnCheckStateChanged(this, &SDMMaterialWizard::SetMode, 0)
			.ToolTipText(LOCTEXT("TemplateModeToolTip", "Create a new Dynamic Material Instance based on a template."))
			.Content()
			[
				SNew(STextBlock)
				.TextStyle(FDynamicMaterialEditorStyle::Get(), "BoldFont")
				.Text(LOCTEXT("TemplateMode", "New Instance"))
			]
		]

		+ SHorizontalBox::Slot()
		.FillContentWidth(1.f)
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.Padding(5.f, 0.f, 0.f, 0.f)
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "DetailsView.SectionButton")
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.Padding(FMargin(10.f, 6.f))
			.IsChecked(this, &SDMMaterialWizard::IsModeSelected, 1)
			.OnCheckStateChanged(this, &SDMMaterialWizard::SetMode, 1)
			.ToolTipText(LOCTEXT("PresetModeToolTip", "Set up a new Material based on simple channel presets."))
			.Content()
			[
				SNew(STextBlock)
				.TextStyle(FDynamicMaterialEditorStyle::Get(), "BoldFont")
				.Text(LOCTEXT("PresetMode", "New Template"))
			]
		];
}

TSharedRef<SWidget> SDMMaterialWizard::CreateSelectPresetLayout()
{
	using namespace UE::DynamicMaterialDesigner::Private;

	return SNew(SVerticalBox)
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
			CreateSelectPreset_ChannelPresets()
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
				CreateSelectPreset_ChannelList()
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		.Padding(0.0f, SeparationDistance, 0.0f, 0.f)
		[
			CreateSelectPreset_AcceptButton()
		];
}

TSharedRef<SWidget> SDMMaterialWizard::CreateSelectPreset_ChannelPresets()
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

TSharedRef<SWidget> SDMMaterialWizard::CreateSelectPreset_ChannelList()
{
	using namespace UE::DynamicMaterialDesigner::Private;

	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(GetMaterialModel());

	if (!ModelEditorOnlyData)
	{
		return SNullWidget::NullWidget;
	}

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
				ChannelPresets->AddSlot()
					.Padding(TextPadding)
					[
						SNew(STextBlock)
						.TextStyle(FDynamicMaterialEditorStyle::Get(), "RegularFont")
						.Text(UE::DynamicMaterialEditor::Private::GetMaterialPropertyShortDisplayName(Property.Key))
					];
			}
		}
	}

	return ChannelPresets;
}

TSharedRef<SWidget> SDMMaterialWizard::CreateTemplateListLayout()
{
	TSharedRef<SWrapBox> WrapBox = SNew(SWrapBox)
		.InnerSlotPadding(FVector2D(5.f))
		.UseAllottedSize(true);

	TArray<FAssetData> Templates = GetTemplateMaterials();

	for (const FAssetData& Template : Templates)
	{
		WrapBox->AddSlot()
			[
				CreateTemplateList_Entry(Template)
			];
	}

	return WrapBox;
}

TSharedRef<SWidget> SDMMaterialWizard::CreateTemplateList_Entry(const FAssetData& InTemplateAsset)
{
	Assets.Add(InTemplateAsset);

	TSharedRef<FAssetThumbnail> Thumbnail = MakeShared<FAssetThumbnail>(InTemplateAsset, 100.f, 100.f, UThumbnailManager::Get().GetSharedThumbnailPool());
	Thumbnails.Add(Thumbnail);

	TSharedRef<SWidget> Entry = SNew(SBox)
		.WidthOverride(100.f)
		.HeightOverride(123.f)
		.Cursor(EMouseCursor::Hand)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				Thumbnail->MakeThumbnailWidget()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 5.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.WrapTextAt(100.f)
				.WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
				.TextStyle(FDynamicMaterialEditorStyle::Get(), "RegularFont")
				.Text(FText::FromName(InTemplateAsset.AssetName))
			]
		];

	Entry->SetOnMouseButtonDown(FPointerEventHandler::CreateSP(this, &SDMMaterialWizard::OnTemplateMouseDown, Assets.Num() - 1));

	return Entry;
}

TSharedRef<SWidget> SDMMaterialWizard::CreateSelectPreset_AcceptButton()
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
			PresetChannelContainer->SetContent(CreateSelectPreset_ChannelList());
		}
	}
}

FReply SDMMaterialWizard::Accept_OnClick()
{
	if (CurrentPreset.IsNone())
	{
		FReply::Handled();
	}

	TSharedPtr<SDMMaterialDesigner> DesignerWidget = GetDesignerWidget();

	if (!DesignerWidget.IsValid())
	{
		return FReply::Handled();
	}

	if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
	{
		SetChannelListInModel(CurrentPreset, MaterialModel);
	}
	else if (MaterialObjectProperty.IsSet())
	{
		if (MaterialObjectProperty->IsValid())
		{
			CreateTemplateMaterialInActor(CurrentPreset, MaterialObjectProperty.GetValue());
		}
		else
		{
			UE::DynamicMaterialEditor::Private::LogError(TEXT("Invalid actor property to create new template in."));

			DesignerWidget->ShowSelectPrompt();
		}
	}
	else
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Missing material information for new template."));

		DesignerWidget->ShowSelectPrompt();
	}

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

	DesignerWidget->Empty();

	if (MaterialObjectProperty.IsSet())
	{
		DesignerWidget->OpenObjectMaterialProperty(MaterialObjectProperty.GetValue());
	}
	else if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
	{
		DesignerWidget->OpenMaterialModelBase(MaterialModel);
	}
}

ECheckBoxState SDMMaterialWizard::IsModeSelected(int32 InMode) const
{
	if (Switcher.IsValid() && Switcher->GetActiveWidgetIndex() == InMode)
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}

void SDMMaterialWizard::SetMode(ECheckBoxState InState, int32 InMode)
{
	if (InState == ECheckBoxState::Checked && Switcher.IsValid())
	{
		Switcher->SetActiveWidgetIndex(InMode);
	}
}

FReply SDMMaterialWizard::OnTemplateMouseDown(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent, int32 InAssetIndex)
{
	if (!Assets.IsValidIndex(InAssetIndex))
	{
		return FReply::Handled();
	}

	TSharedPtr<SDMMaterialDesigner> DesignerWidget = GetDesignerWidget();

	if (!DesignerWidget.IsValid())
	{
		return FReply::Handled();
	}

	UDynamicMaterialInstance* TemplateInstance = Cast<UDynamicMaterialInstance>(Assets[InAssetIndex].GetAsset());

	if (!TemplateInstance)
	{
		return FReply::Handled();
	}

	UDynamicMaterialModel* TemplateModel = Cast<UDynamicMaterialModel>(TemplateInstance->GetMaterialModelBase());

	if (!TemplateModel)
	{
		return FReply::Handled();
	}

	if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
	{
		if (UDynamicMaterialInstance* Instance = MaterialModel->GetDynamicMaterialInstance())
		{
			CreateDynamicMaterialInInstance(TemplateModel, Instance);
		}
		else
		{
			UE::DynamicMaterialEditor::Private::LogError(TEXT("Unable to find material instance to create new dynamic material in."));

			DesignerWidget->ShowSelectPrompt();
		}
	}
	else if (MaterialObjectProperty.IsSet())
	{
		if (MaterialObjectProperty->IsValid())
		{
			if (UDynamicMaterialInstance* Instance = MaterialObjectProperty->GetMaterial())
			{
				CreateDynamicMaterialInInstance(TemplateModel, Instance);
			}
			else
			{
				CreateNewDynamicInstanceInActor(TemplateModel, MaterialObjectProperty.GetValue());
			}
		}
		else
		{
			UE::DynamicMaterialEditor::Private::LogError(TEXT("Invalid actor property to create new dynamic material in."));

			DesignerWidget->ShowSelectPrompt();
		}
	}
	else
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Missing material information for dynamic material."));

		DesignerWidget->ShowSelectPrompt();
	}

	return FReply::Handled();
}

void SDMMaterialWizard::CreateDynamicMaterialInInstance(UDynamicMaterialModel* InTemplateModel, UDynamicMaterialInstance* InToInstance)
{
	TSharedPtr<SDMMaterialDesigner> DesignerWidget = GetDesignerWidget();

	if (!DesignerWidget.IsValid())
	{
		return;
	}

	if (!UDMMaterialModelFunctionLibrary::CreateDynamicModelInInstance(InTemplateModel, InToInstance))
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Failed to create new dynamic model in existing instance."));
		return;
	}

	DesignerWidget->OpenMaterialModelBase(InToInstance->GetMaterialModelBase());
}

void SDMMaterialWizard::CreateNewDynamicInstanceInActor(UDynamicMaterialModel* InFromModel, FDMObjectMaterialProperty& InMaterialObjectProperty)
{
	TSharedPtr<SDMMaterialDesigner> DesignerWidget = GetDesignerWidget();

	if (!DesignerWidget.IsValid())
	{
		return;
	}

	UObject* Outer = InMaterialObjectProperty.GetOuter();

	if (!Outer)
	{
		return;
	}

	UDynamicMaterialInstanceFactory* Factory = NewObject<UDynamicMaterialInstanceFactory>(GetTransientPackage());

	UDynamicMaterialInstance* NewInstance = Cast<UDynamicMaterialInstance>(Factory->FactoryCreateNew(
		UDynamicMaterialInstance::StaticClass(),
		Outer,
		NAME_None,
		RF_Transactional | RF_Public,
		nullptr,
		GWarn
	));

	if (!NewInstance)
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Failed to create new material instance."));
		return;
	}

	if (!UDMMaterialModelFunctionLibrary::CreateDynamicModelInInstance(InFromModel, NewInstance))
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Failed to create new dynamic model in new instance."));
		return;
	}

	InMaterialObjectProperty.SetMaterial(NewInstance);

	DesignerWidget->OpenObjectMaterialProperty(InMaterialObjectProperty);
}

void SDMMaterialWizard::SetChannelListInModel(FName InChannelList, UDynamicMaterialModel* InMaterialModel)
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

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);

	if (!EditorOnlyData)
	{
		return;
	}

	EditorOnlyData->GetOnMaterialBuiltDelegate().RemoveAll(this);
	EditorOnlyData->SetChannelListPreset(InChannelList);
	EditorOnlyData->OnWizardComplete();

	DesignerWidget->OpenMaterialModelBase(MaterialModel);
}

void SDMMaterialWizard::CreateTemplateMaterialInActor(FName InChannelList, FDMObjectMaterialProperty& InMaterialObjectProperty)
{
	TSharedPtr<SDMMaterialDesigner> DesignerWidget = GetDesignerWidget();

	if (!DesignerWidget.IsValid())
	{
		return;
	}

	UObject* Outer = InMaterialObjectProperty.GetOuter();

	if (!Outer)
	{
		return;
	}

	UDynamicMaterialInstanceFactory* Factory = NewObject<UDynamicMaterialInstanceFactory>(GetTransientPackage());

	UDynamicMaterialInstance* NewInstance = Cast<UDynamicMaterialInstance>(Factory->FactoryCreateNew(
		UDynamicMaterialInstance::StaticClass(),
		Outer,
		NAME_None,
		RF_Transactional | RF_Public,
		nullptr,
		GWarn
	));

	if (!NewInstance)
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Failed to create new material instance."));
		return;
	}

	if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(NewInstance))
	{
		EditorOnlyData->SetChannelListPreset(InChannelList);
		EditorOnlyData->OnWizardComplete();
	}

	InMaterialObjectProperty.SetMaterial(NewInstance);

	DesignerWidget->OpenMaterialModelBase(NewInstance->GetMaterialModelBase());
}

#undef LOCTEXT_NAMESPACE
