// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPCGEditorGraphNode.h"

#include "PCGEditorGraphNodeBase.h"
#include "PCGEditorStyle.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "PCGSettingsWithDynamicInputs.h"

#include "GraphEditorSettings.h"
#include "IDocumentation.h"
#include "SCommentBubble.h"
#include "SGraphPin.h"
#include "SLevelOfDetailBranchNode.h"
#include "SPinTypeSelector.h"
#include "TutorialMetaData.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SPCGEditorGraphNode"

/** PCG pin primarily to give more control over pin coloring. */
class SPCGEditorGraphNodePin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphNodePin)
		: _PinLabelStyle(NAME_DefaultPinLabelStyle)
		, _UsePinColorForText(false)
		, _SideToSideMargin(5.0f)
		{}
		SLATE_ARGUMENT(FName, PinLabelStyle)
		SLATE_ARGUMENT(bool, UsePinColorForText)
		SLATE_ARGUMENT(float, SideToSideMargin)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

	virtual FSlateColor GetPinColor() const override;
	virtual FSlateColor GetPinTextColor() const override;
	FName GetLabelStyle(FName DefaultLabelStyle) const;
	bool GetExtraIcon(FName& OutExtraIcon, FText& OutTooltip) const;

	/** Whether pin is required to be connected for execution. */
	bool IsRequiredForExecution() const;

private:
	void ApplyUnusedPinStyle(FSlateColor& InOutColor) const;
	void GetPCGNodeAndPin(const UPCGNode*& OutNode, const UPCGPin*& OutPin) const;
};

