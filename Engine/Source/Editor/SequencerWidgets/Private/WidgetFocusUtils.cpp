// Copyright Epic Games, Inc. All Rights Reserved.

#include "WidgetFocusUtils.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"

FPendingWidgetFocus::FPendingWidgetFocus(const TArray<FName>& InTypesKeepingFocus)
	: KeepingFocus(InTypesKeepingFocus)
{}

FPendingWidgetFocus FPendingWidgetFocus::MakeNoTextEdit()
{
	static TArray<FName> EditableTextTypes({"SEditableText"});
	// NOTE: "SMultiLineEditableText" might be added as well
	
	static FPendingWidgetFocus NewPendingFocus(EditableTextTypes);
	return NewPendingFocus;
}

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

	if (!CanFocusBeStolen())
	{
		PendingFocusFunction.Reset();
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

bool FPendingWidgetFocus::CanFocusBeStolen() const
{
	if (!KeepingFocus.IsEmpty())
	{
		bool bShouldCurrentFocusBeKept = false;
		FSlateApplication::Get().ForEachUser([this, &bShouldCurrentFocusBeKept](const FSlateUser& User)
		{
			if (TSharedPtr<SWidget> FocusedWidget = User.GetFocusedWidget())
			{
				bShouldCurrentFocusBeKept = KeepingFocus.Contains(FocusedWidget->GetType());
			}
		});
		return !bShouldCurrentFocusBeKept;
	}
	return true;
}

