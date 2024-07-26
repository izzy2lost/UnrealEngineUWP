// Copyright Epic Games, Inc. All Rights Reserved.

#include "SColorGradingColorWheelPanel.h"

#include "ColorGradingCommands.h"
#include "ColorGradingPanelState.h"
#include "SColorGradingColorWheel.h"
#include "DetailView/SColorGradingDetailView.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Modules/ModuleManager.h"
#include "PropertyHandle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SSplitter.h"
 
#define LOCTEXT_NAMESPACE "ColorGradingEditor"

SColorGradingColorWheelPanel::~SColorGradingColorWheelPanel()
{
	if (ColorGradingDataModel)
	{
		ColorGradingDataModel->OnColorGradingGroupSelectionChanged().RemoveAll(this);
		ColorGradingDataModel->OnColorGradingElementSelectionChanged().RemoveAll(this);
	}
}

void SColorGradingColorWheelPanel::Construct(const FArguments& InArgs)
{
	ColorGradingDataModel = InArgs._ColorGradingDataModelSource;

	if (ColorGradingDataModel)
	{
		ColorGradingDataModel->OnColorGradingGroupSelectionChanged().AddSP(this, &SColorGradingColorWheelPanel::OnColorGradingGroupSelectionChanged);
		ColorGradingDataModel->OnColorGradingElementSelectionChanged().AddSP(this, &SColorGradingColorWheelPanel::OnColorGradingElementSelectionChanged);
	}

	CommandList = MakeShared<FUICommandList>();
	BindCommands();

	ColorWheelOrientation = EOrientation::Orient_Vertical;

	ColorWheels.AddDefaulted(NumColorWheels);
	HiddenColorWheels.AddZeroed(NumColorWheels);

	for (int32 Index = 0; Index < NumColorWheels; ++Index)
    ChildSlot
	[
		SNew(SVerticalBox)

		// Message indicating that multi select is unavailable in this panel
		+ SVerticalBox::Slot()
		[
			SNew(SBox)
			.Visibility(this, &SColorGradingColorWheelPanel::GetMultiSelectWarningVisibility)
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MultiSelectWarning", "Multi-select editing is unavailable in the Color Grading panel."))
			]
		]

		// Color wheel panel
		+ SVerticalBox::Slot()
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			.PhysicalSplitterHandleSize(1.0f)
			.HitDetectionSplitterHandleSize(5.0f)
			.Style(FAppStyle::Get(), "DetailsView.Splitter")
			.Visibility(this, &SColorGradingColorWheelPanel::GetColorWheelPanelVisibility)

			+ SSplitter::Slot()
			.Value(0.8f)
			[
				SNew(SVerticalBox)

				// Toolbar slot
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(6, 4)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(ColorGradingGroupPropertyBox, SBox)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SSpacer)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(ColorGradingElementsToolBarBox, SHorizontalBox)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SSpacer)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						MakeColorDisplayModeCheckbox()
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(5.0f, 0.0f, 0.0f, 0.0f)
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						SNew(SComboButton)
						.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
						.OnGetMenuContent(this, &SColorGradingColorWheelPanel::MakeSettingsMenu)
						.HasDownArrow(false)
						.ContentPadding(FMargin(1.0f, 0.0f))
						.ButtonContent()
						[
							SNew(SImage)
							.ColorAndOpacity(FSlateColor::UseForeground())
							.Image(FAppStyle::Get().GetBrush("Icons.Settings"))
						]
					]
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(6, 4)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.HAlign(HAlign_Fill)
					.Padding(2, 0)
					[
						SAssignNew(ColorWheels[0], SColorGradingColorWheel)
						.ColorDisplayMode(this, &SColorGradingColorWheelPanel::GetColorDisplayMode)
						.Orientation(ColorWheelOrientation)
						.Visibility(this, &SColorGradingColorWheelPanel::GetColorWheelVisibility, 0)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.HAlign(HAlign_Fill)
					.Padding(2, 0)
					[
						SAssignNew(ColorWheels[1], SColorGradingColorWheel)
						.ColorDisplayMode(this, &SColorGradingColorWheelPanel::GetColorDisplayMode)
						.Orientation(ColorWheelOrientation)
						.Visibility(this, &SColorGradingColorWheelPanel::GetColorWheelVisibility, 1)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.HAlign(HAlign_Fill)
					.Padding(2, 0)
					[
						SAssignNew(ColorWheels[2], SColorGradingColorWheel)
						.ColorDisplayMode(this, &SColorGradingColorWheelPanel::GetColorDisplayMode)
						.Orientation(ColorWheelOrientation)
						.Visibility(this, &SColorGradingColorWheelPanel::GetColorWheelVisibility, 2)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.HAlign(HAlign_Fill)
					.Padding(2, 0)
					[
						SAssignNew(ColorWheels[3], SColorGradingColorWheel)
						.ColorDisplayMode(this, &SColorGradingColorWheelPanel::GetColorDisplayMode)
						.Orientation(ColorWheelOrientation)
						.Visibility(this, &SColorGradingColorWheelPanel::GetColorWheelVisibility, 3)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.HAlign(HAlign_Fill)
					.Padding(2, 0)
					[
						SAssignNew(ColorWheels[4], SColorGradingColorWheel)
						.ColorDisplayMode(this, &SColorGradingColorWheelPanel::GetColorDisplayMode)
						.Orientation(ColorWheelOrientation)
						.Visibility(this, &SColorGradingColorWheelPanel::GetColorWheelVisibility, 4)
					]
				]
			]

			+ SSplitter::Slot()
			.Value(0.2f)
			[
				SAssignNew(DetailView, SColorGradingDetailView)
				.PropertyRowGeneratorSource(ColorGradingDataModel->GetPropertyRowGenerator())
				.OnFilterDetailTreeNode(this, &SColorGradingColorWheelPanel::FilterDetailTreeNode)
			]
		]
	];
}

