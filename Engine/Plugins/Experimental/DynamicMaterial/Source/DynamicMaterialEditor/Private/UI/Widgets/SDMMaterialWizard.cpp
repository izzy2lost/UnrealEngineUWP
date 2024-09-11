// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/SDMMaterialWizard.h"

#include "AssetRegistry/AssetData.h"
#include "AssetTextFilter.h"
#include "AssetThumbnail.h"
#include "DMDefs.h"
#include "DynamicMaterialEditorSettings.h"
#include "DynamicMaterialEditorStyle.h"
#include "DynamicMaterialModule.h"
#include "Engine/EngineTypes.h"
#include "IContentBrowserSingleton.h"
#include "Material/DynamicMaterialInstance.h"
#include "Material/DynamicMaterialInstanceFactory.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "SAssetSearchBox.h"
#include "SAssetView.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"
#include "UI/Widgets/SDMMaterialDesigner.h"
#include "Utils/DMMaterialModelFunctionLibrary.h"
#include "Utils/DMPrivate.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
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
	FCoreDelegates::OnEnginePreExit.RemoveAll(this);

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

	FCoreDelegates::OnEnginePreExit.AddSP(this, &SDMMaterialWizard::OnEnginePreExit);

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
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.VAlign(EVerticalAlignment::VAlign_Fill)
		[
			CreateLayout()
		]
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
					CreateNewTemplateLayout()
				]
				+ SWidgetSwitcher::Slot()
				[
					CreateNewInstanceLayout()
				]
			]
		];
}

TSharedRef<SWidget> SDMMaterialWizard::CreateModeSelector()
{
	TSharedRef<SHorizontalBox> Container = SNew(SHorizontalBox);

	const TArray<EHorizontalAlignment> Alignments = {
		EHorizontalAlignment::HAlign_Right,
		EHorizontalAlignment::HAlign_Left
	};

	const TArray<FMargin> Paddings = {
		FMargin(0.f, 0.f, 5.f, 0.f),
		FMargin(5.f, 0.f, 0.f, 0.f)
	};

	for (int32 SwitcherIndex = 0; SwitcherIndex < EDMMaterialWizardMode::Count; ++SwitcherIndex)
	{
		switch (SwitcherIndex)
		{
			case EDMMaterialWizardMode::Template:
			{
				Container->AddSlot()
					.FillContentWidth(1.f)
					.HAlign(Alignments[SwitcherIndex])
					.Padding(Paddings[SwitcherIndex])
					[
						SNew(SCheckBox)
						.Style(FAppStyle::Get(), "DetailsView.SectionButton")
						.HAlign(EHorizontalAlignment::HAlign_Center)
						.Padding(FMargin(10.f, 6.f))
						.IsChecked(this, &SDMMaterialWizard::IsModeSelected, EDMMaterialWizardMode::Template)
						.OnCheckStateChanged(this, &SDMMaterialWizard::SetMode, EDMMaterialWizardMode::Template)
						.ToolTipText(LOCTEXT("PresetModeToolTip", "Set up a new Material based on simple channel presets."))
						.Content()
						[
							SNew(STextBlock)
							.TextStyle(FDynamicMaterialEditorStyle::Get(), "BoldFont")
							.Text(LOCTEXT("PresetMode", "New Material"))
						]
					];

				break;
			}

			case EDMMaterialWizardMode::Instance:
			{
				Container->AddSlot()
					.FillContentWidth(1.f)
					.HAlign(Alignments[SwitcherIndex])
					.Padding(Paddings[SwitcherIndex])
					[
						SNew(SCheckBox)
						.Style(FAppStyle::Get(), "DetailsView.SectionButton")
						.HAlign(EHorizontalAlignment::HAlign_Center)
						.Padding(FMargin(10.f, 6.f))
						.IsChecked(this, &SDMMaterialWizard::IsModeSelected, EDMMaterialWizardMode::Instance)
						.OnCheckStateChanged(this, &SDMMaterialWizard::SetMode, EDMMaterialWizardMode::Instance)
						.ToolTipText(LOCTEXT("TemplateModeToolTip", "Create a new Dynamic Material Instance based on a template."))
						.Content()
						[
							SNew(STextBlock)
							.TextStyle(FDynamicMaterialEditorStyle::Get(), "BoldFont")
							.Text(LOCTEXT("TemplateMode", "New Material Instance"))
						]
					];

				break;
			}
		}
	}

	return Container;
}

TSharedRef<SWidget> SDMMaterialWizard::CreateNewTemplateLayout()
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
			CreateNewTemplate_ChannelPresets()
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
				CreateNewTemplate_ChannelList()
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		.Padding(0.0f, SeparationDistance, 0.0f, 0.f)
		[
			CreateNewTemplate_AcceptButton()
		];
}

TSharedRef<SWidget> SDMMaterialWizard::CreateNewTemplate_ChannelPresets()
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

TSharedRef<SWidget> SDMMaterialWizard::CreateNewTemplate_ChannelList()
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

