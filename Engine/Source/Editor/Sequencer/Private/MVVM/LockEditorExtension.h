// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/ViewModelTypeID.h"
#include "MVVM/Extensions/DynamicExtensionContainer.h"

namespace UE::Sequencer
{

class FSequencerEditorViewModel;
class IOutlinerExtension;

/**
* Extension that handles the locked state of items.
*/
class FLockEditorExtension : public IDynamicExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(FLockEditorExtension)

	FLockEditorExtension();

	/** Called when the extension is created on a data model. */
	virtual void OnCreated(TSharedRef<FViewModel> InWeakOwner) override;

	/** Returns whether a given item is locked. */
	bool IsNodeLocked(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const;

	/** 
	* Modifies an item and it's child sections to be locked.
	* This operation applies to all selected items if the modified item is selected.
	* @param bInIsLocked - Lock state to set the item to
	* @param InWeakOutlinerExtension - Item to lock
	*/
	void SetNodeLocked(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension, bool bInIsLocked);

	/** Returns whether or not this item has any sections to lock. */
	bool IsNodeLockable(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const;

	/** Returns whether or not a child item of this item is locked. */
	bool HasLockedChildNode(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const;

private:

	/** The sequencer editor we are extending. */
	TWeakPtr<FSequencerEditorViewModel> WeakOwnerModel;
};

} // namespace UE::Sequencer

