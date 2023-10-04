// Copyright Epic Games, Inc. All Rights Reserved.

#include "SConcertReplicationSubobjectEditor.h"

#include "SubobjectDataHandle.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "SConcertReplicationSubobjectEditor"

namespace UE::ConcertClientSharedSlate
{
	void SConcertReplicationSubobjectEditor::Construct(const FArguments& InArgs)
	{
		OnSubobjectsSelectedDelegate = InArgs._OnSubobjectsSelected;
		
		OnSelectionUpdated.BindSP(this, &SConcertReplicationSubobjectEditor::HandleSelectionUpdated);
		ObjectContext = TAttribute<UObject*>::CreateSP(this, &SConcertReplicationSubobjectEditor::GetContextObject);
		AllowEditing = false;
		bAllowTreeUpdates = true;
		// No commands for now... but parent class requires this or it will crash
		CommandList = MakeShareable(new FUICommandList);
		ConstructTreeWidget();
		
		ButtonBox = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(0.0f)
		.FillWidth(1.f)
		[
			SAssignNew(FilterBox, SSearchBox)
			.HintText(LOCTEXT("SearchHint", "Search"))
			.OnTextChanged(this, &SConcertReplicationSubobjectEditor::OnFilterTextChanged)
		];

		ChildSlot
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			.Padding(4.f, 0, 4.f, 4.f)
			[
				ButtonBox.ToSharedRef()
			]
			
			+SVerticalBox::Slot()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("SCSEditor.Background"))
				.Padding(4.f)
				[
					TreeWidget.ToSharedRef()
				]
			]
		];
	}

	void SConcertReplicationSubobjectEditor::SetDisplayedRootObject(UObject* Object)
	{
		DisplayedRootObject = Object;
		UpdateTree();
	}

	TSharedPtr<SWidget> SConcertReplicationSubobjectEditor::BuildSceneRootDropActionMenu(FSubobjectEditorTreeNodePtrType DroppedOntoNodePtr, FSubobjectEditorTreeNodePtrType DroppedNodePtr)
	{
		return SNullWidget::NullWidget;
	}

	FSubobjectDataHandle SConcertReplicationSubobjectEditor::AddNewSubobject(
		const FSubobjectDataHandle& ParentHandle,
		UClass* NewClass,
		UObject* AssetOverride,
		FText& OutFailReason,
		TUniquePtr<FScopedTransaction> InOngoingTransaction
		)
	{
		// We do not edit the hierarchy. Ever. See AllowEditing being set to false in Construct.
		checkNoEntry();
		return FSubobjectDataHandle{};
	}

	UObject* SConcertReplicationSubobjectEditor::GetContextObject() const
	{
		return DisplayedRootObject.Get();
	}

	void SConcertReplicationSubobjectEditor::HandleSelectionUpdated(const TArray<TSharedPtr<FSubobjectEditorTreeNode>>& SelectedNodes) const
	{
		OnSubobjectsSelectedDelegate.Execute();
	}

	TSet<const UObject*> SConcertReplicationSubobjectEditor::GetSelectedObjectsFromNodes(const TArray<TSharedPtr<FSubobjectEditorTreeNode>>& SelectedNodes)
	{
		TSet<const UObject*> ObjectPaths;
		Algo::Transform(SelectedNodes, ObjectPaths, [](const TSharedPtr<FSubobjectEditorTreeNode>& Node)
		{
			return Node->GetObject();
		});

		ObjectPaths.Remove(nullptr);
		return ObjectPaths;
	}
}

#undef LOCTEXT_NAMESPACE