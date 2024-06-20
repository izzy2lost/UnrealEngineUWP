// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkinWeightDetailCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "ModelingToolsEditorModeStyle.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "SkeletalMesh/SkinWeightsPaintTool.h"
#include "Selection/PolygonSelectionMechanic.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "SkinWeightToolSettingsEditor"

// layout constants
float FSkinWeightDetailCustomization::WeightSliderWidths = 150.0f;
float FSkinWeightDetailCustomization::WeightEditingLabelsPercent = 0.40f;
float FSkinWeightDetailCustomization::WeightEditVerticalPadding = 4.0f;
float FSkinWeightDetailCustomization::WeightEditHorizontalPadding = 2.0f;

// add a buffer to the weight sliders to prevent ever fully getting a value to 1 or 0 using the slider alone
// doing so will remove other influences and cause the slider to no longer function as all other influences
// will be culled by normalization thus making the slider "stuck" at full value.
constexpr float SliderBuffer = UE::AnimationCore::BoneWeightThreshold * 2.f;

FSkinWeightDetailCustomization::~FSkinWeightDetailCustomization()
{
	if (Tool.IsValid())
	{
		Tool->OnSelectionChanged.RemoveAll(this);
	}

	Tool.Reset();
	ToolSettings.Reset();
}

void FSkinWeightDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	CurrentDetailBuilder = &DetailBuilder;
	
	TArray<TWeakObjectPtr<UObject>> DetailObjects;
	DetailBuilder.GetObjectsBeingCustomized(DetailObjects);

	// should be impossible to get multiple settings objects for a single tool
	ensure(DetailObjects.Num()==1);
	ToolSettings = Cast<USkinWeightsPaintToolProperties>(DetailObjects[0]);
	Tool = ToolSettings->WeightTool;
	Tool->OnSelectionChanged.AddSP(this, &FSkinWeightDetailCustomization::OnSelectionChanged);

	// Edit SkinWeightLayer category first 
	IDetailCategoryBuilder& SkinWeightLayerCategory = DetailBuilder.EditCategory("SkinWeightLayer", FText::GetEmpty(), ECategoryPriority::Important);
	SkinWeightLayerCategory.InitiallyCollapsed(true);
	
	// custom display of falloff mode as segmented toggle buttons
	IDetailCategoryBuilder& EditModeCategory = DetailBuilder.EditCategory("Weight Editing Mode", FText::GetEmpty(), ECategoryPriority::Important);

	// add segmented control toggle for editing modes ("Brush" or "Selection")
	EditModeCategory.AddCustomRow(LOCTEXT("EditModeCategory", "Weight Editing Mode"), false)
	.WholeRowContent()
	[
		SNew(SBox)
		.Padding(2.0f)
		[
			SNew(SSegmentedControl<EWeightEditMode>)
			.ToolTipText(LOCTEXT("EditingModeTooltip",
					"Brush: edit weights by painting on mesh.\n"
					"Mesh: select vertices/edges/faces to edit weights directly.\n"
					"Bones: select and manipulate bones to preview deformations.\n"))
			.Value_Lambda([this]()
			{
				return ToolSettings->EditingMode;
			})
			.OnValueChanged_Lambda([this](EWeightEditMode Mode)
			{
				ToolSettings->EditingMode = Mode;
				ToolSettings->WeightTool->ToggleEditingMode();
				if (CurrentDetailBuilder)
				{
					CurrentDetailBuilder->ForceRefreshDetails();
				}
			})
			+SSegmentedControl<EWeightEditMode>::Slot(EWeightEditMode::Brush)
			.Text(LOCTEXT("BrushEditMode", "Brush"))
			+ SSegmentedControl<EWeightEditMode>::Slot(EWeightEditMode::Mesh)
			.Text(LOCTEXT("MeshEditMode", "Mesh"))
			+ SSegmentedControl<EWeightEditMode>::Slot(EWeightEditMode::Bones)
			.Text(LOCTEXT("BoneEditMode", "Bones"))
		]
	];

	// BRUSH editing mode UI
	if (ToolSettings->EditingMode == EWeightEditMode::Brush)
	{
		AddBrushUI(DetailBuilder);
	}

	// MESH editing mode UI
	if (ToolSettings->EditingMode == EWeightEditMode::Mesh)
	{
		AddSelectionUI(DetailBuilder);
	}
	
	// COLOR MODE category
	IDetailCategoryBuilder& MeshDisplayCategory = DetailBuilder.EditCategory("MeshDisplay", FText::GetEmpty(), ECategoryPriority::Important);
	MeshDisplayCategory.InitiallyCollapsed(false);
	MeshDisplayCategory.AddCustomRow(LOCTEXT("ColorModeCategory", "Color Mode"), false)
	.WholeRowContent()
	[
		SNew(SBox)
		.Padding(2.0f)
		.HAlign(HAlign_Center)
		[
			SNew(SSegmentedControl<EWeightColorMode>)
			.ToolTipText(LOCTEXT("WeightColorTooltip",
					"Adjust the weight display in the viewport.\n\n"
					"Greyscale: Displays weights on the current bone by blending from black (0) to white (1).\n"
					"Ramp: Displays weights on the current bone. Weights at 0 and 1 use the min and max colors. Weights inbetween 0 and 1 use the ramp colors.\n"
					"Multi Color: Displays weights on ALL bones using the color of the bones.\n"
					"Full Material: Displays normal mesh materials with textures.\n"))
			.Value_Lambda([this]()
			{
				return ToolSettings->ColorMode;
			})
			.OnValueChanged_Lambda([this](EWeightColorMode Mode)
			{
				ToolSettings->SetColorMode(Mode);
			})
			+ SSegmentedControl<EWeightColorMode>::Slot(EWeightColorMode::Greyscale)
			.Text(LOCTEXT("GreyscaleMode", "Greyscale"))
			+SSegmentedControl<EWeightColorMode>::Slot(EWeightColorMode::Ramp)
			.Text(LOCTEXT("RampMode", "Ramp"))
			+SSegmentedControl<EWeightColorMode>::Slot(EWeightColorMode::BoneColors)
			.Text(LOCTEXT("BoneColorsMode", "Bone Colors"))
			+SSegmentedControl<EWeightColorMode>::Slot(EWeightColorMode::FullMaterial)
			.Text(LOCTEXT("MaterialMode", "Full Material"))
		]
	];

	AddTransferUI(DetailBuilder);

	// hide all base brush properties that have been customized
	const TSharedRef<IPropertyHandle> BrushModeHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USkinWeightsPaintToolProperties, BrushMode));
	DetailBuilder.HideProperty(BrushModeHandle);
	const TSharedRef<IPropertyHandle> BrushSizeHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBrushBaseProperties, BrushSize), UBrushBaseProperties::StaticClass());
	DetailBuilder.HideProperty(BrushSizeHandle);
	const TSharedRef<IPropertyHandle> BrushStrengthHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBrushBaseProperties, BrushStrength), UBrushBaseProperties::StaticClass());
	DetailBuilder.HideProperty(BrushStrengthHandle);
	const TSharedRef<IPropertyHandle> BrushFalloffHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBrushBaseProperties, BrushFalloffAmount), UBrushBaseProperties::StaticClass());
	DetailBuilder.HideProperty(BrushFalloffHandle);
	const TSharedRef<IPropertyHandle> BrushRadiusHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBrushBaseProperties, BrushRadius), UBrushBaseProperties::StaticClass());
	DetailBuilder.HideProperty(BrushRadiusHandle);
	const TSharedRef<IPropertyHandle> SpecifyRadiusHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBrushBaseProperties, bSpecifyRadius), UBrushBaseProperties::StaticClass());
	DetailBuilder.HideProperty(SpecifyRadiusHandle);
	const TSharedRef<IPropertyHandle> EditModePropHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USkinWeightsPaintToolProperties, EditingMode));
	DetailBuilder.HideProperty(EditModePropHandle);
	const TSharedRef<IPropertyHandle> ColorModePropHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USkinWeightsPaintToolProperties, ColorMode));
	DetailBuilder.HideProperty(ColorModePropHandle);
}