void SColorGradingColorWheelPanel::Refresh()
{
	if (ColorGradingDataModel)
	{
		if (FColorGradingEditorDataModel::FColorGradingGroup* ColorGradingGroup = ColorGradingDataModel->GetSelectedColorGradingGroup())
		{
			FillColorGradingGroupProperty(*ColorGradingGroup);
			FillColorGradingElementsToolBar(ColorGradingGroup->ColorGradingElements);

			if (const FColorGradingEditorDataModel::FColorGradingElement* ColorGradingElement = ColorGradingDataModel->GetSelectedColorGradingElement())
			{
				FillColorWheels(*ColorGradingElement);
			}
			else
			{
				ClearColorWheels();
			}
		}
		else
		{
			ClearColorGradingGroupProperty();
			ClearColorGradingElementsToolBar();
			ClearColorWheels();
		}

		DetailView->Refresh();
	}
}

void SColorGradingColorWheelPanel::GetPanelState(FColorGradingPanelState& OutPanelState)
{
	OutPanelState.HiddenColorWheels = HiddenColorWheels;
	OutPanelState.ColorDisplayMode = ColorDisplayMode;
	OutPanelState.ColorWheelOrientation = ColorWheelOrientation;
}

void SColorGradingColorWheelPanel::SetPanelState(const FColorGradingPanelState& InPanelState)
{
	// TODO: These could also be output to a config file to be stored between runs
	HiddenColorWheels = InPanelState.HiddenColorWheels;
	ColorDisplayMode = InPanelState.ColorDisplayMode;
	SetColorWheelOrientation(InPanelState.ColorWheelOrientation);
}

