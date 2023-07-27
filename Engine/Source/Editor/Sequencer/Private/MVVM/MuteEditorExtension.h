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
* Extension that handles the mute state of items.
*/
class FMuteEditorExtension : public IDynamicExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(FMuteEditorExtension)

	FMuteEditorExtension();

	/** Called when the extension is created on a data model. */
	virtual void OnCreated(TSharedRef<FViewModel> InWeakOwner) override;

	/** 
	* Returns whether a given item is directly muted. 
	* Returns false if not directly muted but implicitly muted by a parent node.
	*/
	bool IsNodeMuted(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const;


	/**
	* Modifies an item to be muted.
	* This operation applies to all selected items if the modified item is selected.
	* @param bInIsMuted - Muted state to set the item to
	* @param InWeakOutlinerExtension - Item to mute
	*/
	void SetNodeMuted(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension, bool bInIsMuted);

	/** Returns whether or not the item has a valid node path to mute. */
	bool IsNodeMutable(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const;

	/** Returns whether or not the item has any muted child items. */
	bool HasMutedChildNode(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const;

	/**
	 * Returns whether a given item is implicitly muted.
	 * Returns false if not implicitly muted by a parent node but directly muted itself.
	 */
	bool IsNodeImplicitlyMuted(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const;

private:

	/** The sequencer editor we are extending */
	TWeakPtr<FSequencerEditorViewModel> WeakOwnerModel;
};

} // namespace UE::Sequencer