TSharedRef<SWidget> SDMMaterialWizard::CreateNewInstanceLayout()
{
	FAssetPickerConfig PickerConfig;
	PickerConfig.SelectionMode = ESelectionMode::Single;
	PickerConfig.Filter.ClassPaths.Add(FTopLevelAssetPath(UDynamicMaterialInstance::StaticClass()));
	PickerConfig.Filter.bIncludeOnlyOnDiskAssets = true;
	PickerConfig.ThumbnailLabel = EThumbnailLabel::AssetName;
	PickerConfig.OnShouldFilterAsset.BindSP(this, &SDMMaterialWizard::ShouldFilterOutAsset);
	PickerConfig.InitialAssetViewType = EAssetViewType::Tile;
	PickerConfig.bAutohideSearchBar = false;
	PickerConfig.bFocusSearchBoxWhenOpened = false;
	PickerConfig.bShowBottomToolbar = false;
	PickerConfig.bAllowDragging = false;
	PickerConfig.bAllowRename = false;
	PickerConfig.bCanShowClasses = false;
	PickerConfig.bCanShowFolders = true;
	PickerConfig.bCanShowReadOnlyFolders = true;
	PickerConfig.bCanShowRealTimeThumbnails = true;
	PickerConfig.bCanShowDevelopersFolder = true;
	PickerConfig.bForceShowEngineContent = true;
	PickerConfig.bForceShowPluginContent = true;
	PickerConfig.bAddFilterUI = false;

	PickerConfig.Filter.PackagePaths.Reset();

	TextFilter = MakeShared<FAssetTextFilter>();
	TextFilter->SetIncludeClassName(true);
	TextFilter->SetIncludeAssetPath(false);
	TextFilter->SetIncludeCollectionNames(false);

	AssetView = SNew(SAssetView)
		.InitialCategoryFilter(EContentBrowserItemCategoryFilter::IncludeAssets)
		.SelectionMode(PickerConfig.SelectionMode)
		.OnShouldFilterAsset(PickerConfig.OnShouldFilterAsset)
		.OnItemsActivated(this, &SDMMaterialWizard::OnAssetsActivated)
		.OnIsAssetValidForCustomToolTip(PickerConfig.OnIsAssetValidForCustomToolTip)
		.OnGetCustomAssetToolTip(PickerConfig.OnGetCustomAssetToolTip)
		.OnVisualizeAssetToolTip(PickerConfig.OnVisualizeAssetToolTip)
		.OnAssetToolTipClosing(PickerConfig.OnAssetToolTipClosing)
		.InitialSourcesData(FSourcesData())
		.InitialBackendFilter(PickerConfig.Filter)
		.InitialViewType(PickerConfig.InitialAssetViewType)
		.InitialAssetSelection(PickerConfig.InitialAssetSelection)
		.ShowBottomToolbar(PickerConfig.bShowBottomToolbar)
		.OnAssetTagWantsToBeDisplayed(PickerConfig.OnAssetTagWantsToBeDisplayed)
		.OnGetCustomSourceAssets(PickerConfig.OnGetCustomSourceAssets)
		.AllowDragging(PickerConfig.bAllowDragging)
		.CanShowClasses(PickerConfig.bCanShowClasses)
		.CanShowFolders(PickerConfig.bCanShowFolders)
		.CanShowReadOnlyFolders(PickerConfig.bCanShowReadOnlyFolders)
		.ShowPathInColumnView(PickerConfig.bShowPathInColumnView)
		.ShowTypeInColumnView(PickerConfig.bShowTypeInColumnView)
		.ShowViewOptions(false)
		.SortByPathInColumnView(PickerConfig.bSortByPathInColumnView)
		.FilterRecursivelyWithBackendFilter(false)
		.CanShowRealTimeThumbnails(PickerConfig.bCanShowRealTimeThumbnails)
		.CanShowDevelopersFolder(PickerConfig.bCanShowDevelopersFolder)
		.ForceShowEngineContent(PickerConfig.bForceShowEngineContent)
		.ForceShowPluginContent(PickerConfig.bForceShowPluginContent)
		.HighlightedText(TAttribute<FText>(this, &SDMMaterialWizard::GetSearchText))
		.ThumbnailLabel(PickerConfig.ThumbnailLabel)
		.AssetShowWarningText(PickerConfig.AssetShowWarningText)
		.AllowFocusOnSync(false)
		.HiddenColumnNames(PickerConfig.HiddenColumnNames)
		.CustomColumns(PickerConfig.CustomColumns)
		.InitialThumbnailSize(EThumbnailSize::Small)
		.ShowTypeInTileView(false)
		.TextFilter(TextFilter);

	AssetView->RequestSlowFullListRefresh();

	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Menu.Background"))
		.Padding(3.f, 3.f, 3.f, 3.f)
		.VAlign(EVerticalAlignment::VAlign_Fill)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f, 0.f, 3.f)
			[
				SAssignNew(AssetSearchBox, SAssetSearchBox)
				.HintText(NSLOCTEXT("ContentBrowser", "SearchBoxHint", "Search Assets"))
				.OnTextChanged(this, &SDMMaterialWizard::OnSearchBoxChanged)
				.OnTextCommitted(this, &SDMMaterialWizard::OnSearchBoxCommitted)
				.DelayChangeNotificationsWhileTyping(true)
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				AssetView.ToSharedRef()
			]
		];
}