void SPCGEditorGraphNodePin::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	// IMPLEMENTATION NOTE: this is the code from SGraphPin::Construct with additional padding exposed,
	// with an optional extra icon shown before the pin label, and with a marker icon to show pins
	// that are required for execution.

	bUsePinColorForText = InArgs._UsePinColorForText;
	this->SetCursor(EMouseCursor::Default);

	SetVisibility(MakeAttributeSP(this, &SPCGEditorGraphNodePin::GetPinVisiblity));

	GraphPinObj = InPin;
	check(GraphPinObj != NULL);

	const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
	checkf(
		Schema, 
		TEXT("Missing schema for pin: %s with outer: %s of type %s"), 
		*(GraphPinObj->GetName()),
		GraphPinObj->GetOuter() ? *(GraphPinObj->GetOuter()->GetName()) : TEXT("NULL OUTER"), 
		GraphPinObj->GetOuter() ? *(GraphPinObj->GetOuter()->GetClass()->GetName()) : TEXT("NULL OUTER")
	);

	const bool bIsInput = (GetDirection() == EGPD_Input);

	// A small marker to indicate the pin is required for the node to be executed.
	TSharedPtr<SImage> RequiredPinIconWidget;
	float RequiredPinMarkerWidth = 0.0f;
	bool bDisplayPinMarker = false;
	if (bIsInput)
	{
		const FSlateBrush* RequiredPinMarkerIcon = FPCGEditorStyle::Get().GetBrush(PCGEditorStyleConstants::Pin_Required);
		RequiredPinMarkerWidth = RequiredPinMarkerIcon ? RequiredPinMarkerIcon->GetImageSize().X : 8.0f;
		bDisplayPinMarker = IsRequiredForExecution();

		RequiredPinIconWidget =
			SNew(SImage)
			.Image(bDisplayPinMarker ? RequiredPinMarkerIcon : FAppStyle::GetNoBrush())
			.ColorAndOpacity(this, &SPCGEditorGraphNodePin::GetPinColor);
	}

	// Create the pin icon widget
	TSharedRef<SWidget> PinWidgetRef = SPinTypeSelector::ConstructPinTypeImage(
		MakeAttributeSP(this, &SPCGEditorGraphNodePin::GetPinIcon ),
		MakeAttributeSP(this, &SPCGEditorGraphNodePin::GetPinColor),
		MakeAttributeSP(this, &SPCGEditorGraphNodePin::GetSecondaryPinIcon),
		MakeAttributeSP(this, &SPCGEditorGraphNodePin::GetSecondaryPinColor));
	PinImage = PinWidgetRef;

	PinWidgetRef->SetCursor( 
		TAttribute<TOptional<EMouseCursor::Type> >::Create (
			TAttribute<TOptional<EMouseCursor::Type> >::FGetter::CreateRaw( this, &SPCGEditorGraphNodePin::GetPinCursor )
		)
	);

	// Create the pin indicator widget (used for watched values)
	static const FName NAME_NoBorder("NoBorder");
	TSharedRef<SWidget> PinStatusIndicator =
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), NAME_NoBorder)
		.Visibility(this, &SPCGEditorGraphNodePin::GetPinStatusIconVisibility)
		.ContentPadding(0)
		.OnClicked(this, &SPCGEditorGraphNodePin::ClickedOnPinStatusIcon)
		[
			SNew(SImage)
			.Image(this, &SPCGEditorGraphNodePin::GetPinStatusIcon)
		];

	TSharedRef<SWidget> LabelWidget = GetLabelWidget(GetLabelStyle(InArgs._PinLabelStyle));

	// Create the widget used for the pin body (status indicator, label, and value)
	LabelAndValue =
		SNew(SWrapBox)
		.PreferredSize(150.f);

	TSharedPtr<SImage> ExtraPinIconWidget;
	FName ExtraPinIcon;
	FText ExtraPinIconTooltip;
	if (GetExtraIcon(ExtraPinIcon, ExtraPinIconTooltip))
	{
		ExtraPinIconWidget = SNew(SImage)
			.Image(FAppStyle::GetBrush(ExtraPinIcon))
			.ColorAndOpacity(this, &SPCGEditorGraphNodePin::GetPinTextColor);

		if (!ExtraPinIconTooltip.IsEmpty())
		{
			ExtraPinIconWidget->SetToolTipText(ExtraPinIconTooltip);
		}
	}

	if (!bIsInput)
	{
		LabelAndValue->AddSlot()
			.VAlign(VAlign_Center)
			[
				PinStatusIndicator
			];

		LabelAndValue->AddSlot()
			.VAlign(VAlign_Center)
			[
				LabelWidget
			];

		if (ExtraPinIconWidget.IsValid())
		{
			LabelAndValue->AddSlot()
				.Padding(5, 0, 0, 0)
				.VAlign(VAlign_Center)
				[
					ExtraPinIconWidget.ToSharedRef()
				];
		}
	}
	else
	{
		if (ExtraPinIconWidget.IsValid())
		{
			LabelAndValue->AddSlot()
				.Padding(0, 0, 5, 0)
				.VAlign(VAlign_Center)
				[
					ExtraPinIconWidget.ToSharedRef()
				];
		}

		LabelAndValue->AddSlot()
			.VAlign(VAlign_Center)
			[
				LabelWidget
			];

		ValueWidget = GetDefaultValueWidget();

		if (ValueWidget != SNullWidget::NullWidget)
		{
			TSharedPtr<SBox> ValueBox;
			LabelAndValue->AddSlot()
				.Padding(bIsInput ? FMargin(InArgs._SideToSideMargin, 0, 0, 0) : FMargin(0, 0, InArgs._SideToSideMargin, 0))
				.VAlign(VAlign_Center)
				[
					SAssignNew(ValueBox, SBox)
					.Padding(0.0f)
					[
						ValueWidget.ToSharedRef()
					]
				];

			if (!DoesWidgetHandleSettingEditingEnabled())
			{
				ValueBox->SetEnabled(TAttribute<bool>(this, &SPCGEditorGraphNodePin::IsEditingEnabled));
			}
		}

		LabelAndValue->AddSlot()
			.VAlign(VAlign_Center)
			[
				PinStatusIndicator
			];
	}

	TSharedPtr<SHorizontalBox> PinContent;
	if (bIsInput)
	{
		// Input pin
		FullPinHorizontalRowWidget = PinContent = 
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				RequiredPinIconWidget.ToSharedRef()
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(bDisplayPinMarker ? 0.0f : RequiredPinMarkerWidth, 0, InArgs._SideToSideMargin, 0)
			[
				PinWidgetRef
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				LabelAndValue.ToSharedRef()
			];
	}
	else
	{
		// Output pin
		FullPinHorizontalRowWidget = PinContent = SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				LabelAndValue.ToSharedRef()
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(InArgs._SideToSideMargin, 0, 0, 0)
			[
				PinWidgetRef
			];
	}

	// Set up a hover for pins that is tinted the color of the pin.
	
	SBorder::Construct(SBorder::FArguments()
		.BorderImage(this, &SPCGEditorGraphNodePin::GetPinBorder)
		.BorderBackgroundColor(this, &SPCGEditorGraphNodePin::GetHighlightColor)
		.OnMouseButtonDown(this, &SPCGEditorGraphNodePin::OnPinNameMouseDown)
		.Padding(0) // NOTE: This is different from base class implementation
		[
			SNew(SBorder)
			.BorderImage(CachedImg_Pin_DiffOutline)
			.BorderBackgroundColor(this, &SPCGEditorGraphNodePin::GetPinDiffColor)
			.Padding(0) // NOTE: This is different from base class implementation
			[
				SNew(SLevelOfDetailBranchNode)
				.UseLowDetailSlot(this, &SPCGEditorGraphNodePin::UseLowDetailPinNames)
				.LowDetail()
				[
					//@TODO: Try creating a pin-colored line replacement that doesn't measure text / call delegates but still renders
					PinWidgetRef
				]
				.HighDetail()
				[
					PinContent.ToSharedRef()
				]
			]
		]
	);

	TSharedPtr<IToolTip> TooltipWidget = SNew(SToolTip)
		.Text(this, &SPCGEditorGraphNodePin::GetTooltipText);

	SetToolTip(TooltipWidget);

}

