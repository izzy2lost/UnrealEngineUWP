// Copyright Epic Games, Inc. All Rights Reserved.
#include "Dataflow/DataflowConstructionViewportToolbar.h"
#include "Dataflow/DataflowConstructionViewport.h"

void SDataflowConstructionViewportSelectionToolBar::Construct(const FArguments& InArgs, TSharedPtr<SDataflowConstructionViewport> InDataflowViewport)
{
	EditorViewport = InDataflowViewport;
	
	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments(), InDataflowViewport);
}