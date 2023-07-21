// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/PinEditorExtension.h"

#include "MovieScene.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/Selection/Selection.h"
#include "MVVM/ViewModels/OutlinerViewModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "ScopedTransaction.h"
#include "Sequencer.h"

namespace UE::Sequencer
{

FPinEditorExtension::FPinEditorExtension()
{

}

void FPinEditorExtension::OnCreated(TSharedRef<FViewModel> InWeakOwner)
{
	ensureMsgf(!WeakOwnerModel.Pin().IsValid(), TEXT("This extension was already created!"));
	WeakOwnerModel = InWeakOwner->CastThisShared<FSequencerEditorViewModel>();
}

bool FPinEditorExtension::IsNodePinnable(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const
{
	TSharedPtr<FViewModel> Item = InWeakOutlinerExtension.Pin();
	return Item->GetHierarchicalDepth() == 1;
}

void FPinEditorExtension::SetNodePinned(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension, bool bInIsPinned)
{
	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "SetNodePinned", "Set Node Pinned"));

	TSharedPtr<FViewModel> Item = InWeakOutlinerExtension.Pin();
	FSequenceModel* RootModel = Item->GetRoot()->CastThis<FSequenceModel>();
	TSharedPtr<IPinnableExtension> PinnableParent = Item->FindAncestorOfType<IPinnableExtension>(true);

	if (RootModel && PinnableParent)
	{
		TSharedPtr<FOutlinerViewModel> Outliner = WeakOwnerModel.Pin()->GetOutliner();
		Outliner->UnpinAllNodes();

		PinnableParent->SetPinned(bInIsPinned);

		TSharedPtr<FSequencer> Sequencer = RootModel->GetSequencerImpl();
		if (Sequencer)
		{
			UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
			FMovieSceneEditorData& EditorData = MovieScene->GetEditorData();

			if (bInIsPinned)
			{
				EditorData.PinnedNodes.AddUnique(IOutlinerExtension::GetPathName(Item));
			}
			else
			{
				EditorData.PinnedNodes.RemoveSingle(IOutlinerExtension::GetPathName(Item));
			}

			Sequencer->RefreshTree();
		}
	}
}

} // namespace UE::Sequencer

