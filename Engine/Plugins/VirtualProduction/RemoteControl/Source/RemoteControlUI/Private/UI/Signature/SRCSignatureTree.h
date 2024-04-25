// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UI/BaseLogicUI/SRCLogicPanelListBase.h"

class FRCSignatureTreeItemBase;
class IRCSignatureColumn;
class SHeaderRow;
class SRCSignaturePanel;
class URemoteControlSignatureRegistry;
template<typename ItemType> class STreeView;

class SRCSignatureTree : public SRCLogicPanelListBase
{
public:
	SLATE_BEGIN_ARGS(SRCSignatureTree) {}
		SLATE_ARGUMENT(TArray<TSharedRef<IRCSignatureColumn>>, Columns)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<SRCSignaturePanel>& InSignaturePanel, const TSharedRef<SRemoteControlPanel>& InRCPanel);

	URemoteControlSignatureRegistry* GetSignatureRegistry() const;

	TSharedPtr<IRCSignatureColumn> FindColumn(FName InColumnName) const;

	void Refresh();

	//~ Begin SRCLogicPanelListBase
	virtual URemoteControlPreset* GetPreset() override;
	virtual TArray<TSharedPtr<FRCLogicModeBase>> GetSelectedLogicItems() override;
	virtual bool IsEmpty() const override;
	virtual bool IsListFocused() const override;
	virtual int32 Num() const override;
	virtual int32 NumSelectedLogicItems() const override;
	virtual int32 RemoveModel(const TSharedPtr<FRCLogicModeBase> InItem) override;
	virtual void DeleteSelectedPanelItems() override;
	virtual void BroadcastOnItemRemoved() override {}
	virtual void Reset() override;
	//~ End SRCLogicPanelListBase

private:
	void ConstructColumns(TConstArrayView<TSharedRef<IRCSignatureColumn>> InColumns);

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FRCSignatureTreeItemBase> InItem, const TSharedRef<STableViewBase>& InTableView);

	void OnGetChildren(TSharedPtr<FRCSignatureTreeItemBase> InItem, TArray<TSharedPtr<FRCSignatureTreeItemBase>>& OutChildren) const;

	/** All Known Signatures in this Tree View */
	TArray<TSharedPtr<FRCSignatureTreeItemBase>> SignatureItems;

	TMap<FName, TSharedRef<IRCSignatureColumn>> Columns;

	TSharedPtr<STreeView<TSharedPtr<FRCSignatureTreeItemBase>>> SignatureTreeView;

	TSharedPtr<SHeaderRow> HeaderRow;

	TWeakPtr<SRCSignaturePanel> SignaturePanelWeak;
};