void FSkinWeightDetailCustomization::AddBrushUI(IDetailLayoutBuilder& DetailBuilder) const
{
	// custom display of falloff mode as segmented toggle buttons
	IDetailCategoryBuilder& BrushCategory = DetailBuilder.EditCategory("Brush", FText::GetEmpty(), ECategoryPriority::Important);

	// add segmented control toggle for brush behavior modes ("Add", "Replace", etc..)
	BrushCategory.AddCustomRow(LOCTEXT("BrushModeCategory", "Brush Mode"), false)
	.WholeRowContent()
	[
		SNew(SBox)
		.Padding(2.0f)
		[
			SNew(SSegmentedControl<EWeightEditOperation>)
			.ToolTipText(LOCTEXT("FloodTooltip",
				"Add: applies the current weight plus the flood value to the new weight.\n"
				"Replace: applies the current weight minus the strength value to the new weight.\n"
				"Multiply: applies the current weight multiplied by the strength value to the new weight.\n"
				"Relax: applies the average of the connected (by edge) vertex weights to the new vertex weight, blended by the strength.\n"
				"This command operates on the selected bone(s) and selected vertices.\n"
				"If no bones are selected, ALL bones are considered.\n"
				"If no vertices are selected, ALL vertices are considered."))
			.Value_Lambda([this]()
			{
				return ToolSettings->BrushMode;
			})
			.OnValueChanged_Lambda([this](EWeightEditOperation Mode)
			{
				ToolSettings->SetBrushMode(Mode);
			})
			+SSegmentedControl<EWeightEditOperation>::Slot(EWeightEditOperation::Add)
			.Text(LOCTEXT("BrushAddMode", "Add"))
			+ SSegmentedControl<EWeightEditOperation>::Slot(EWeightEditOperation::Replace)
			.Text(LOCTEXT("BrushReplaceMode", "Replace"))
			+ SSegmentedControl<EWeightEditOperation>::Slot(EWeightEditOperation::Multiply)
			.Text(LOCTEXT("BrushMultiplyMode", "Multiply"))
			+ SSegmentedControl<EWeightEditOperation>::Slot(EWeightEditOperation::Relax)
			.Text(LOCTEXT("BrushRelaxMode", "Relax"))
		]
	];

	// add segmented control toggle for brush falloff modes ("Surface" or "Volume")
	BrushCategory.AddCustomRow(LOCTEXT("BrushFalloffModeCategory", "Brush Falloff Mode"), false)
	.WholeRowContent()
	[
		SNew(SBox)
		.Padding(2.0f)
		[
			SNew(SSegmentedControl<EWeightBrushFalloffMode>)
			.ToolTipText(LOCTEXT("BrushFalloffModeTooltip",
					"Surface: falloff is based on the distance along the surface from the brush center to nearby connected vertices.\n"
					"Volume: falloff is based on the straight-line distance from the brush center to surrounding vertices.\n"))
			.Value_Lambda([this]()
			{
				return ToolSettings->GetBrushConfig().FalloffMode;
			})
			.OnValueChanged_Lambda([this](EWeightBrushFalloffMode Mode)
			{
				ToolSettings->SetFalloffMode(Mode);
			})
			+SSegmentedControl<EWeightBrushFalloffMode>::Slot(EWeightBrushFalloffMode::Surface)
			.Text(LOCTEXT("SurfaceMode", "Surface"))
			+ SSegmentedControl<EWeightBrushFalloffMode>::Slot(EWeightBrushFalloffMode::Volume)
			.Text(LOCTEXT("VolumeMode", "Volume"))
		]
	];

	BrushCategory.AddCustomRow(LOCTEXT("BrushSizeCategory", "Brush Radius"), false)
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BrushRadiusLabel", "Radius"))
		.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		.ToolTipText(LOCTEXT("BrushRadiusTooltip", "The radius of the brush in scene units."))
	]
	.ValueContent()
	[
		SNew(SSpinBox<float>)
		.MinValue(0.01f)
		.MaxSliderValue(20.f)
		.Value(10.0f)
		.SupportDynamicSliderMaxValue(true)
		.Value_Lambda([this]()
		{
			return ToolSettings->GetBrushConfig().Radius;
		})
		.OnValueChanged_Lambda([this](float NewValue)
		{
			ToolSettings->BrushRadius = NewValue;
			ToolSettings->GetBrushConfig().Radius = NewValue;
			FPropertyChangedEvent PropertyChangedEvent(UBrushBaseProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UBrushBaseProperties, BrushRadius)));
			ToolSettings->PostEditChangeProperty(PropertyChangedEvent);
		})
		.OnValueCommitted_Lambda([this](float NewValue, ETextCommit::Type CommitType)
		{
			ToolSettings->SaveConfig();
		})
	];

	BrushCategory.AddCustomRow(LOCTEXT("BrushStrengthCategory", "Brush Strength"), false)
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BrushStrengthLabel", "Strength"))
		.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		.ToolTipText(LOCTEXT("BrushStrengthTooltip", "The strength of the effect on the weights. Exact effect depends on the Brush mode."))
	]
	.ValueContent()
	[
		SNew(SSpinBox<float>)
		.MinValue(0.f)
		.MaxValue(2.0f)
		.MaxSliderValue(1.f)
		.Value(1.0f)
		.SupportDynamicSliderMaxValue(true)
		.Value_Lambda([this]()
		{
			return ToolSettings->GetBrushConfig().Strength;
		})
		.OnValueChanged_Lambda([this](float NewValue)
		{
			ToolSettings->BrushStrength = NewValue;
			ToolSettings->GetBrushConfig().Strength = NewValue;
			FPropertyChangedEvent PropertyChangedEvent(UBrushBaseProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UBrushBaseProperties, BrushStrength)));
			ToolSettings->PostEditChangeProperty(PropertyChangedEvent);
		})
		.OnValueCommitted_Lambda([this](float NewValue, ETextCommit::Type CommitType)
		{
			ToolSettings->SaveConfig();
		})
	];

	BrushCategory.AddCustomRow(LOCTEXT("BrushFalloffCategory", "Brush Falloff"), false)
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BrushFalloffLabel", "Falloff"))
		.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		.ToolTipText(LOCTEXT("BrushFalloffTooltip", "At 0, the brush has no falloff. At 1 it has exponential falloff."))
	]
	.ValueContent()
	[
		SNew(SSpinBox<float>)
		.MinValue(0.f)
		.MaxValue(1.f)
		.Value_Lambda([this]()
		{
			return ToolSettings->GetBrushConfig().Falloff;
		})
		.OnValueChanged_Lambda([this](float NewValue)
		{
			ToolSettings->BrushFalloffAmount = NewValue;
			ToolSettings->GetBrushConfig().Falloff = NewValue;
			FPropertyChangedEvent PropertyChangedEvent(UBrushBaseProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UBrushBaseProperties, BrushFalloffAmount)));
			ToolSettings->PostEditChangeProperty(PropertyChangedEvent);
		})
		.OnValueCommitted_Lambda([this](float NewValue, ETextCommit::Type CommitType)
		{
			ToolSettings->SaveConfig();
		})
	];
}

