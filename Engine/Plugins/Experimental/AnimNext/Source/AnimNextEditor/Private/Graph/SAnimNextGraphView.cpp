// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAnimNextGraphView.h"
#include "Widgets/SBoxPanel.h"
#include "SPrimaryButton.h"
#include "Graph/AnimNextGraph_EditorData.h"

#define LOCTEXT_NAMESPACE "SAnimNextGraphView"

namespace UE::AnimNext::Editor
{

void SAnimNextGraphView::Construct(const FArguments& InArgs, UAnimNextGraph_EditorData* InEditorData)
{
	EditorData = InEditorData;

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5.0f)
		[
			SNew(SPrimaryButton)
			.Text(LOCTEXT("OpenGraph", "Open Graph"))
			.OnClicked_Lambda([this, OnOpenGraph = InArgs._OnOpenGraph]()
			{
				OnOpenGraph.ExecuteIfBound(EditorData->GetRigVMGraphForEditorObject(EditorData->Graphs[0]));

				return FReply::Handled();
			})
		]
	];
}

}

#undef LOCTEXT_NAMESPACE