void SColorGradingColorWheelPanel::BindCommands()
{
	const FColorGradingCommands& Commands = FColorGradingCommands::Get();

	CommandList->MapAction(
		Commands.SaturationColorWheelVisibility,
		FExecuteAction::CreateSP(this, &SColorGradingColorWheelPanel::ToggleColorWheelVisible, 3),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SColorGradingColorWheelPanel::IsColorWheelVisible, 3)
	);

	CommandList->MapAction(
		Commands.ContrastColorWheelVisibility,
		FExecuteAction::CreateSP(this, &SColorGradingColorWheelPanel::ToggleColorWheelVisible, 4),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SColorGradingColorWheelPanel::IsColorWheelVisible, 4)
	);

	CommandList->MapAction(
		Commands.ColorWheelSliderOrientationHorizontal,
		FExecuteAction::CreateSP(this, &SColorGradingColorWheelPanel::SetColorWheelOrientation, EOrientation::Orient_Horizontal),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SColorGradingColorWheelPanel::IsColorWheelOrientationSelected, EOrientation::Orient_Horizontal)
	);

	CommandList->MapAction(
		Commands.ColorWheelSliderOrientationVertical,
		FExecuteAction::CreateSP(this, &SColorGradingColorWheelPanel::SetColorWheelOrientation, EOrientation::Orient_Vertical),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SColorGradingColorWheelPanel::IsColorWheelOrientationSelected, EOrientation::Orient_Vertical)
	);
}

TSharedRef<SWidget> SColorGradingColorWheelPanel::MakeColorDisplayModeCheckbox()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.0f, 0.0f, 3.0f, 0.0f))
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
			.Type(ESlateCheckBoxType::ToggleButton)
			.IsChecked(this, &SColorGradingColorWheelPanel::IsColorDisplayModeChecked, EColorGradingColorDisplayMode::RGB)
			.OnCheckStateChanged(this, &SColorGradingColorWheelPanel::OnColorDisplayModeCheckedChanged, EColorGradingColorDisplayMode::RGB)
			.ToolTipText(this, &SColorGradingColorWheelPanel::GetColorDisplayModeToolTip, EColorGradingColorDisplayMode::RGB)
			.Padding(4)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &SColorGradingColorWheelPanel::GetColorDisplayModeLabel, EColorGradingColorDisplayMode::RGB)
				.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)			
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.0f, 0.0f, 3.0f, 0.0f))
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
			.Type(ESlateCheckBoxType::ToggleButton)
			.IsChecked(this, &SColorGradingColorWheelPanel::IsColorDisplayModeChecked, EColorGradingColorDisplayMode::HSV)
			.OnCheckStateChanged(this, &SColorGradingColorWheelPanel::OnColorDisplayModeCheckedChanged, EColorGradingColorDisplayMode::HSV)
			.ToolTipText(this, &SColorGradingColorWheelPanel::GetColorDisplayModeToolTip, EColorGradingColorDisplayMode::HSV)
			.Padding(4)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &SColorGradingColorWheelPanel::GetColorDisplayModeLabel, EColorGradingColorDisplayMode::HSV)
				.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
			]
		];
}

