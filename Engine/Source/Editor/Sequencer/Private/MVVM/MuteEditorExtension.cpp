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

void FMuteEditorExtension::SetNodeMuted(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension, bool bInIsMuted)
{              
	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "SetNodeMuted", "Set Node Muted"));

	TViewModelPtr<IOutlinerExtension> OutlinerItem = InWeakOutlinerExtension.Pin();
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = WeakOwnerModel.Pin();
	TSharedPtr<FSequencer> Sequencer = EditorViewModel ? EditorViewModel->GetSequencerImpl() : nullptr;
		
	if (Sequencer)
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
		
	if (Sequencer)
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

	for (const TViewModelPtr<IMutableExtension>& Child : OutlinerItem.AsModel()->GetDescendantsOfType<IMutableExtension>())
	{
		if (Child->IsMuted())
		{
			return true;
		}
	}

	return false;
}

void FMuteEditorExtension::OnPostMute()
{
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = WeakOwnerModel.Pin();
	TSharedPtr<FSequencer> Sequencer = EditorViewModel ? EditorViewModel->GetSequencerImpl() : nullptr;
	if (Sequencer)
	{
		Sequencer->RefreshTree();
	}
}

} // namespace UE::Sequencer
