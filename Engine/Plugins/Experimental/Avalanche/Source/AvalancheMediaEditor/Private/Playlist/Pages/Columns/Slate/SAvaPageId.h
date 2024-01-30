// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Playlist/AvaPlaylistDefines.h"
#include "Playlist/Pages/PageViews/AvaPageView.h"
#include "Widgets/SCompoundWidget.h"

class SInlineEditableTextBlock;

class SAvaPageId : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SAvaPageId){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FAvaPageViewPtr& InPageView);

	virtual ~SAvaPageId() override;

	void OnRenumberAction(EAvaPageActionState InRenumberAction);
	
	void OnTextCommitted(const FText& InText, ETextCommit::Type InCommitInfo);
	
	bool OnVerifyTextChanged(const FText& InText, FText& OutErrorMessage);

	void OnEnterEditingMode();
	
	void OnExitEditingMode();

	void RenumberPageId(const FText& InText, const FAvaPageViewPtr& InPageView);
	
protected:

	TWeakPtr<IAvaPageView> PageViewWeak;
	
	TSharedPtr<SInlineEditableTextBlock> InlineTextBlock;

	bool bInEditingMode = false;
};