void FSkinWeightDetailCustomization::AddSelectionUI(IDetailLayoutBuilder& DetailBuilder) const
{
	// custom display of weight editing tools
	IDetailCategoryBuilder& EditSelectionCategory = DetailBuilder.EditCategory("Edit Selection", FText::GetEmpty(), ECategoryPriority::Important);
	EditSelectionCategory.InitiallyCollapsed(true);

	// create a toolbar for the selection filter
	FSlimHorizontalToolBarBuilder ToolbarBuilder(MakeShared<FUICommandList>(), FMultiBoxCustomization::None);
	ToolbarBuilder.SetStyle(FModelingToolsEditorModeStyle::Get().Get(), "PolyEd.SelectionToolbar");
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

	ToolbarBuilder.BeginSection("SelectionFilter");
	ToolbarBuilder.BeginBlockGroup();
	
	auto AddToggleButtonForBool = [&ToolbarBuilder, this](EComponentSelectionMode Mode, const FText& Label, const FText& Tooltip, const FName IconName)
	{
		ToolbarBuilder.AddToolBarButton(FUIAction(
		FExecuteAction::CreateLambda([this, Mode]()
		{
			ToolSettings->SetComponentMode(Mode);
		}),
		FCanExecuteAction::CreateLambda([this]()
		{
			return ToolSettings->EditingMode == EWeightEditMode::Mesh;
		}),
		FIsActionChecked::CreateLambda([this, Mode]()
		{
			return ToolSettings->ComponentSelectionMode == Mode;
		})),
		NAME_None,	// Extension hook
		Label,		// Label
		Tooltip,	// Tooltip
		FSlateIcon(FModelingToolsEditorModeStyle::Get()->GetStyleSetName(), IconName),
		EUserInterfaceActionType::ToggleButton);
	};

	AddToggleButtonForBool(
		EComponentSelectionMode::Vertices,
		LOCTEXT("VerticesLabel", "Vertices"),
		LOCTEXT("VerticesTooltip", "Select mesh vertices."),
		"PolyEd.SelectCorners");
	AddToggleButtonForBool(
		EComponentSelectionMode::Edges,
		LOCTEXT("EdgesLabel", "Edges"),
		LOCTEXT("EdgesTooltip", "Select mesh edges."),
		"PolyEd.SelectEdges");
	AddToggleButtonForBool(
		EComponentSelectionMode::Faces,
		LOCTEXT("FacesLabel", "Faces"),
		LOCTEXT("FacesTooltip", "Select mesh faces."),
		"PolyEd.SelectFaces");

	ToolbarBuilder.EndBlockGroup();
	ToolbarBuilder.EndSection();

	// GROW/SHRINK/FLOOD Selection category
	EditSelectionCategory.AddCustomRow(LOCTEXT("EditSelectionRow", "Edit Selection"), false)
	.WholeRowContent()
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(2.0f, WeightEditVerticalPadding)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				[
					ToolbarBuilder.MakeWidget()
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
					.HAlign(HAlign_Center)
					.ToolTipText(LOCTEXT("IsolateSelectedTooltip",
							"Shows only the selected faces in the viewport.\n"
							"Weight editing operations will not affect hidden vertices.\n "))
					.IsEnabled_Lambda([this]()
					{
						const bool bHasSelection = Tool->IsAnyComponentSelected();
						const bool bAlreadyIsolatingSelection = Tool->IsSelectionIsolated();
						return bHasSelection ||  bAlreadyIsolatingSelection;
					})
					.IsChecked_Lambda([this]()
					{
						const bool bAlreadyIsolatingSelection = Tool->IsSelectionIsolated();
						return bAlreadyIsolatingSelection ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([this](ECheckBoxState InCheckBoxState)
					{
						if (InCheckBoxState == ECheckBoxState::Checked)
						{
							Tool->SetIsolateSelected(true);	
						}
						else
						{
							Tool->SetIsolateSelected(false);	
						}
					})
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							if (Tool->IsSelectionIsolated())
							{
								return LOCTEXT("ShowAllButtonLabel", "Show All");
							}
								
							return LOCTEXT("IsolateButtonLabel", "Isolate Selected");
						})
					]
				]
			]
		]

		+SVerticalBox::Slot()
		[
			SNew(SHorizontalBox)
		
			+SHorizontalBox::Slot()
			.Padding(2.f, WeightEditVerticalPadding)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(LOCTEXT("GrowSelectionButtonLabel", "Grow"))
				.ToolTipText(LOCTEXT("GrowSelectionTooltip",
						"Grow the current selection by adding connected neighbors to current selection.\n"))
				.OnClicked_Lambda([this]()
				{
					Tool->GrowSelection();
					return FReply::Handled();
				})
			]

			+SHorizontalBox::Slot()
			.Padding(2.f, WeightEditVerticalPadding)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(LOCTEXT("ShrinkSelectionButtonLabel", "Shrink"))
				.ToolTipText(LOCTEXT("ShrinkSelectionTooltip",
						"Shrink the current selection by removing components on the border of the current selection.\n"))
				.OnClicked_Lambda([this]()
				{
					Tool->ShrinkSelection();
					return FReply::Handled();
				})
			]

			+SHorizontalBox::Slot()
			.Padding(2.f, WeightEditVerticalPadding)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Text(LOCTEXT("FloodSelectionButtonLabel", "Flood"))
				.ToolTipText(LOCTEXT("FloodSelectionTooltip",
						"Flood the current selection by adding all connected components to the current selection.\n"))
				.OnClicked_Lambda([this]()
				{
					Tool->FloodSelection();
					return FReply::Handled();
				})
			]
		]
	];

	// custom display of weight editing tools
	IDetailCategoryBuilder& EditWeightsCategory = DetailBuilder.EditCategory("Edit Weights", FText::GetEmpty(), ECategoryPriority::Important);
	EditWeightsCategory.InitiallyCollapsed(true);

	// FLOOD WEIGHTS SLIDER category
	ToolSettings->DirectEditState.Reset();
	EditWeightsCategory.AddCustomRow(LOCTEXT("FloodWeightsRow", "Flood Weights Slider"), false)
	.WholeRowContent()
	[
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		[
			SNew(SSegmentedControl<EWeightEditOperation>)
			.ToolTipText(LOCTEXT("EditModeTooltip",
				"Add: applies the current weight plus the flood value to the new weight.\n"
				"Multiply: applies the current weight multiplied by the flood value to the new weight.\n"))
			.Value_Lambda([this]()
			{
				return ToolSettings->DirectEditState.EditMode;
			})
			.OnValueChanged_Lambda([this](EWeightEditOperation Mode)
			{
				ToolSettings->DirectEditState.EditMode = Mode;
				ToolSettings->DirectEditState.Reset();
			})
			+SSegmentedControl<EWeightEditOperation>::Slot(EWeightEditOperation::Add)
			.Text(LOCTEXT("BrushAddMode", "Add"))
			+ SSegmentedControl<EWeightEditOperation>::Slot(EWeightEditOperation::Multiply)
			.Text(LOCTEXT("BrushMultiplyMode", "Multiply"))
		]
		
		+SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSpinBox<float>)
				.Visibility_Lambda([this]()
				{
					const bool bIsVisible = ToolSettings->DirectEditState.EditMode != EWeightEditOperation::Relax;
					return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.MinSliderValue_Lambda([this]()
				{
					return ToolSettings->DirectEditState.GetModeMinValue() + SliderBuffer;
				})
				.MaxSliderValue_Lambda([this]()
				{
					return ToolSettings->DirectEditState.GetModeMaxValue() - SliderBuffer;
				})
				.MinValue_Lambda([this]()
				{
					return ToolSettings->DirectEditState.GetModeMinValue();
				})
				.MaxValue_Lambda([this]()
				{
					return ToolSettings->DirectEditState.GetModeMaxValue();
				})
				.Value_Lambda([this]()
				{
					return ToolSettings->DirectEditState.CurrentValue;
				})
				.OnValueChanged_Lambda([this](float NewValue)
				{
					ToolSettings->DirectEditState.CurrentValue = NewValue;
					
					if (ToolSettings->DirectEditState.bInTransaction)
					{
						
						float Value = NewValue;
						if (ToolSettings->DirectEditState.EditMode == EWeightEditOperation::Add)
						{
							Value = NewValue - ToolSettings->DirectEditState.StartValue;
						}
						
						constexpr bool bShouldTransact = false;
						
						Tool->EditWeightsOnVertices(
							Tool->GetCurrentBoneIndex(),
							Value,
							ToolSettings->DirectEditState.EditMode,
							Tool->GetVerticesToEdit(),
							bShouldTransact);
					}
				})
				.OnValueCommitted_Lambda([this](float NewValue, ETextCommit::Type CommitType)
				{
					if (!ToolSettings->DirectEditState.bInTransaction)
					{
						constexpr bool bShouldTransact = true;
						Tool->EditWeightsOnVertices(
							Tool->GetCurrentBoneIndex(),
							NewValue, 
							ToolSettings->DirectEditState.EditMode,
							Tool->GetVerticesToEdit(),
							bShouldTransact);
					}
					ToolSettings->DirectEditState.bInTransaction = false;
				})
				.OnBeginSliderMovement_Lambda([this]()
				{
					ToolSettings->DirectEditState.StartValue = ToolSettings->DirectEditState.CurrentValue;
					ToolSettings->DirectEditState.bInTransaction = true;
					Tool->BeginChange();
				})
				.OnEndSliderMovement_Lambda([this](float)
				{
					const FText TransactionLabel = LOCTEXT("FloodWeightChange", "Flood weights on vertices.");
					Tool->EndChange(TransactionLabel);
					ToolSettings->DirectEditState.bInTransaction = false;

					// reset slider after each edit because multiplying operation is always relative to 1.0
					if (ToolSettings->DirectEditState.EditMode == EWeightEditOperation::Multiply)
					{
						ToolSettings->DirectEditState.CurrentValue = 1.0f;
						ToolSettings->DirectEditState.StartValue = 1.0f;
					}
				})
				.ToolTipText(LOCTEXT("FloodWeightsToolTip", "Drag the slider to interactively adjust weights on the selected vertices."))
			]	
		]
	];

	// REPLACE WEIGHTS category
	EditWeightsCategory.AddCustomRow(LOCTEXT("ReplaceWeightsRow", "Replace"), false)
	.WholeRowContent()
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.Padding(WeightEditHorizontalPadding, WeightEditVerticalPadding)
		[
			SNew(SHorizontalBox)
					
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(WeightEditingLabelsPercent)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ReplaceLabel", "Replace"))
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.ToolTipText(LOCTEXT("ReplaceTooltip", "Replace the current weight."))
			]

			+SHorizontalBox::Slot()
			[
				SNew(SSpinBox<float>)
				.MinValue(0.f)
				.MaxValue(1.f)
				.Value_Lambda([this]()
				{
					return ToolSettings->ReplaceValue;
				})
				.OnValueChanged_Lambda([this](float NewValue)
				{
					ToolSettings->ReplaceValue = NewValue;
				})
				.OnValueCommitted_Lambda([this](float NewValue, ETextCommit::Type CommitType)
				{
					ToolSettings->SaveConfig();
				})
			]

			+SHorizontalBox::Slot()
			[
				SNew(SBox)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("ReplaceWeightsButtonLabel", "Replace"))
					.ToolTipText(LOCTEXT("ReplaceButtonTooltip",
						"Replace: the weight of selected vertices is replaced by the specified value.\n"))
					.OnClicked_Lambda([this]()
					{
						constexpr bool bShouldTransact = true;
						Tool->EditWeightsOnVertices(
							Tool->GetCurrentBoneIndex(),
							ToolSettings->ReplaceValue, 
							EWeightEditOperation::Replace,
							Tool->GetVerticesToEdit(),
							bShouldTransact);
						return FReply::Handled();
					})
				]
			]
		]
	];

	// RELAX WEIGHTS category
	EditWeightsCategory.AddCustomRow(LOCTEXT("RelaxWeightsRow", "Relax"), false)
	.WholeRowContent()
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.Padding(WeightEditHorizontalPadding, WeightEditVerticalPadding)
		[
			SNew(SHorizontalBox)
					
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(WeightEditingLabelsPercent)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("RelaxIterationsLabel", "Iterations"))
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.ToolTipText(LOCTEXT("RelaxIterationsTooltip", "The number of iterations of relaxation to apply."))
			]

			+SHorizontalBox::Slot()
			[
				SNew(SSpinBox<int32>)
				.MinValue(1)
				.MaxValue(10)
				.Value_Lambda([this]()
				{
					return ToolSettings->RelaxIterations;
				})
				.OnValueChanged_Lambda([this](int32 NewValue)
				{
					ToolSettings->RelaxIterations = NewValue;
				})
				.OnValueCommitted_Lambda([this](int32 NewValue, ETextCommit::Type CommitType)
				{
					ToolSettings->SaveConfig();
				})
			]

			+SHorizontalBox::Slot()
			[
				SNew(SBox)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("RelaxWeightsButtonLabel", "Relax"))
					.ToolTipText(LOCTEXT("RelaxButtonTooltip",
						"Relax: the weight of each vertex is replaced by the average of it's neighbors. This smooths weights across the mesh.\n"))
					.OnClicked_Lambda([this]()
					{
						constexpr bool bShouldTransact = true;
						Tool->EditWeightsOnVertices(
							Tool->GetCurrentBoneIndex(),
							ToolSettings->RelaxIterations, 
							EWeightEditOperation::Relax,
							Tool->GetVerticesToEdit(),
							bShouldTransact);
						return FReply::Handled();
					})
				]
			]
		]
	];
	
	// PRUNE WEIGHTS category
	EditWeightsCategory.AddCustomRow(LOCTEXT("PruneWeightsRow", "Prune"), false)
	.WholeRowContent()
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.Padding(WeightEditHorizontalPadding, WeightEditVerticalPadding)
		[
			SNew(SHorizontalBox)
					
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(WeightEditingLabelsPercent)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PruneThresholdLabel", "Prune"))
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.ToolTipText(LOCTEXT("PruneThresholdTooltip", "The threshold weight value to use with Prune operation."))
			]

			+SHorizontalBox::Slot()
			[
				SNew(SSpinBox<float>)
				.MinValue(0.f)
				.MaxValue(1.f)
				.Value_Lambda([this]()
				{
					return ToolSettings->PruneValue;
				})
				.OnValueChanged_Lambda([this](float NewValue)
				{
					ToolSettings->PruneValue = NewValue;
				})
				.OnValueCommitted_Lambda([this](float NewValue, ETextCommit::Type CommitType)
				{
					ToolSettings->SaveConfig();
				})
			]

			+SHorizontalBox::Slot()
			[
				SNew(SBox)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("PruneWeightsButtonLabel", "Prune"))
					.ToolTipText(LOCTEXT("PruneButtonTooltip",
						"Weights below the given threshold value are removed.\n"
						"This command operates on the selected bone(s) and selected vertices.\n "
						"If no bones are selected, ALL bone weights are considered.\n "
						"If no vertices are selected, ALL vertices are considered."))
					.OnClicked_Lambda([this]()
					{
						ToolSettings->WeightTool->PruneWeights(ToolSettings->PruneValue);
						return FReply::Handled();
					})
				]
			]
		]
	];

	// AVERAGE/NORMALIZE WEIGHTS category
	EditWeightsCategory.AddCustomRow(LOCTEXT("NormalizeWeightsRow", "Normalize"), false)
	.WholeRowContent()
	[
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.Padding(2.f, WeightEditVerticalPadding)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Text(LOCTEXT("AverageWeightsButtonLabel", "Average"))
			.ToolTipText(LOCTEXT("AverageWeightsTooltip",
					"Takes the average of vertex weights and applies it to the selected vertices.\n"
					"This command operates on the selected bone(s) and selected vertices.\n "
					"If no bones are selected, ALL bone weights are considered.\n "
					"If no vertices are selected, ALL vertices are considered."))
			.OnClicked_Lambda([this]()
			{
				Tool->AverageWeights();
				return FReply::Handled();
			})
		]

		+SHorizontalBox::Slot()
		.Padding(2.f, WeightEditVerticalPadding)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Text(LOCTEXT("NormalizeWeightsButtonLabel", "Normalize"))
			.ToolTipText(LOCTEXT("NormalizeWeightsTooltip",
					"Forces the weights on the selected vertices to sum to 1.\n"
					"This command operates on the selected vertices.\n "
					"If no vertices are selected, ALL vertices are considered."))
			.IsEnabled_Lambda([this]()
			{
				return ToolSettings->EditingMode == EWeightEditMode::Mesh;
			})
			.OnClicked_Lambda([this]()
			{
				Tool->NormalizeWeights();
				return FReply::Handled();
			})
		]
	];

	// MIRROR WEIGHTS category
	EditWeightsCategory.AddCustomRow(LOCTEXT("MirrorWeightsRow", "Mirror"), false)
	.WholeRowContent()
	[
		SNew(SVerticalBox)
		
		+ SVerticalBox::Slot()
		.Padding(WeightEditHorizontalPadding, WeightEditVerticalPadding)
		[
			SNew(SHorizontalBox)
			
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(WeightEditingLabelsPercent)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MirrorPlaneLabel", "Mirror Plane"))
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.ToolTipText(LOCTEXT("MirrorPlaneTooltip", "The plane to copy weights across."))
			]

			+SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				[
					SNew(SSegmentedControl<EAxis::Type>)
					.ToolTipText(LOCTEXT("MirrorAxisTooltip",
						"X: copies weights across the YZ plane.\n"
						"Y: copies weights across the XZ plane.\n"
						"Z: copies weights across the XY plane."))
					.Value_Lambda([this]()
					{
						return ToolSettings->MirrorAxis;
					})
					.OnValueChanged_Lambda([this](EAxis::Type Mode)
					{
						ToolSettings->MirrorAxis = Mode;
					})
					+ SSegmentedControl<EAxis::Type>::Slot(EAxis::X)
					.Text(LOCTEXT("MirrorXLabel", "X"))
					+ SSegmentedControl<EAxis::Type>::Slot(EAxis::Y)
					.Text(LOCTEXT("MirrorYLabel", "Y"))
					+ SSegmentedControl<EAxis::Type>::Slot(EAxis::Z)
					.Text(LOCTEXT("MirrorZLabel", "Z"))
				]
					
				+SHorizontalBox::Slot()
				[
					SNew(SSegmentedControl<EMirrorDirection>)
					.ToolTipText(LOCTEXT("MirrorDirectionTooltip", "The direction that determines what side of the plane to copy weights from."))
					.Value_Lambda([this]()
					{
						return ToolSettings->MirrorDirection;
					})
					.OnValueChanged_Lambda([this](EMirrorDirection Mode)
					{
						ToolSettings->MirrorDirection = Mode;
					})
					+ SSegmentedControl<EMirrorDirection>::Slot(EMirrorDirection::PositiveToNegative)
					.Text(LOCTEXT("MirrorPosToNegLabel", "+ to -"))
					+ SSegmentedControl<EMirrorDirection>::Slot(EMirrorDirection::NegativeToPositive)
					.Text(LOCTEXT("MirrorNegToPosLabel", "- to +"))
				]
			]
		]

		+SVerticalBox::Slot()
		.Padding(0.f, WeightEditVerticalPadding)
		[
			SNew(SBox)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("MirrorWeightsButtonLabel", "Mirror"))
				.ToolTipText(LOCTEXT("MirrorButtonTooltip",
					"Weights are copied across the given plane in the given direction.\n"
					"This command operates on the selected bone(s) and selected vertices.\n "
					"If no bones are selected, ALL bone weights are considered.\n "
					"If no vertices are selected, ALL vertices are considered."))
				.OnClicked_Lambda([this]()
				{
					Tool->MirrorWeights(ToolSettings->MirrorAxis, ToolSettings->MirrorDirection);
					return FReply::Handled();
				})
			]
		]
	];

	// VERTEX EDITOR category
	EditWeightsCategory.AddCustomRow(LOCTEXT("VertexEditorRow", "Component Editor"), false)
	.WholeRowContent()
	[
		SNew(SVertexWeightEditor, ToolSettings->WeightTool)
	];
}