void SPCGEditorGraphNodePin::GetPCGNodeAndPin(const UPCGNode*& OutNode, const UPCGPin*& OutPin) const
{
	UEdGraphPin* GraphPin = GetPinObj();
	if (GraphPin && !GraphPin->IsPendingKill())
	{
		const UPCGEditorGraphNodeBase* EditorNode = CastChecked<const UPCGEditorGraphNodeBase>(GraphPinObj->GetOwningNode());
		
		OutNode = EditorNode ? EditorNode->GetPCGNode() : nullptr;

		OutPin = OutNode ? OutNode->GetInputPin(GraphPin->GetFName()) : nullptr;
		if (!OutPin)
		{
			OutPin = OutNode ? OutNode->GetOutputPin(GraphPin->GetFName()) : nullptr;
		}
	}
	else
	{
		OutNode = nullptr;
		OutPin = nullptr;
	}
}

void SPCGEditorGraphNodePin::ApplyUnusedPinStyle(FSlateColor& InOutColor) const
{
	const UPCGPin* PCGPin = nullptr;
	const UPCGNode* PCGNode = nullptr;
	GetPCGNodeAndPin(PCGNode, PCGPin);

	bool bPinDisabled = false;

	// Check if the pin was deactivated in the previous execution.
	const UEdGraphPin* Pin = GetPinObj();
	if (Pin && Pin->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		if (const UPCGEditorGraphNodeBase* Node = Cast<UPCGEditorGraphNodeBase>(Pin ? Pin->GetOwningNode() : nullptr))
		{
			// If node is already disabled, don't bother disabling pin on top of that, does not look nice to disable both and may
			// not be meaningful to do so in any case.
			if (!Node->IsDisplayAsDisabledForced())
			{
				bPinDisabled = !Node->IsOutputPinActive(Pin);
			}
		}
	}

	// Halve opacity if pin is unused - intended to happen whether disabled or not
	if (bPinDisabled || (PCGPin && PCGNode && !PCGNode->IsPinUsedByNodeExecution(PCGPin)))
	{
		FLinearColor Color = InOutColor.GetSpecifiedColor();
		Color.A *= 0.5;
		InOutColor = Color;
	}
}