TSharedRef<SWidget> SColorGradingColorWheelPanel::MakeSettingsMenu()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, CommandList);

	const FColorGradingCommands& Commands = FColorGradingCommands::Get();

	MenuBuilder.BeginSection(TEXT("ColorWheelVisibility"), LOCTEXT("ColorWheelPanel_ShowLabel", "Show"));
	{
		MenuBuilder.AddMenuEntry(Commands.SaturationColorWheelVisibility);
		MenuBuilder.AddMenuEntry(Commands.ContrastColorWheelVisibility);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(TEXT("ColorWheelSliders"), LOCTEXT("ColorWheelPanel_SlidersLabel", "Sliders"));
	{
		MenuBuilder.AddMenuEntry(Commands.ColorWheelSliderOrientationVertical);
		MenuBuilder.AddMenuEntry(Commands.ColorWheelSliderOrientationHorizontal);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SColorGradingColorWheelPanel::FillColorGradingGroupProperty(const FColorGradingEditorDataModel::FColorGradingGroup& ColorGradingGroup)
{
	if (ColorGradingGroupPropertyBox.IsValid())
	{
		TSharedRef<SHorizontalBox> PropertyNameBox = SNew(SHorizontalBox);

		if (ColorGradingGroup.EditConditionPropertyHandle.IsValid())
		{
			if (TSharedPtr<IDetailTreeNode> EditConditionTreeNode = ColorGradingDataModel->GetPropertyRowGenerator()->FindTreeNode(ColorGradingGroup.EditConditionPropertyHandle))
			{
				FNodeWidgets EditConditionWidgets = EditConditionTreeNode->CreateNodeWidgets();

				if (EditConditionWidgets.ValueWidget.IsValid())
				{
					PropertyNameBox->AddSlot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.Padding(2, 0, 4, 0)
						.AutoWidth()
						[
							EditConditionWidgets.ValueWidget.ToSharedRef()
						];
				}
			}
		}

		TSharedRef<SWidget> GroupHeaderWidget = ColorGradingGroup.GroupHeaderWidget.IsValid()
			? ColorGradingGroup.GroupHeaderWidget.ToSharedRef()
			: SNew(STextBlock).Text(ColorGradingGroup.DisplayName).Font(FAppStyle::Get().GetFontStyle("NormalFontBold"));

		PropertyNameBox->AddSlot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(2, 0, 0, 0)
			[
				GroupHeaderWidget
			];

		ColorGradingGroupPropertyBox->SetContent(PropertyNameBox);
	}
}

void SColorGradingColorWheelPanel::ClearColorGradingGroupProperty()
{
	ColorGradingGroupPropertyBox->SetContent(SNullWidget::NullWidget);
}

void SColorGradingColorWheelPanel::FillColorGradingElementsToolBar(const TArray<FColorGradingEditorDataModel::FColorGradingElement>& ColorGradingElements)
{
	ColorGradingElementsToolBarBox->ClearChildren();

	for (const FColorGradingEditorDataModel::FColorGradingElement& Element : ColorGradingElements)
	{
		ColorGradingElementsToolBarBox->AddSlot()
			.AutoWidth()
			.Padding(2.0f, 0.0f, 2.0f, 0.0f)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.OnCheckStateChanged(this, &SColorGradingColorWheelPanel::OnColorGradingElementCheckedChanged, Element.DisplayName)
				.IsChecked(this, &SColorGradingColorWheelPanel::IsColorGradingElementSelected, Element.DisplayName)
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "SmallText")
					.Text(Element.DisplayName)
				]
			];
	}
}

void SColorGradingColorWheelPanel::ClearColorGradingElementsToolBar()
{
	ColorGradingElementsToolBarBox->ClearChildren();
}

void SColorGradingColorWheelPanel::FillColorWheels(const FColorGradingEditorDataModel::FColorGradingElement& ColorGradingElement)
{
	auto FillColorWheel = [this](const TSharedPtr<SColorGradingColorWheel>& ColorWheel, const TSharedPtr<IPropertyHandle>& PropertyHandle)
	{
		if (ColorWheel.IsValid())
		{
			ColorWheel->SetColorPropertyHandle(PropertyHandle);
			ColorWheel->SetHeaderContent(CreateColorWheelHeaderWidget(PropertyHandle));
		}
	};

	FillColorWheel(ColorWheels[0], ColorGradingElement.OffsetPropertyHandle);
	FillColorWheel(ColorWheels[1], ColorGradingElement.GammaPropertyHandle);
	FillColorWheel(ColorWheels[2], ColorGradingElement.GainPropertyHandle);
	FillColorWheel(ColorWheels[3], ColorGradingElement.SaturationPropertyHandle);
	FillColorWheel(ColorWheels[4], ColorGradingElement.ContrastPropertyHandle);
}