void FSkinWeightDetailCustomization::AddTransferUI(IDetailLayoutBuilder& DetailBuilder) const
{
	if (!ensure(Tool.IsValid()))
	{
		return;
	}
	
	IDetailCategoryBuilder& TransferWeightsCategory = DetailBuilder.EditCategory("WeightTransfer", FText::GetEmpty(), ECategoryPriority::Important);
	TransferWeightsCategory.InitiallyCollapsed(true);

	TransferWeightsCategory.AddCustomRow(LOCTEXT("TransferWeightsRow", "Transfer Weights"), false)
	.WholeRowContent()
	[
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.Padding(0.f, WeightEditVerticalPadding)
		[
			SNew(SBox)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("TransferWeightsButtonLabel", "Transfer Weights"))
				.ToolTipText(LOCTEXT("TransferButtonTooltip",
					"Weights are transfered from the source skeletal mesh using inpainting.\n"
					"This command can operate on selected components when in Mesh mode."))
				.OnClicked_Lambda([this]()
				{
					Tool->TransferWeights();
					return FReply::Handled();
				})
				.IsEnabled_Lambda([this]()
				{
					return Tool->GetSourceTarget() != nullptr;
				})
			]
		]
	];
}

void SVertexWeightItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView)
{
	Element = InArgs._Element;
	ParentTable = InArgs._ParentTable;
	SMultiColumnTableRow<TSharedPtr<FWeightEditorElement>>::Construct(FSuperRowType::FArguments(), OwnerTableView);
}

