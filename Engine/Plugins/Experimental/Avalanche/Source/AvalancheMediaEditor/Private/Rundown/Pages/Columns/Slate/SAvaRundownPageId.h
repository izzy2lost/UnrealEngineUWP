// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rundown/AvaRundownDefines.h"
#include "Rundown/Pages/PageViews/IAvaRundownPageView.h"
#include "Widgets/SCompoundWidget.h"

class SInlineEditableTextBlock;

class SAvaRundownPageId : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SAvaRundownPageId){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FAvaRundownPageViewPtr& InPageView);

	virtual ~SAvaRundownPageId() override;

	void OnRenumberAction(EAvaRundownPageActionState InRenumberAction);
	
	void OnTextCommitted(const FText& InText, ETextCommit::Type InCommitInfo);
	
	bool OnVerifyTextChanged(const FText& InText, FText& OutErrorMessage);

	void OnEnterEditingMode();
	
	void OnExitEditingMode();

	void RenumberPageId(const FText& InText, const FAvaRundownPageViewPtr& InPageView);
	
protected:

	TWeakPtr<IAvaRundownPageView> PageViewWeak;
	
	TSharedPtr<SInlineEditableTextBlock> InlineTextBlock;

	bool bInEditingMode = false;
};