void SColorGradingColorWheelPanel::ClearColorWheels()
{
	for (const TSharedPtr<SColorGradingColorWheel>& ColorWheel : ColorWheels)
	{
		if (ColorWheel.IsValid())
		{
			ColorWheel->SetColorPropertyHandle(nullptr);
			ColorWheel->SetHeaderContent(SNullWidget::NullWidget);
		}
	};
}

TSharedRef<SWidget> SColorGradingColorWheelPanel::CreateColorWheelHeaderWidget(const TSharedPtr<IPropertyHandle>& ColorPropertyHandle)
{
	if (TSharedPtr<IDetailTreeNode> TreeNode = ColorGradingDataModel->GetPropertyRowGenerator()->FindTreeNode(ColorPropertyHandle))
	{
		FNodeWidgets NodeWidgets = TreeNode->CreateNodeWidgets();

		TSharedRef<SHorizontalBox> PropertyNameBox = SNew(SHorizontalBox);

		PropertyNameBox->AddSlot()
			.FillWidth(1.0f)
			[
				SNew(SSpacer)
			];

		if (NodeWidgets.EditConditionWidget.IsValid())
		{
			PropertyNameBox->AddSlot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.Padding(2, 0, 0, 0)
				.AutoWidth()
				[
					NodeWidgets.EditConditionWidget.ToSharedRef()
				];
		}

		if (NodeWidgets.NameWidget.IsValid())
		{
			PropertyNameBox->AddSlot()
				.HAlign(NodeWidgets.NameWidgetLayoutData.HorizontalAlignment)
				.VAlign(NodeWidgets.NameWidgetLayoutData.VerticalAlignment)
				.Padding(2, 0, 0, 0)
				[
					NodeWidgets.NameWidget.ToSharedRef()
				];

			PropertyNameBox->AddSlot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.FillWidth(1.0f)
				[
					SNew(SBox)
					.WidthOverride(22.0f)
					[
						CreateColorPropertyExtensions(ColorPropertyHandle, TreeNode)
					]
				];
		}

		return PropertyNameBox;
	}

	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SColorGradingColorWheelPanel::CreateColorPropertyExtensions(const TSharedPtr<IPropertyHandle>& ColorPropertyHandle, const TSharedPtr<IDetailTreeNode>& DetailTreeNode)
{
	TArray<FPropertyRowExtensionButton> ExtensionButtons;

	// Use a weak pointer to pass into delegates
	TWeakPtr<IPropertyHandle> WeakColorPropertyHandle = ColorPropertyHandle;

	FPropertyRowExtensionButton& ResetToDefaultButton = ExtensionButtons.AddDefaulted_GetRef();
	ResetToDefaultButton.Label = NSLOCTEXT("PropertyEditor", "ResetToDefault", "Reset to Default");
	ResetToDefaultButton.UIAction = FUIAction(
		FExecuteAction::CreateLambda([WeakColorPropertyHandle]
		{
			if (WeakColorPropertyHandle.IsValid())
			{
				WeakColorPropertyHandle.Pin()->ResetToDefault();
			}
		}),
		FCanExecuteAction::CreateLambda([WeakColorPropertyHandle]
		{
			const bool bIsEditable = WeakColorPropertyHandle.Pin()->IsEditable();
			return bIsEditable;
		}),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateLambda([WeakColorPropertyHandle]
		{
			bool bShowResetToDefaultButton = false;
			if (WeakColorPropertyHandle.IsValid())
			{
				if (!WeakColorPropertyHandle.Pin()->HasMetaData("NoResetToDefault") && !WeakColorPropertyHandle.Pin()->GetInstanceMetaData("NoResetToDefault"))
				{
					bShowResetToDefaultButton = WeakColorPropertyHandle.Pin()->CanResetToDefault();
				}
			}

			return bShowResetToDefaultButton;
		})
	);

	ResetToDefaultButton.Icon = FSlateIcon(FAppStyle::Get().GetStyleSetName(), "PropertyWindow.DiffersFromDefault");
	ResetToDefaultButton.ToolTip = NSLOCTEXT("PropertyEditor", "ResetToDefaultPropertyValueToolTip", "Reset this property to its default value.");

	// Add any global row extensions that are registered for the color property
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FOnGenerateGlobalRowExtensionArgs Args;
	Args.OwnerTreeNode = DetailTreeNode;
	Args.PropertyHandle = ColorPropertyHandle;

	PropertyEditorModule.GetGlobalRowExtensionDelegate().Broadcast(Args, ExtensionButtons);

	FSlimHorizontalToolBarBuilder ToolbarBuilder(TSharedPtr<FUICommandList>(), FMultiBoxCustomization::None);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), "DetailsView.ExtensionToolBar");
	ToolbarBuilder.SetIsFocusable(false);

	for (const FPropertyRowExtensionButton& Extension : ExtensionButtons)
	{
		ToolbarBuilder.AddToolBarButton(Extension.UIAction, NAME_None, Extension.Label, Extension.ToolTip, Extension.Icon);
	}

	return ToolbarBuilder.MakeWidget();
}

