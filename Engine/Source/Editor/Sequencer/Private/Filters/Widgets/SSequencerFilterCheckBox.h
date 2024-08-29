// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Input/SCheckBox.h"

class SSequencerFilterCheckBox : public SCheckBox
{
public:
	DECLARE_DELEGATE_RetVal_TwoParams(FReply, FOnPointerEvent, const FGeometry& /*InMyGeometry*/, const FPointerEvent& /*InMouseEvent*/);

	void SetOnMouseUp(const FOnPointerEvent& InNewOnMouseUp)
	{
		OnMouseUp = InNewOnMouseUp;
	}

	void SetOnDoubleClick(const FOnPointerEvent& InNewOnDoubleClick)
	{
		OnDoubleClick = InNewOnDoubleClick;
	}

protected:
	//~ Begin SWidget

	virtual FReply OnMouseButtonUp(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override
	{
		FReply Reply = SCheckBox::OnMouseButtonUp(InMyGeometry, InMouseEvent);
		if (OnMouseUp.IsBound())
		{
			Reply = OnMouseUp.Execute(InMyGeometry, InMouseEvent).ReleaseMouseCapture();
		}
		return Reply;
	}

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override
	{
		if (OnDoubleClick.IsBound())
		{
			return OnDoubleClick.Execute(InMyGeometry, InMouseEvent);
		}
		return SCheckBox::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
	}

	//~ End SWidget

	FOnPointerEvent OnDoubleClick;
	FOnPointerEvent OnMouseUp;
};
