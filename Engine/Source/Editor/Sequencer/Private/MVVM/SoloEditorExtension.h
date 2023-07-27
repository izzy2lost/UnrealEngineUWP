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
* Extension that handles the solo state of items.
*/
class FSoloEditorExtension : public IDynamicExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(FSoloEditorExtension)

	FSoloEditorExtension();

	/** Called when the extension is created on a data model. */
	virtual void OnCreated(TSharedRef<FViewModel> InWeakOwner) override;

	/**
	* Returns whether a given item is directly soloed.
	* Returns false if not directly soloed but implicitly soloed by a parent node.
	*/
	bool IsNodeSoloed(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const;

	/**
	* Modifies an item and it's child sections to be soloed.
	* This operation applies to all selected items if the modified item is selected.
	* @param bInIsSoloed - Solo state to set the item to
	* @param InWeakOutlinerExtension - Item to solo
	*/
	void SetNodeSoloed(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension, bool bInIsSoloed);

	/** Returns whether or not the item has a valid node path to solo. */
	bool IsNodeSoloable(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const;

	/** Returns whether or not the item has any soloed child items. */
	bool HasSoloedChildNode(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const;

	/**
	 * Returns whether a given item is implicitly soloed.
	 * Returns false if not implicitly soloed by a parent node but directly soloed itself.
	 */
	bool IsNodeImplicitlySoloed(TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension) const;

private:

	/** The sequencer editor we are extending */
	TWeakPtr<FSequencerEditorViewModel> WeakOwnerModel;
};

} // namespace UE::Sequencer

