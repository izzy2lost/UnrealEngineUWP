// Copyright Epic Games, Inc. All Rights Reserved.
#include "Dataflow/DataflowSimulationViewportToolbar.h"
#include "Dataflow/DataflowSimulationViewport.h"
#include "Dataflow/DataflowEditorCommands.h"
#include "Styling/AppStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SEditorViewportToolBarMenu.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "DataflowSimulationViewportToolBar"

void SDataflowSimulationViewportToolBar::Construct(const FArguments& InArgs, TSharedPtr<SDataflowSimulationViewport> InDataflowViewport)
{
	EditorViewport = InDataflowViewport;
	CommandList = InArgs._CommandList;
	Extenders = InArgs._Extenders;
	
	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments(), InDataflowViewport);
}

void SDataflowSimulationViewportToolBar::ExtendLeftAlignedToolbarSlots(TSharedPtr<SHorizontalBox> MainBoxPtr, TSharedPtr<SViewportToolBar> ParentToolBarPtr) const
{
	const FMargin ToolbarSlotPadding(2.0f, 2.0f);
	MainBoxPtr->AddSlot()
		.Padding(ToolbarSlotPadding)
		[
			MakeToolBar(Extenders)
		];
}

TSharedRef<SWidget> SDataflowSimulationViewportToolBar::MakeToolBar(const TSharedPtr<FExtender> InExtenders) const
{
	FSlimHorizontalToolBarBuilder ToolbarBuilder(CommandList, FMultiBoxCustomization::None, InExtenders);
	
	const FName ToolBarStyle = "EditorViewportToolBar";
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), ToolBarStyle);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

	ToolbarBuilder.BeginSection("Sim Controls");
	ToolbarBuilder.BeginBlockGroup();
	{
		// the simulation caching should be triggered from the simulation panel as well
		// we keep it here as well to have a template to add future simulation controls
		ToolbarBuilder.AddToolBarButton(FDataflowEditorCommands::Get().UpdateSimulationCache,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Animation.Record"),
			FName(*FDataflowEditorCommands::Get().UpdateSimulationCacheIdentifier));
	}
	ToolbarBuilder.EndBlockGroup();
	ToolbarBuilder.EndSection();

	return ToolbarBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