TSharedRef<SWidget> SVertexWeightItem::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == "Bone")
	{
		const FName BoneName = ParentTable->Tool->GetBoneNameFromIndex(Element->BoneIndex);
		return SNew(STextBlock).Text(FText::FromName(BoneName));
	}

	if (ColumnName == "Weight")
	{
		return SNew(SNumericEntryBox<float>)
		.AllowSpin(true)
		.MinSliderValue(SliderBuffer)
		.MinValue(0.f)
		.MaxSliderValue(1.0f-SliderBuffer)
		.MaxValue(1.f)
		.Value_Lambda([this]()
		{
			if (bInTransaction)
			{
				return ValueDuringSlide;
			}
			
			return ParentTable->Tool->GetAverageWeightOnBone(Element->BoneIndex, ParentTable->Tool->GetVerticesToEdit());
		})
		.OnValueChanged_Lambda([this](float NewValue)
		{
			if (bInTransaction)
			{
				ValueDuringSlide = NewValue;
				
				const float ScaleMultiplier = NewValue / FMath::Max(ValueAtStartOfSlide, SliderBuffer);
				constexpr bool bShouldTransact = false;
				ParentTable->Tool->EditWeightsOnVertices(Element->BoneIndex, ScaleMultiplier, EWeightEditOperation::Multiply, ParentTable->Tool->GetVerticesToEdit(), bShouldTransact);
			}
		})
		.OnValueCommitted_Lambda([this](float NewValue, ETextCommit::Type CommitType)
		{
			if (!bInTransaction)
			{
				constexpr bool bShouldTransact = true;
				ParentTable->Tool->EditWeightsOnVertices(Element->BoneIndex, NewValue, EWeightEditOperation::Replace, ParentTable->Tool->GetVerticesToEdit(), bShouldTransact);
			}
			bInTransaction = false;
		})
		.OnBeginSliderMovement_Lambda([this]()
		{
			ParentTable->Tool->BeginChange();
			ValueAtStartOfSlide = ParentTable->Tool->GetAverageWeightOnBone(Element->BoneIndex, ParentTable->Tool->GetVerticesToEdit());
			ValueDuringSlide = ValueAtStartOfSlide;
			bInTransaction = true;
		})
		.OnEndSliderMovement_Lambda([this](float)
		{
			const FText TransactionLabel = LOCTEXT("DirectWeightChange", "Scale weights on vertices.");
			ParentTable->Tool->EndChange(TransactionLabel);
			bInTransaction = false;
		})
		.ToolTipText(LOCTEXT("WeightSliderToolTip", "Set the weight on this bone for the selected vertices."));
	}

	checkNoEntry();
	return SNullWidget::NullWidget;
}

