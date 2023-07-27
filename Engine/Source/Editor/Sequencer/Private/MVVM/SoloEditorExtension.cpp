// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/SoloEditorExtension.h"

#include "MovieScene.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/Extensions/ISoloableExtension.h"
#include "MVVM/Selection/Selection.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "ScopedTransaction.h"
#include "Sequencer.h"

namespace UE::Sequencer
{
	
FSoloEditorExtension::FSoloEditorExtension()
{

}

void FSoloEditorExtension::OnCreated(TSharedRef<FViewModel> InWeakOwner)
{
	ensureMsgf(!WeakOwnerModel.Pin().IsValid(), TEXT("This extension was already created!"));
	WeakOwnerModel = InWeakOwner->CastThisShared<FSequencerEditorViewModel>();
}

bool FSoloEditorExtension::IsNodeSoloed(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const
{
	TViewModelPtr<IOutlinerExtension> Item = InWeakOutlinerExtension.Pin();
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = WeakOwnerModel.Pin();
	TSharedPtr<FSequencer> Sequencer = EditorViewModel ? EditorViewModel->GetSequencerImpl() : nullptr;
	if (Item && Sequencer)
	{
		const TArray<FString>& SoloNodes = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetSoloNodes();
		const FString NodePath = IOutlinerExtension::GetPathName(Item);

		if (SoloNodes.Contains(NodePath))
		{
			return true;
		}
	}

	return false;
}

void FSoloEditorExtension::SetNodeSoloed(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension, bool bInIsSoloed)
{
	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "SetNodeSoloed", "Set Node Soloed"));

	TViewModelPtr<IOutlinerExtension> OutlinerItem = InWeakOutlinerExtension.Pin();
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = WeakOwnerModel.Pin();
	TSharedPtr<FSequencer> Sequencer = EditorViewModel ? EditorViewModel->GetSequencerImpl() : nullptr;

	if (Sequencer && OutlinerItem)
	{
		UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();

		if (MovieScene->IsReadOnly())
		{
			return;
		}

		TArray<FString>& SoloNodes = MovieScene->GetSoloNodes();

		MovieScene->Modify();

		if (OutlinerItem->GetSelectionState() == EOutlinerSelectionState::SelectedDirectly)
		{
			// if selected, modify all selected items
			for (FViewModelPtr Node : Sequencer->GetViewModel()->GetSelection()->Outliner)
			{
				FString NodePath = IOutlinerExtension::GetPathName(Node);
				if (bInIsSoloed)
				{
					SoloNodes.AddUnique(NodePath);
				}
				else
				{
					SoloNodes.Remove(NodePath);
				}
			}
		}
		else
		{
			// modify only the toggled item
			TSharedPtr<FViewModel> Item = OutlinerItem;
			FString NodePath = IOutlinerExtension::GetPathName(Item);
			if (bInIsSoloed)
			{
				SoloNodes.AddUnique(NodePath);
			}
			else
			{
				SoloNodes.Remove(NodePath);
			}
		}
	}
}

bool FSoloEditorExtension::IsNodeSoloable(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const
{
	TSharedPtr<FViewModel> Item = InWeakOutlinerExtension.Pin();
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = WeakOwnerModel.Pin();
	TSharedPtr<FSequencer> Sequencer = EditorViewModel ? EditorViewModel->GetSequencerImpl() : nullptr;
	
	if (Sequencer && Item)
	{
		FString NodePath = IOutlinerExtension::GetPathName(Item);
		return Sequencer->GetNodeTree()->GetNodeAtPath(NodePath) != nullptr;
	}
	return false;
}

bool FSoloEditorExtension::HasSoloedChildNode(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const
{
	using namespace UE::Sequencer;

	TViewModelPtr<IOutlinerExtension> OutlinerItem = InWeakOutlinerExtension.Pin();

	if (OutlinerItem)
	{
		for (const TViewModelPtr<IOutlinerExtension>& Child : OutlinerItem.AsModel()->GetDescendantsOfType<IOutlinerExtension>(false, EViewModelListType::Outliner))
		{
			if (IsNodeSoloed(Child))
			{
				return true;
			}
		}
	}

	return false;
}

bool FSoloEditorExtension::IsNodeImplicitlySoloed(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const
{
	TViewModelPtr<IOutlinerExtension> Item = InWeakOutlinerExtension.Pin();
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = WeakOwnerModel.Pin();
	TSharedPtr<FSequencer> Sequencer = EditorViewModel ? EditorViewModel->GetSequencerImpl() : nullptr;
	if (Item && Sequencer)
	{
		const TArray<FString>& SoloNodes = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetSoloNodes();

		TViewModelPtr<IOutlinerExtension> ParentNode = Item.AsModel()->FindAncestorOfType<IOutlinerExtension>();
		while (ParentNode)
		{
			const FString NodePath = IOutlinerExtension::GetPathName(ParentNode);
			if (SoloNodes.Contains(NodePath))
			{
				return true;
			}

			// continue traversing upwards until no more parents
			ParentNode = ParentNode.AsModel()->FindAncestorOfType<IOutlinerExtension>();
		}
	}

	return false;
}

} // namespace UE::Sequencer
