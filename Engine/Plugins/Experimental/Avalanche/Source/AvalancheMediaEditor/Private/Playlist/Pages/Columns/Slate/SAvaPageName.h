// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Playlist/AvaPlaylistDefines.h"
#include "Widgets/SCompoundWidget.h"

class IAvaPageView;
class SAvaPageViewRow;
class SInlineEditableTextBlock;
enum class EAvaPageActionState : uint8;

class SAvaPageName : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SAvaPageName){}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const FAvaPageViewPtr& InPageView, const TSharedPtr<SAvaPageViewRow>& InRow);
	virtual ~SAvaPageName() override;

	void OnRenameAction(EAvaPageActionState InRenameAction);
	
	void OnTextCommitted(const FText& InText, ETextCommit::Type InCommitInfo);
	bool OnVerifyTextChanged(const FText& InText, FText& OutErrorMessage);

	void OnEnterEditingMode();
	void OnExitEditingMode();

	void RenamePage(const FText& InText, const FAvaPageViewPtr& InPageView);

	bool IsReadOnly() const;
	
protected:

	TWeakPtr<IAvaPageView> PageViewWeak;
	
	TWeakPtr<SAvaPageViewRow> PageViewRowWeak;
	
	TSharedPtr<SInlineEditableTextBlock> InlineTextBlock;

	bool bInEditingMode = false;
	bool bIsReadOnly = true;
};
