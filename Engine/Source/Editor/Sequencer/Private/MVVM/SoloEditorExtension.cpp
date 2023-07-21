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
	TViewModelPtr<IOutlinerExtension> OutlinerItem = InWeakOutlinerExtension.Pin();
	if (OutlinerItem)
	{
		if (TViewModelPtr<ISoloableExtension> Soloable = OutlinerItem.ImplicitCast())
		{
			return Soloable->IsSolo();
		}
	}

	return false;
}

void FSoloEditorExtension::SetNodeSoloed(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension, bool bInIsSoloed)
{
	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "SetNodeSoloed", "Set Node Soloed"));

	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = WeakOwnerModel.Pin();
	TSharedPtr<FSequencer> Sequencer = EditorViewModel ? EditorViewModel->GetSequencerImpl() : nullptr;
	TViewModelPtr<IOutlinerExtension> OutlinerItem = InWeakOutlinerExtension.Pin();

	if (Sequencer)
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
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = WeakOwnerModel.Pin();
	TSharedPtr<FSequencer> Sequencer = EditorViewModel ? EditorViewModel->GetSequencerImpl() : nullptr;
	
	if (Sequencer)
	{
		TSharedPtr<FViewModel> Item = InWeakOutlinerExtension.Pin();
		FString NodePath = IOutlinerExtension::GetPathName(Item);
		return Sequencer->GetNodeTree()->GetNodeAtPath(NodePath) != nullptr;
	}
	return false;
}

void FSoloEditorExtension::OnPostSolo()
{
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = WeakOwnerModel.Pin();
	TSharedPtr<FSequencer> Sequencer = EditorViewModel ? EditorViewModel->GetSequencerImpl() : nullptr;
	if (Sequencer)
	{
		Sequencer->RefreshTree();
	}
}

bool FSoloEditorExtension::HasSoloedChildNode(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const
{
	using namespace UE::Sequencer;

	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = WeakOwnerModel.Pin();
	TSharedPtr<FSequencer> Sequencer = EditorViewModel ? EditorViewModel->GetSequencerImpl() : nullptr;

	if (Sequencer)
	{
		TViewModelPtr<IOutlinerExtension> OutlinerItem = InWeakOutlinerExtension.Pin();
		const TArray<FString>& MuteNodes = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetMuteNodes();

		TSharedPtr<FViewModel> Item = OutlinerItem;
		const FString NodePath = IOutlinerExtension::GetPathName(Item);

		for (const TViewModelPtr<IOutlinerExtension>& Child : OutlinerItem.AsModel()->GetDescendantsOfType<IOutlinerExtension>())
		{
			if (Sequencer->GetNodeTree()->IsNodeSolo(Child))
			{
				return true;
			}
		}
	}

	return false;
}

} // namespace UE::Sequencer
