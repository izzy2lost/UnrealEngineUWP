// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/LockEditorExtension.h"

#include "MovieScene.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/Selection/Selection.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "ScopedTransaction.h"
#include "Sequencer.h"
#include "SequencerCommonHelpers.h"

namespace UE::Sequencer
{

FLockEditorExtension::FLockEditorExtension()
{

}

void FLockEditorExtension::OnCreated(TSharedRef<FViewModel> InWeakOwner)
{
	ensureMsgf(!WeakOwnerModel.Pin().IsValid(), TEXT("This extension was already created!"));
	WeakOwnerModel = InWeakOwner->CastThisShared<FSequencerEditorViewModel>();
}

bool FLockEditorExtension::IsNodeLocked(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const
{
	FViewModelPtr Item = InWeakOutlinerExtension.Pin();

	// Locked if all sections are locked
	int32 NumSections = 0;
	TSet<TWeakObjectPtr<UMovieSceneSection>> Sections;
	SequencerHelpers::GetAllSections(Item, Sections);

	for (auto Section : Sections)
	{
		if (!Section->IsLocked())
		{
			return false;
		}
		++NumSections;
	}
	return NumSections > 0;
}

void FLockEditorExtension::SetNodeLocked(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension, bool bInIsLocked)
{
	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "SetNodeLocked", "Set Node Locked"));

	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = WeakOwnerModel.Pin();
	TViewModelPtr<IOutlinerExtension> OutlinerItem = InWeakOutlinerExtension.Pin();
	FViewModelPtr Item = OutlinerItem;

	if (OutlinerItem->GetSelectionState() == EOutlinerSelectionState::SelectedDirectly)
	{
		// modify all selected items
		for (FViewModelPtr OutlinerNode : EditorViewModel->GetSelection()->Outliner)
		{
			TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
			SequencerHelpers::GetAllSections(OutlinerNode, Sections);

			for (auto Section : Sections)
			{
				Section->Modify();
				Section->SetIsLocked(bInIsLocked);
			}
		}
	}
	else
	{
		// only one unselected item was toggled, toggle just that node
		TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
		SequencerHelpers::GetAllSections(Item, Sections);

		for (auto Section : Sections)
		{
			Section->Modify();
			Section->SetIsLocked(bInIsLocked);
		}
	}
}

bool FLockEditorExtension::IsNodeLockable(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const
{
	FViewModelPtr Item = InWeakOutlinerExtension.Pin();

	TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
	SequencerHelpers::GetAllSections(Item, Sections);

	return Sections.Num() > 0;
}

bool FLockEditorExtension::HasLockedChildNode(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const
{
	TViewModelPtr<IOutlinerExtension> OutlinerItem = InWeakOutlinerExtension.Pin();

	for (const TViewModelPtr<IOutlinerExtension>& Child : OutlinerItem.AsModel()->GetDescendantsOfType<IOutlinerExtension>())
	{
		if (IsNodeLocked(Child))
		{
			return true;
		}
	}

	return false;
}

} // namespace UE::Sequencer