bool SColorGradingColorWheelPanel::FilterDetailTreeNode(const TSharedRef<IDetailTreeNode>& InDetailTreeNode)
{
	if (ColorGradingDataModel)
	{
		if (FColorGradingEditorDataModel::FColorGradingGroup* ColorGradingGroup = ColorGradingDataModel->GetSelectedColorGradingGroup())
		{
			// Filter out any categories that are not configured by the data model to be displayed in the details section or subsection.
			// All other nodes (which will be any child of the category), should be displayed.
			if (InDetailTreeNode->GetNodeType() == EDetailNodeType::Category)
			{
				return ColorGradingGroup->DetailsViewCategories.Contains(InDetailTreeNode->GetNodeName());
			}
			else
			{
				return true;
			}
		}
	}

	return false;
}

void SColorGradingColorWheelPanel::SetColorWheelOrientation(EOrientation NewOrientation)
{
	if (ColorWheelOrientation != NewOrientation)
	{
		ColorWheelOrientation = NewOrientation;

		for (const TSharedPtr<SColorGradingColorWheel>& ColorWheel : ColorWheels)
		{
			if (ColorWheel.IsValid())
			{
				ColorWheel->SetOrientation(ColorWheelOrientation);
			}
		};
	}
}

bool SColorGradingColorWheelPanel::IsColorWheelOrientationSelected(EOrientation Orientation) const
{
	return ColorWheelOrientation == Orientation;
}

void SColorGradingColorWheelPanel::ToggleColorWheelVisible(int32 ColorWheelIndex)
{
	if (ColorWheelIndex >= 0 && ColorWheelIndex < HiddenColorWheels.Num())
	{
		HiddenColorWheels[ColorWheelIndex] = !HiddenColorWheels[ColorWheelIndex];
	}
}

bool SColorGradingColorWheelPanel::IsColorWheelVisible(int32 ColorWheelIndex)
{
	if (ColorWheelIndex >= 0 && ColorWheelIndex < HiddenColorWheels.Num())
	{
		return !HiddenColorWheels[ColorWheelIndex];
	}

	return false;
}

void SColorGradingColorWheelPanel::OnColorGradingGroupSelectionChanged()
{
	Refresh();
}

void SColorGradingColorWheelPanel::OnColorGradingElementSelectionChanged()
{
	if (const FColorGradingEditorDataModel::FColorGradingElement* ColorGradingElement = ColorGradingDataModel->GetSelectedColorGradingElement())
	{
		FillColorWheels(*ColorGradingElement);
	}
	else
	{
		ClearColorWheels();
	}
}