// Adapted from SGraphPin::GetPinColor
FSlateColor SPCGEditorGraphNodePin::GetPinColor() const
{
	FSlateColor Color = SGraphPin::GetPinColor();

	ApplyUnusedPinStyle(Color);

	return Color;
}

FSlateColor SPCGEditorGraphNodePin::GetPinTextColor() const
{
	FSlateColor Color = SGraphPin::GetPinTextColor();

	ApplyUnusedPinStyle(Color);

	return Color;
}

FName SPCGEditorGraphNodePin::GetLabelStyle(FName DefaultLabelStyle) const
{
	const UPCGPin* PCGPin = nullptr;
	const UPCGNode* PCGNode = nullptr;
	FName LabelStyle = NAME_None;

	GetPCGNodeAndPin(PCGNode, PCGPin);

	const UPCGSettings* Settings = PCGNode ? PCGNode->GetSettings() : nullptr;

	if (!PCGPin || !Settings || !Settings->GetPinLabelStyle(PCGPin, LabelStyle))
	{
		LabelStyle = DefaultLabelStyle;
	}

	return LabelStyle;
}

bool SPCGEditorGraphNodePin::GetExtraIcon(FName& OutExtraIcon, FText& OutTooltip) const
{
	const UPCGPin* PCGPin = nullptr;
	const UPCGNode* PCGNode = nullptr;

	GetPCGNodeAndPin(PCGNode, PCGPin);

	const UPCGSettings* Settings = PCGNode ? PCGNode->GetSettings() : nullptr;
	return (PCGPin && Settings) ? Settings->GetPinExtraIcon(PCGPin, OutExtraIcon, OutTooltip) : false;
}

bool SPCGEditorGraphNodePin::IsRequiredForExecution() const
{
	const UPCGPin* PCGPin = nullptr;
	const UPCGNode* PCGNode = nullptr;
	GetPCGNodeAndPin(PCGNode, PCGPin);

	return PCGPin && PCGNode->IsInputPinRequiredByExecution(PCGPin);
}

void SPCGEditorGraphNode::Construct(const FArguments& InArgs, UPCGEditorGraphNodeBase* InNode)
{
	GraphNode = InNode;
	PCGEditorGraphNode = InNode;

	if (InNode)
	{
		InNode->OnNodeChangedDelegate.BindSP(this, &SPCGEditorGraphNode::OnNodeChanged);
		InNode->OnNodeRenameInitiatedDelegate.BindSP(this, &SPCGEditorGraphNode::OnNodeRenameInitiated);
	}

	UpdateGraphNode();
}

void SPCGEditorGraphNode::CreateAddPinButtonWidget()
{
	// Add Pin Button (+) Source Reference: Engine\Source\Editor\GraphEditor\Private\KismetNodes\SGraphNodeK2Sequence.cpp
	const TSharedPtr<SWidget> AddPinButton = AddPinButtonContent(LOCTEXT("AddSourcePin", "Add Pin"), LOCTEXT("AddSourcePinTooltip", "Add a dynamic source input pin"));

	FMargin AddPinPadding = Settings->GetInputPinPadding();
	AddPinPadding.Top += 6.0f;

	check(PCGEditorGraphNode->GetPCGNode());
	const UPCGSettingsWithDynamicInputs* NodeSettings = CastChecked<UPCGSettingsWithDynamicInputs>(PCGEditorGraphNode->GetPCGNode()->GetSettings());

	const int32 Index = NodeSettings->GetStaticInputPinNum() + NodeSettings->GetDynamicInputPinNum();
	LeftNodeBox->InsertSlot(Index)
		.AutoHeight()
		.VAlign(VAlign_Bottom)
		.Padding(AddPinPadding)
		[
			AddPinButton.ToSharedRef()
		];
}

void SPCGEditorGraphNode::UpdateGraphNode()
{
	if (PCGEditorGraphNode && PCGEditorGraphNode->ShouldDrawCompact())
	{
		UpdateCompactNode();
	}
	else
	{
		SGraphNode::UpdateGraphNode();

		if (PCGEditorGraphNode->CanUserAddRemoveDynamicInputPins())
		{
			CreateAddPinButtonWidget();
		}
	}
}

