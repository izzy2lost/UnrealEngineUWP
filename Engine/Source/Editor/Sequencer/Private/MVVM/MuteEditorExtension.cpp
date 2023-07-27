// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/MuteEditorExtension.h"

#include "MovieScene.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/Extensions/IMutableExtension.h"
#include "MVVM/Selection/Selection.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "ScopedTransaction.h"
#include "Sequencer.h"

namespace UE::Sequencer
{

FMuteEditorExtension::FMuteEditorExtension()
{

}

void FMuteEditorExtension::OnCreated(TSharedRef<FViewModel> InWeakOwner)
{
	ensureMsgf(!WeakOwnerModel.Pin().IsValid(), TEXT("This extension was already created!"));
	WeakOwnerModel = InWeakOwner->CastThisShared<FSequencerEditorViewModel>();
}

bool FMuteEditorExtension::IsNodeMuted(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const
{
	TViewModelPtr<IOutlinerExtension> Item = InWeakOutlinerExtension.Pin();
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = WeakOwnerModel.Pin();
	TSharedPtr<FSequencer> Sequencer = EditorViewModel ? EditorViewModel->GetSequencerImpl() : nullptr;
	if(Item && Sequencer)
	{
		const TArray<FString>& MuteNodes = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetMuteNodes();
		const FString NodePath = IOutlinerExtension::GetPathName(Item);

		if (MuteNodes.Contains(NodePath))
		{
			return true;
		}
	}

	return false;
}

void FMuteEditorExtension::SetNodeMuted(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension, bool bInIsMuted)
{              
	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "SetNodeMuted", "Set Node Muted"));

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

		TArray<FString>& MuteNodes = MovieScene->GetMuteNodes();

		MovieScene->Modify();

		if (OutlinerItem->GetSelectionState() == EOutlinerSelectionState::SelectedDirectly)
		{
			// if selected, modify all selected items
			for (FViewModelPtr Node : Sequencer->GetViewModel()->GetSelection()->Outliner)
			{
				FString NodePath = IOutlinerExtension::GetPathName(Node);
				if (bInIsMuted)
				{
					// Mark Mute, being careful as we might be re-marking an already Mute node
					MuteNodes.AddUnique(NodePath);
				}
				else
				{
					// UnMute
					MuteNodes.Remove(NodePath);
				}
			}
		}
		else
		{
			// modify only the toggled item
			TSharedPtr<FViewModel> Item = OutlinerItem;
			FString NodePath = IOutlinerExtension::GetPathName(Item);
			if (bInIsMuted)
			{
				// Mark Mute, being careful as we might be re-marking an already Mute node
				MuteNodes.AddUnique(NodePath);
			}
			else
			{
				// Unmute the toggled item
				MuteNodes.Remove(NodePath);
			}
		}
	}
}

bool FMuteEditorExtension::IsNodeMutable(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const
{
	TViewModelPtr<IOutlinerExtension> OutlinerItem = InWeakOutlinerExtension.Pin();
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = WeakOwnerModel.Pin();
	TSharedPtr<FSequencer> Sequencer = EditorViewModel ? EditorViewModel->GetSequencerImpl() : nullptr;
		
	if (Sequencer && OutlinerItem)
	{
		TSharedPtr<FViewModel> Item = OutlinerItem;
		FString NodePath = IOutlinerExtension::GetPathName(Item);
		return Sequencer->GetNodeTree()->GetNodeAtPath(NodePath) != nullptr;
	}

	return false;
}

bool FMuteEditorExtension::HasMutedChildNode(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const
{
	using namespace UE::Sequencer;

	TViewModelPtr<IOutlinerExtension> OutlinerItem = InWeakOutlinerExtension.Pin();

	if (OutlinerItem)
	{
		for (const TViewModelPtr<IOutlinerExtension>& Child : OutlinerItem.AsModel()->GetDescendantsOfType<IOutlinerExtension>(false, EViewModelListType::Outliner))
		{
			if (IsNodeMuted(Child))
			{
				return true;
			}
		}
	}

	return false;
}

bool FMuteEditorExtension::IsNodeImplicitlyMuted(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const
{
	TViewModelPtr<IOutlinerExtension> Item = InWeakOutlinerExtension.Pin();
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = WeakOwnerModel.Pin();
	TSharedPtr<FSequencer> Sequencer = EditorViewModel ? EditorViewModel->GetSequencerImpl() : nullptr;
	if (Item && Sequencer)
	{
		const TArray<FString>& MuteNodes = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetMuteNodes();

		TViewModelPtr<IOutlinerExtension> ParentNode = Item.AsModel()->FindAncestorOfType<IOutlinerExtension>();
		while (ParentNode)
		{
			const FString NodePath = IOutlinerExtension::GetPathName(ParentNode);
			if (MuteNodes.Contains(NodePath))
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
