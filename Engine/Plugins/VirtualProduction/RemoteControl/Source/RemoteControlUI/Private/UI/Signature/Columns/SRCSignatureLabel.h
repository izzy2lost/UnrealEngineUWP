// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class FRCSignatureTreeItemBase;
class SRCSignatureRow;
enum class ECheckBoxState : uint8;

class SRCSignatureLabel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRCSignatureLabel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs
		, const TSharedRef<FRCSignatureTreeItemBase>& InItem
		, const TSharedRef<SRCSignatureRow>& InRow);

private:
	ECheckBoxState GetItemEnabledState() const;

	void SetItemEnabledState(ECheckBoxState InState);

	FText GetSignatureDisplayName() const;

	void OnSignatureDisplayNameCommitted(const FText& InText, ETextCommit::Type InCommitType);

	TWeakPtr<FRCSignatureTreeItemBase> ItemWeak;

	mutable TOptional<ECheckBoxState> CachedCheckBoxState;

	mutable TOptional<FText> CachedDisplayName;
};