const FSlateBrush* SPCGEditorGraphNode::GetNodeBodyBrush() const
{
	const bool bNeedsTint = PCGEditorGraphNode &&
		((PCGEditorGraphNode->GetPCGNode() && PCGEditorGraphNode->GetPCGNode()->IsInstance()) || PCGEditorGraphNode->IsHighlighted());
	if (bNeedsTint)
	{
		return FAppStyle::GetBrush("Graph.Node.TintedBody");
	}
	else
	{
		return FAppStyle::GetBrush("Graph.Node.Body");
	}
}

TSharedRef<SWidget> SPCGEditorGraphNode::CreateTitleWidget(TSharedPtr<SNodeTitle> InNodeTitle)
{
	// Reimplementation of the SGraphNode::CreateTitleWidget so we can control the style
	const bool bIsInstanceNode = (PCGEditorGraphNode && PCGEditorGraphNode->GetPCGNode() && PCGEditorGraphNode->GetPCGNode()->IsInstance());

	SAssignNew(InlineEditableText, SInlineEditableTextBlock)
		.Style(FPCGEditorStyle::Get(), bIsInstanceNode ? "PCG.Node.InstancedNodeTitleInlineEditableText" : "PCG.Node.NodeTitleInlineEditableText")
		.Text(InNodeTitle.Get(), &SNodeTitle::GetHeadTitle)
		.OnVerifyTextChanged(this, &SPCGEditorGraphNode::OnVerifyNameTextChanged)
		.OnTextCommitted(this, &SPCGEditorGraphNode::OnNameTextCommited)
		.IsReadOnly(this, &SPCGEditorGraphNode::IsNameReadOnly)
		.IsSelected(this, &SPCGEditorGraphNode::IsSelectedExclusively);
	InlineEditableText->SetColorAndOpacity(TAttribute<FLinearColor>::Create(TAttribute<FLinearColor>::FGetter::CreateSP(this, &SPCGEditorGraphNode::GetNodeTitleTextColor)));

	return InlineEditableText.ToSharedRef();
}

TSharedPtr<SGraphPin> SPCGEditorGraphNode::CreatePinWidget(UEdGraphPin* Pin) const
{
	return SNew(SPCGEditorGraphNodePin, Pin);
}

EVisibility SPCGEditorGraphNode::IsAddPinButtonVisible() const
{
	if (PCGEditorGraphNode && PCGEditorGraphNode->IsNodeEnabled() && PCGEditorGraphNode->CanUserAddRemoveDynamicInputPins() && SGraphNode::IsAddPinButtonVisible() == EVisibility::Visible)
	{
		return EVisibility::Visible;
	}
	
	return EVisibility::Hidden;
}

FReply SPCGEditorGraphNode::OnAddPin()
{
	check(PCGEditorGraphNode);

	PCGEditorGraphNode->OnUserAddDynamicInputPin();
	
	return FReply::Handled();
}

void SPCGEditorGraphNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	check(PCGEditorGraphNode);
	UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();

	if (PCGNode && PinToAdd->GetPinObj())
	{
		const bool bIsInPin = PinToAdd->GetPinObj()->Direction == EEdGraphPinDirection::EGPD_Input;
		const FName& PinName = PinToAdd->GetPinObj()->PinName;

		if(UPCGPin* Pin = (bIsInPin ? PCGNode->GetInputPin(PinName) : PCGNode->GetOutputPin(PinName)))
		{
			const bool bIsMultiData = Pin->Properties.bAllowMultipleData;
			const bool bIsMultiConnections = Pin->AllowsMultipleConnections();

			// Check for special types
			if (Pin->GetCurrentTypes() == EPCGDataType::Param)
			{
				const FSlateBrush* ConnectedBrush = FPCGEditorStyle::Get().GetBrush(bIsInPin ? PCGEditorStyleConstants::Pin_Param_IN_C : PCGEditorStyleConstants::Pin_Param_OUT_C);
				const FSlateBrush* DisconnectedBrush = FPCGEditorStyle::Get().GetBrush(bIsInPin ? PCGEditorStyleConstants::Pin_Param_IN_DC : PCGEditorStyleConstants::Pin_Param_OUT_DC);

				PinToAdd->SetCustomPinIcon(ConnectedBrush, DisconnectedBrush);
			}
			else if (Pin->GetCurrentTypes() == EPCGDataType::Spatial)
			{
				const FSlateBrush* ConnectedBrush = FPCGEditorStyle::Get().GetBrush(bIsInPin ? PCGEditorStyleConstants::Pin_Composite_IN_C : PCGEditorStyleConstants::Pin_Composite_OUT_C);
				const FSlateBrush* DisconnectedBrush = FPCGEditorStyle::Get().GetBrush(bIsInPin ? PCGEditorStyleConstants::Pin_Composite_IN_DC : PCGEditorStyleConstants::Pin_Composite_OUT_DC);

				PinToAdd->SetCustomPinIcon(ConnectedBrush, DisconnectedBrush);
			}
			else
			{
				// Node outputs are always single collection (SC).
				static const FName* PinBrushes[] =
				{
					&PCGEditorStyleConstants::Pin_SD_SC_IN_C,
					&PCGEditorStyleConstants::Pin_SD_SC_IN_DC,
					&PCGEditorStyleConstants::Pin_SD_MC_IN_C,
					&PCGEditorStyleConstants::Pin_SD_MC_IN_DC,
					&PCGEditorStyleConstants::Pin_MD_SC_IN_C,
					&PCGEditorStyleConstants::Pin_MD_SC_IN_DC,
					&PCGEditorStyleConstants::Pin_MD_MC_IN_C,
					&PCGEditorStyleConstants::Pin_MD_MC_IN_DC,
					&PCGEditorStyleConstants::Pin_SD_SC_OUT_C,
					&PCGEditorStyleConstants::Pin_SD_SC_OUT_DC,
					&PCGEditorStyleConstants::Pin_SD_SC_OUT_C,
					&PCGEditorStyleConstants::Pin_SD_SC_OUT_DC,
					&PCGEditorStyleConstants::Pin_MD_SC_OUT_C,
					&PCGEditorStyleConstants::Pin_MD_SC_OUT_DC,
					&PCGEditorStyleConstants::Pin_MD_SC_OUT_C,
					&PCGEditorStyleConstants::Pin_MD_SC_OUT_DC
				};

				const int32 ConnectedIndex = (bIsInPin ? 0 : 8) + (bIsMultiData ? 4 : 0) + (bIsMultiConnections ? 2 : 0);
				const int32 DisconnectedIndex = ConnectedIndex + 1;

				const FSlateBrush* ConnectedBrush = FPCGEditorStyle::Get().GetBrush(*PinBrushes[ConnectedIndex]);
				const FSlateBrush* DisconnectedBrush = FPCGEditorStyle::Get().GetBrush(*PinBrushes[DisconnectedIndex]);

				PinToAdd->SetCustomPinIcon(ConnectedBrush, DisconnectedBrush);
			}
		}
	}

	SGraphNode::AddPin(PinToAdd);

	// The base class does not give an override to change the padding of the pin widgets, so do it here. Our input pins widgets include
	// a small marker to indicate the pin is required, which need to display at the left edge of the node, so remove left padding.
	if (PinToAdd->GetDirection() == EEdGraphPinDirection::EGPD_Input)
	{
		const int LastIndex = LeftNodeBox->GetChildren()->Num() - 1;
		check(LastIndex >= 0);

		SVerticalBox::FSlot& PinSlot = LeftNodeBox->GetSlot(LastIndex);

		FMargin Margin = Settings->GetInputPinPadding();
		Margin.Left = 0;
		PinSlot.SetPadding(Margin);
	}
}