SVertexWeightEditor::~SVertexWeightEditor()
{
	if (Tool.IsValid())
	{
		Tool->OnSelectionChanged.RemoveAll(this);
		Tool->OnWeightsChanged.RemoveAll(this);
		Tool.Reset();
	}
}

void SVertexWeightEditor::Construct(const FArguments& InArgs, USkinWeightsPaintTool* InSkinTool)
{
	Tool = InSkinTool;

	ChildSlot
	[
		SNew(SBox)
		[
			SAssignNew( ListView, SWeightEditorListViewType )
			.SelectionMode(ESelectionMode::Single)
			.ListItemsSource( &ListViewItems )
			.OnGenerateRow_Lambda([this](TSharedPtr<FWeightEditorElement> Element, const TSharedRef<STableViewBase>& OwnerTableView)
			{
				return SNew(SVertexWeightItem, OwnerTableView).Element(Element).ParentTable(SharedThis(this));
			})
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column("Bone").DefaultLabel(NSLOCTEXT("WeightEditorBoneColumn", "Bone", "Bone"))
				+ SHeaderRow::Column("Weight").DefaultLabel(NSLOCTEXT("WeightEditorWeightColumn", "Weight (Average)", "Weight (Average)"))
			)
		]
	];

	RefreshView();
	
	Tool->OnSelectionChanged.AddSP(this, &SVertexWeightEditor::RefreshView);
	Tool->OnWeightsChanged.AddSP(this, &SVertexWeightEditor::RefreshView);
}

void SVertexWeightEditor::RefreshView()
{
	if (!Tool.IsValid())
	{
		return; 
	}
	
	// get all bones affecting the selected vertices
	TArray<int32> Influences;
	Tool->GetInfluences(Tool->GetVerticesToEdit(), Influences);

	// generate list view items
	ListViewItems.Reset();
	for (const int32 InfluenceIndex : Influences)
	{
		ListViewItems.Add(MakeShareable(new FWeightEditorElement(InfluenceIndex)));
	}
	
	ListView->RequestListRefresh();
}

#undef LOCTEXT_NAMESPACE
