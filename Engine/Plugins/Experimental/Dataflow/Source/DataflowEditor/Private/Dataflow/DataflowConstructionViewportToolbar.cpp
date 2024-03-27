// Copyright Epic Games, Inc. All Rights Reserved.
#include "Dataflow/DataflowConstructionViewportToolbar.h"
#include "Dataflow/DataflowEditorViewport.h"

void SDataflowConstructionViewportSelectionToolBar::Construct(const FArguments& InArgs, TSharedPtr<SDataflowEditorViewport> InDataflowViewport)
{
	EditorViewport = InDataflowViewport;
	
	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments(), InDataflowViewport);
}