void SPCGEditorGraphNode::GetOverlayBrushes(bool bSelected, const FVector2D WidgetSize, TArray<FOverlayBrushInfo>& Brushes) const
{
	check(PCGEditorGraphNode);
	
	FVector2D OverlayOffset(0.0, 0.0);

	auto AddOverlayBrush = [&OverlayOffset, &Brushes](const FName& BrushName)
	{
		const FSlateBrush* Brush = FPCGEditorStyle::Get().GetBrush(BrushName);

		if (Brush)
		{
			FOverlayBrushInfo BrushInfo;
			BrushInfo.Brush = Brush;
			BrushInfo.OverlayOffset = OverlayOffset - Brush->GetImageSize() / 2.0;
			Brushes.Add(BrushInfo);

			OverlayOffset.Y += Brush->GetImageSize().Y;
		}
	};

	if (PCGEditorGraphNode->IsCulledFromExecution())
	{
		AddOverlayBrush(PCGEditorStyleConstants::Node_Overlay_Inactive);
	}

	if (const UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode())
	{
		if (PCGNode->GetSettingsInterface() && PCGNode->GetSettingsInterface()->bDebug)
		{
			AddOverlayBrush(TEXT("PCG.NodeOverlay.Debug"));
		}
	}

	if (PCGEditorGraphNode->GetInspected())
	{
		AddOverlayBrush(TEXT("PCG.NodeOverlay.Inspect"));
	}
}

void SPCGEditorGraphNode::OnNodeChanged()
{
	UpdateGraphNode();
}

void SPCGEditorGraphNode::OnNodeRenameInitiated()
{
	if (InlineEditableText.IsValid())
	{
		InlineEditableText->EnterEditingMode();
	}
}