TSharedRef<SWidget> SDMMaterialWizard::CreateNewTemplate_AcceptButton()
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
			PresetChannelContainer->SetContent(CreateNewTemplate_ChannelList());
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

ECheckBoxState SDMMaterialWizard::IsModeSelected(EDMMaterialWizardMode InMode) const
{
	if (Switcher.IsValid() && Switcher->GetActiveWidgetIndex() == InMode)
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}

void SDMMaterialWizard::SetMode(ECheckBoxState InState, EDMMaterialWizardMode InMode)
{
	if (InState == ECheckBoxState::Checked && Switcher.IsValid())
	{
		Switcher->SetActiveWidgetIndex(InMode);
	}
}

void SDMMaterialWizard::OnSearchBoxChanged(const FText& InSearchText)
{
	SetSearchText(InSearchText);
}

void SDMMaterialWizard::OnSearchBoxCommitted(const FText& InSearchText, ETextCommit::Type InCommitInfo)
{
	SetSearchText(InSearchText);
}

FText SDMMaterialWizard::GetSearchText() const
{
	if (TextFilter.IsValid())
	{
		return TextFilter->GetRawFilterText();
	}

	return FText::GetEmpty();
}

void SDMMaterialWizard::SetSearchText(const FText& InSearchText)
{
	if (InSearchText.ToString().Equals(TextFilter->GetRawFilterText().ToString(), ESearchCase::CaseSensitive))
	{
		return;
	}

	TextFilter->SetRawFilterText(InSearchText);
	AssetView->SetUserSearching(!InSearchText.IsEmpty());
}

bool SDMMaterialWizard::ShouldFilterOutAsset(const FAssetData& InAsset) const
{
	UDynamicMaterialInstance* MaterialInstance = Cast<UDynamicMaterialInstance>(InAsset.GetAsset());

	if (!MaterialInstance)
	{
		return true;
	}

	UDynamicMaterialModelBase* AssetMaterialModelBase = MaterialInstance->GetMaterialModelBase();

	if (!AssetMaterialModelBase)
	{
		return true;
	}

	// Only non-dynamic models can be used as a basis.
	if (!AssetMaterialModelBase->IsA<UDynamicMaterialModel>())
	{
		return true;
	}

	if (UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get())
	{
		// Can't use it off ourselves
		if (AssetMaterialModelBase == MaterialModel)
		{
			return true;
		}
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(AssetMaterialModelBase);

	if (!EditorOnlyData)
	{
		return true;
	}

	// Can't base it off things which also need a wizard.
	if (EditorOnlyData->NeedsWizard())
	{
		return true;
	}

	return false;
}

void SDMMaterialWizard::OnAssetsActivated(TArrayView<const FContentBrowserItem> InSelectedItems, EAssetTypeActivationMethod::Type InActivationMethod)
{
	if (InSelectedItems.IsEmpty() || InActivationMethod == EAssetTypeActivationMethod::Previewed)
	{
		return;
	}

	FAssetData AssetData;

	if (!InSelectedItems[0].Legacy_TryGetAssetData(AssetData))
	{
		return;
	}

	UDynamicMaterialInstance* Instance = Cast<UDynamicMaterialInstance>(AssetData.GetAsset());

	if (!Instance)
	{
		return;
	}

	UDynamicMaterialModel* MaterialModel = Cast<UDynamicMaterialModel>(Instance->GetMaterialModelBase());

	if (!MaterialModel)
	{
		return;
	}

	SelectTemplate(MaterialModel);
}

void SDMMaterialWizard::OnEnginePreExit()
{
	TextFilter.Reset();
	AssetSearchBox.Reset();
	AssetView.Reset();
	Switcher.Reset();

	ChildSlot.DetachWidget();
}

void SDMMaterialWizard::SelectTemplate(UDynamicMaterialModel* InTemplateModel)
{
	TSharedPtr<SDMMaterialDesigner> DesignerWidget = GetDesignerWidget();

	if (!DesignerWidget.IsValid())
	{
		return;
	}

	if (UDynamicMaterialModel* MaterialModel = GetMaterialModel())
	{
		if (UDynamicMaterialInstance* Instance = MaterialModel->GetDynamicMaterialInstance())
		{
			CreateDynamicMaterialInInstance(InTemplateModel, Instance);
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
				CreateDynamicMaterialInInstance(InTemplateModel, Instance);
			}
			else
			{
				CreateNewDynamicInstanceInActor(InTemplateModel, MaterialObjectProperty.GetValue());
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
