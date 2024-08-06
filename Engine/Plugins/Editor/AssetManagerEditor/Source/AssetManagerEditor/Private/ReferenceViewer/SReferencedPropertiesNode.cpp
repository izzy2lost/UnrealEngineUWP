// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReferencedPropertiesNode.h"
#include "ReferenceViewer/EdGraphNode_ReferencedProperties.h"
#include "ReferenceViewerStyle.h"
#include "SlateOptMacros.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"

/**
 * Widget representing a referencing property
 */
class SReferencedPropertyNode : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SReferencedPropertiesNode)
	{
	}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FReferencingPropertyDescription& InReferencingPropertyDescription);

private:
	FText GetPropertyDisplayName() const;
	FText GetTooltipText() const;
	const FSlateBrush* GetIconBrush() const;

	FReferencingPropertyDescription PropertyDescription;
};

void SReferencedPropertiesNode::Construct(const FArguments& InArgs, UEdGraphNode_ReferencedProperties* InReferencedPropertiesNode)
{
	GraphNode = InReferencedPropertiesNode;
	SetCursor(EMouseCursor::CardinalCross);

	if (InReferencedPropertiesNode)
	{
		InReferencedPropertiesNode->OnPropertiesDescriptionUpdated().AddRaw(this, &SReferencedPropertiesNode::UpdateGraphNode);
	}

	UpdateGraphNode();
}

SReferencedPropertiesNode::~SReferencedPropertiesNode()
{
	if (UEdGraphNode_ReferencedProperties* ReferencedProperties = Cast<UEdGraphNode_ReferencedProperties>(GraphNode))
	{
		ReferencedProperties->OnPropertiesDescriptionUpdated().RemoveAll(this);
	}
}

void SReferencedPropertiesNode::UpdateGraphNode()
{
	// No pins
	InputPins.Empty();
	OutputPins.Empty();

	// No side Boxes
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	ContentScale.Bind(this, &SReferencedPropertiesNode::GetContentScale);

	TSharedPtr<SVerticalBox> PropertiesBox;

	GetOrAddSlot(ENodeZone::Center)
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SBorder)
		.Padding(FMargin(6.0f, 4.0f, 6.0f, 4.0f))
		.BorderImage(FReferenceViewerStyle::Get().GetBrush(TEXT("Graph.ReferencedPropertiesBrush")))
		[
			SAssignNew(PropertiesBox, SVerticalBox)
		]
	];

	if (const UEdGraphNode_ReferencedProperties* ReferencedProperties = Cast<UEdGraphNode_ReferencedProperties>(GraphNode))
	{
		for (const FReferencingPropertyDescription& PropertyDescription : ReferencedProperties->GetReferencedPropertiesDescription())
		{
			PropertiesBox->AddSlot()
			[
				SNew(SReferencedPropertyNode, PropertyDescription)
			];
		}
	}
}

void SReferencedPropertiesNode::Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SGraphNode::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

	if (UEdGraphNode_ReferencedProperties* ReferencedProperties = Cast<UEdGraphNode_ReferencedProperties>(GraphNode))
	{
		const FVector2f& Size = InAllottedGeometry.GetLocalSize();
		ReferencedProperties->RefreshLocation(Size);
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SReferencedPropertyNode::Construct(const FArguments& InArgs, const FReferencingPropertyDescription& InReferencingPropertyDescription)
{
	PropertyDescription = InReferencingPropertyDescription;

	ChildSlot
	.Padding(FMargin(6.0f, 4.0f, 6.0f, 4.0f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
		.AutoWidth()
		[
			SNew(SImage)
			.Image(GetIconBrush())
			.DesiredSizeOverride(FVector2D(16.0f, 16.0f))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.ToolTipText_Raw(this, &SReferencedPropertyNode::GetTooltipText)
			[
				SNew(STextBlock)
				.TextStyle(FReferenceViewerStyle::Get(), "Graph.ReferencedPropertiesText")
				.Text(this, &SReferencedPropertyNode::GetPropertyDisplayName)
			]
		]
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FText SReferencedPropertyNode::GetPropertyDisplayName() const
{
	return FText::FromString(PropertyDescription.GetName());
}

FText SReferencedPropertyNode::GetTooltipText() const
{
	return FText::FromString(TEXT("Reference used as ") + PropertyDescription.GetTypeAsString());
}

const FSlateBrush* SReferencedPropertyNode::GetIconBrush() const
{

	const UClass* Class = PropertyDescription.GetPropertyClass();

	const FSlateBrush* ComponentIcon;

	if (Class)
	{
		ComponentIcon = FSlateIconFinder::FindIconBrushForClass(Class);
	}
	else
	{
		if (PropertyDescription.GetType() == FReferencingPropertyDescription::EAssetReferenceType::Component)
		{
			ComponentIcon = FSlateIconFinder::FindIconBrushForClass(UActorComponent::StaticClass(), TEXT("SCS.Component"));
		}
		else
		{
			ComponentIcon = FSlateIconFinder::FindIconBrushForClass(UObject::StaticClass());
		}
	}

	return ComponentIcon;
}
