// Copyright Epic Games, Inc. All Rights Reserved.

#include "WidgetFocusUtils.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"

FPendingWidgetFocus::~FPendingWidgetFocus()
{
	PendingFocusFunction.Reset();
	if (PreInputKeyDownHandle.IsValid())
	{
		FSlateApplication& SlateApplication = FSlateApplication::Get();
		SlateApplication.OnApplicationPreInputKeyDownListener().Remove(PreInputKeyDownHandle);
		PreInputKeyDownHandle.Reset();
	}
}

void FPendingWidgetFocus::SetPendingFocusIfNeeded(const TWeakPtr<SWidget>& InWidget)
{
	if (!PreInputKeyDownHandle.IsValid())
	{
		return;
	}

	PendingFocusFunction = [WidgetFocus = InWidget]()
	{
		if (WidgetFocus.IsValid())
		{
			TSharedPtr<SWidget> Widget = WidgetFocus.Pin();
			FSlateApplication::Get().ForEachUser([&Widget](FSlateUser& User) 
			{
				User.SetFocus(Widget.ToSharedRef());
			});
		}
	};
}

void FPendingWidgetFocus::ResetPendingFocus()
{
	PendingFocusFunction.Reset();
}

void FPendingWidgetFocus::Enable(const bool InEnabled)
{
	FSlateApplication& SlateApplication = FSlateApplication::Get();
	if (PreInputKeyDownHandle.IsValid())
	{
		SlateApplication.OnApplicationPreInputKeyDownListener().Remove(PreInputKeyDownHandle);
		PreInputKeyDownHandle.Reset();
	}
	
	PendingFocusFunction.Reset();
	
	if (InEnabled)
	{
		PreInputKeyDownHandle = SlateApplication.OnApplicationPreInputKeyDownListener().AddRaw(this, &FPendingWidgetFocus::OnPreInputKeyDown);
	}
}

bool FPendingWidgetFocus::IsEnabled() const
{
	return PreInputKeyDownHandle.IsValid();
}
	
void FPendingWidgetFocus::OnPreInputKeyDown(const FKeyEvent&)
{
	if (PendingFocusFunction)
	{
		PendingFocusFunction();
		PendingFocusFunction.Reset();
	}
}