void SPCGEditorGraphNode::UpdateCompactNode()
{
	// Based on SGraphNodeK2Base::UpdateCompactNode. Changes:
	// * Removed creation of tooltip widget, it did not port across trivially and the usage is fairly obvious for the current
	//   compact nodes, but this could be re-added - TODO
	// * Changed title style - reduced font size substantially
	// * Layout differentiation for "pure" vs "impure" K2 nodes removed

	InputPins.Empty();
	OutputPins.Empty();

	// Error handling set-up
	SetupErrorReporting();

	// Reset variables that are going to be exposed, in case we are refreshing an already setup node.
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	if (!SWidget::GetToolTip().IsValid())
	{
		TSharedRef<SToolTip> DefaultToolTip = IDocumentation::Get()->CreateToolTip(TAttribute< FText >(this, &SGraphNode::GetNodeTooltip), NULL, GraphNode->GetDocumentationLink(), GraphNode->GetDocumentationExcerptName());
		SetToolTip(DefaultToolTip);
	}

	// Setup a meta tag for this node
	FGraphNodeMetaData TagMeta(TEXT("Graphnode"));
	PopulateMetaTag(&TagMeta);
	
	TSharedPtr<SNodeTitle> NodeTitle = SNew(SNodeTitle, GraphNode);
	TSharedRef<SOverlay> NodeOverlay = SNew(SOverlay);
	
	IconColor = FLinearColor::White;

	// Add optional node specific widget to the overlay:
	TSharedPtr<SWidget> OverlayWidget = GraphNode->CreateNodeImage();
	if(OverlayWidget.IsValid())
	{
		NodeOverlay->AddSlot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew( SBox )
			.WidthOverride( 70.f )
			.HeightOverride( 70.f )
			[
				OverlayWidget.ToSharedRef()
			]
		];
	}

	FName CompactBodyIcon = NAME_None;
	check(PCGEditorGraphNode);
	if (PCGEditorGraphNode->GetCompactNodeIcon(CompactBodyIcon))
	{
		NodeOverlay->AddSlot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(45.f, 0.f, 45.f, 0.f)
			[
				SNew(SImage)
					.Image(FPCGEditorStyle::Get().GetBrush(CompactBodyIcon))
					.ColorAndOpacity(this, &SGraphNode::GetNodeTitleIconColor)
			];
	}
	else
	{
		NodeOverlay->AddSlot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(45.f, 0.f, 45.f, 0.f)
			[
				// MIDDLE
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.AutoHeight()
				[
					SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "Graph.Node.NodeTitle" )
						.Text( NodeTitle.Get(), &SNodeTitle::GetHeadTitle )
						.WrapTextAt(128.0f)
						.ColorAndOpacity(this, &SGraphNode::GetNodeTitleIconColor)
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					NodeTitle.ToSharedRef()
				]
			];
	}
	
	NodeOverlay->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(0.f, 0.f, 55.f, 0.f)
		[
			// LEFT
			SAssignNew(LeftNodeBox, SVerticalBox)
		];

	NodeOverlay->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(55.f, 0.f, 0.f, 0.f)
		[
			// RIGHT
			SAssignNew(RightNodeBox, SVerticalBox)
		];

	//
	//             ______________________
	//            | (>) L |      | R (>) |
	//            | (>) E |      | I (>) |
	//            | (>) F |   +  | G (>) |
	//            | (>) T |      | H (>) |
	//            |       |      | T (>) |
	//            |_______|______|_______|
	//
	this->ContentScale.Bind( this, &SGraphNode::GetContentScale );
	
	TSharedRef<SVerticalBox> InnerVerticalBox =
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		[
			// NODE CONTENT AREA
			SNew( SOverlay)
			+SOverlay::Slot()
			[
				SNew(SImage)
				.Image( FAppStyle::GetBrush("Graph.VarNode.Body") )
				.ColorAndOpacity(this, &SGraphNode::GetNodeBodyColor)
			]
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image( FAppStyle::GetBrush("Graph.VarNode.Gloss") )
				.ColorAndOpacity(this, &SGraphNode::GetNodeBodyColor)
			]
			+SOverlay::Slot()
			.Padding( FMargin(0,3) )
			[
				NodeOverlay
			]
		];
	
	TSharedPtr<SWidget> EnabledStateWidget = GetEnabledStateWidget();
	if (EnabledStateWidget.IsValid())
	{
		InnerVerticalBox->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Top)
			.Padding(FMargin(2, 0))
			[
				EnabledStateWidget.ToSharedRef()
			];
	}

	InnerVerticalBox->AddSlot()
		.AutoHeight()
		.Padding( FMargin(5.0f, 1.0f) )
		[
			ErrorReporting->AsWidget()
		];

	this->GetOrAddSlot( ENodeZone::Center )
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			InnerVerticalBox
		];

	CreatePinWidgets();

	// Hide pin labels
	for (auto InputPin: this->InputPins)
	{
		if (InputPin->GetPinObj()->ParentPin == nullptr)
		{
			InputPin->SetShowLabel(false);
		}
	}

	for (auto OutputPin : this->OutputPins)
	{
		if (OutputPin->GetPinObj()->ParentPin == nullptr)
		{
			OutputPin->SetShowLabel(false);
		}
	}

	// Create comment bubble
	TSharedPtr<SCommentBubble> CommentBubble;
	const FSlateColor CommentColor = GetDefault<UGraphEditorSettings>()->DefaultCommentNodeTitleColor;

	SAssignNew( CommentBubble, SCommentBubble )
		.GraphNode( GraphNode )
		.Text( this, &SGraphNode::GetNodeComment )
		.OnTextCommitted( this, &SGraphNode::OnCommentTextCommitted )
		.ColorAndOpacity( CommentColor )
		.AllowPinning( true )
		.EnableTitleBarBubble( true )
		.EnableBubbleCtrls( true )
		.GraphLOD( this, &SGraphNode::GetCurrentLOD )
		.IsGraphNodeHovered( this, &SGraphNode::IsHovered );

	GetOrAddSlot( ENodeZone::TopCenter )
		.SlotOffset(TAttribute<FVector2D>(CommentBubble.Get(), &SCommentBubble::GetOffset))
		.SlotSize(TAttribute<FVector2D>(CommentBubble.Get(), &SCommentBubble::GetSize))
		.AllowScaling(TAttribute<bool>(CommentBubble.Get(), &SCommentBubble::IsScalingAllowed))
		.VAlign(VAlign_Top)
		[
			CommentBubble.ToSharedRef()
		];

	CreateInputSideAddButton(LeftNodeBox);
	CreateOutputSideAddButton(RightNodeBox);
}

#undef LOCTEXT_NAMESPACE