void SColorGradingColorWheelPanel::OnColorGradingElementCheckedChanged(ECheckBoxState State, FText ElementName)
{
	if (State == ECheckBoxState::Checked && ColorGradingDataModel)
	{
		if (FColorGradingEditorDataModel::FColorGradingGroup* ColorGradingGroup = ColorGradingDataModel->GetSelectedColorGradingGroup())
		{
			int32 ColorGradingElementIndex = ColorGradingGroup->ColorGradingElements.IndexOfByPredicate([=](const FColorGradingEditorDataModel::FColorGradingElement& Element)
			{
				return Element.DisplayName.CompareTo(ElementName) == 0;
			});

			ColorGradingDataModel->SetSelectedColorGradingElement(ColorGradingElementIndex);
		}
	}
}

ECheckBoxState SColorGradingColorWheelPanel::IsColorGradingElementSelected(FText ElementName) const
{
	if (ColorGradingDataModel)
	{
		if (const FColorGradingEditorDataModel::FColorGradingElement* ColorGradingElement = ColorGradingDataModel->GetSelectedColorGradingElement())
		{
			if (ColorGradingElement->DisplayName.CompareTo(ElementName) == 0)
			{
				return ECheckBoxState::Checked;
			}
		}
	}

	return ECheckBoxState::Unchecked;
}

EVisibility SColorGradingColorWheelPanel::GetColorWheelPanelVisibility() const
{
	bool bHasObject = ColorGradingDataModel && ColorGradingDataModel->GetPropertyRowGenerator()->GetSelectedObjects().Num() == 1;
	return bHasObject ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SColorGradingColorWheelPanel::GetMultiSelectWarningVisibility() const
{
	bool bHasMultipleObjects = ColorGradingDataModel && ColorGradingDataModel->GetPropertyRowGenerator()->GetSelectedObjects().Num() > 1;
	return bHasMultipleObjects ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SColorGradingColorWheelPanel::GetColorWheelVisibility(int32 ColorWheelIndex) const
{
	bool bIsHidden = HiddenColorWheels[ColorWheelIndex];
	return bIsHidden ? EVisibility::Collapsed : EVisibility::Visible;
}

ECheckBoxState SColorGradingColorWheelPanel::IsColorDisplayModeChecked(EColorGradingColorDisplayMode InColorDisplayMode) const
{
	bool bIsModeSelected = InColorDisplayMode == ColorDisplayMode;
	return bIsModeSelected ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SColorGradingColorWheelPanel::OnColorDisplayModeCheckedChanged(ECheckBoxState State, EColorGradingColorDisplayMode InColorDisplayMode)
{
	if (State == ECheckBoxState::Checked)
	{
		ColorDisplayMode = InColorDisplayMode;
	}
}

FText SColorGradingColorWheelPanel::GetColorDisplayModeLabel(EColorGradingColorDisplayMode InColorDisplayMode) const
{
	FText Text;

	switch (InColorDisplayMode)
	{
	case EColorGradingColorDisplayMode::RGB: Text = LOCTEXT("ColorWheel_RGBColorDisplayModeLabel", "RGB"); break;
	case EColorGradingColorDisplayMode::HSV: Text = LOCTEXT("ColorWheel_HSVColorDisplayModeLabel", "HSV"); break;
	}

	return Text;
}

FText SColorGradingColorWheelPanel::GetColorDisplayModeToolTip(EColorGradingColorDisplayMode InColorDisplayMode) const
{
	FText Text;

	switch (InColorDisplayMode)
	{
	case EColorGradingColorDisplayMode::RGB: Text = LOCTEXT("ColorWheel_RGBColorDisplayModeToolTip", "Change to RGB color mode"); break;
	case EColorGradingColorDisplayMode::HSV: Text = LOCTEXT("ColorWheel_HSVColorDisplayModeToolTip", "Change to HSV color mode"); break;
	}

	return Text;
}

#undef LOCTEXT_NAMESPACE