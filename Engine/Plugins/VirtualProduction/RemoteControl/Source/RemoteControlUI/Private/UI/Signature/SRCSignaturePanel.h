// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UI/BaseLogicUI/SRCLogicPanelBase.h"

class IRCSignatureColumn;
class URemoteControlSignatureRegistry;
class SRCSignatureTree;

class SRCSignaturePanel : public SRCLogicPanelBase
{
public:
	SLATE_BEGIN_ARGS(SRCSignaturePanel) {}
		SLATE_ATTRIBUTE(bool, LiveMode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<SRemoteControlPanel>& InPanel);

	/** Gets the Signature Registry from the RC Preset */
	URemoteControlSignatureRegistry* GetSignatureRegistry() const;

	/** Whether this widget currently has focus */
	bool IsListFocused() const;

	//~ Begin SRCLogicPanelBase
	virtual TArray<TSharedPtr<FRCLogicModeBase>> GetSelectedLogicItems() const override;
	virtual FReply RequestDeleteSelectedItem() override;
	virtual FReply RequestDeleteAllItems() override;
	virtual bool CanCopyItems() const override;
	virtual bool CanDuplicateItems() const override;
	virtual void DeleteSelectedPanelItems() override;
	//~ End SRCLogicPanelBase

private:
	FReply OnAddButtonClicked();

	FReply DeleteAllItems();

	TSharedPtr<SRCSignatureTree> SignatureTreeView;
};
