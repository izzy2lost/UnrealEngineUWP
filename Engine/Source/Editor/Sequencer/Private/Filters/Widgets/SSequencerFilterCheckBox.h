// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Application/SlateApplication.h"
#include "GameFramework/InputSettings.h"
#include "Widgets/Input/SCheckBox.h"

class SSequencerFilterCheckBox : public SCheckBox
{
public:
	void SetOnClick(const FOnCheckStateChanged& InOnClick)
	{
		OnClick = InOnClick;
	}

	void SetOnCtrlClick(const FSimpleDelegate& InNewCtrlClick)
	{
		OnCtrlClick = InNewCtrlClick;
	}

	void SetOnAltClick(const FSimpleDelegate& InNewAltClick)
	{
		OnAltClick = InNewAltClick;
	}

	void SetOnMiddleButtonClick(const FSimpleDelegate& InNewMiddleButtonClick)
	{
		OnMiddleButtonClick = InNewMiddleButtonClick;
	}

	void SetOnDoubleClick(const FSimpleDelegate& InNewDoubleClick)
	{
		OnDoubleClick = InNewDoubleClick;
	}

protected:
	//~ Begin SWidget

	virtual FReply OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent) override
	{
		if (InPointerEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			bIsPressed = true;

			const EButtonClickMethod::Type InputClickMethod = GetClickMethodFromInputType(InPointerEvent);

			if (InputClickMethod == EButtonClickMethod::MouseDown)
			{
				Internal_Click(InPointerEvent);

				return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse);
			}

			return FReply::Handled().CaptureMouse(AsShared()).SetUserFocus(AsShared(), EFocusCause::Mouse);
		}

		if (InPointerEvent.GetEffectingButton() == EKeys::RightMouseButton && OnGetMenuContent.IsBound())
		{
			FSlateApplication::Get().PushMenu(
				AsShared(),
				InPointerEvent.GetEventPath() ? *InPointerEvent.GetEventPath() : FWidgetPath(),
				OnGetMenuContent.Execute(),
				InPointerEvent.GetScreenSpacePosition(),
				FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

			return FReply::Handled();
		}

		return FReply::Unhandled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent) override
	{
		const EButtonClickMethod::Type InputClickMethod = GetClickMethodFromInputType(InPointerEvent);
		const bool bMustBePressed = InputClickMethod == EButtonClickMethod::DownAndUp || InputClickMethod == EButtonClickMethod::PreciseClick;
		const bool bMeetsPressedRequirements = !bMustBePressed || (bIsPressed && bMustBePressed);

		if (bMeetsPressedRequirements && ((InPointerEvent.GetEffectingButton() == EKeys::LeftMouseButton || InPointerEvent.IsTouchEvent())))
		{
			bIsPressed = false;

			if (InputClickMethod != EButtonClickMethod::MouseDown)
			{
				const bool IsUnderMouse = InGeometry.IsUnderLocation(InPointerEvent.GetScreenSpacePosition());
				if (IsUnderMouse)
				{
					// If we were asked to allow the button to be clicked on mouse up, regardless of whether the user
					// pressed the button down first, then we'll allow the click to proceed without an active capture
					if (InputClickMethod == EButtonClickMethod::MouseUp || HasMouseCapture())
					{
						Internal_Click(InPointerEvent);
					}
				}
			}

			return FReply::Handled().ReleaseMouseCapture();
		}

		return FReply::Unhandled();
	}

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent) override
	{
		OnDoubleClick.ExecuteIfBound();

		return FReply::Handled();
	}

	//~ End SWidget

	void Internal_Click(const FPointerEvent& InPointerEvent)
	{
		// Use a timer for click to give double click a chance to register
		TimerHandle = RegisterActiveTimer(GetDefault<UInputSettings>()->DoubleClickTime,
			FWidgetActiveTimerDelegate::CreateLambda([this, InPointerEvent](double InCurrentTime, float InDeltaTime) -> EActiveTimerReturnType
			{
				if (InPointerEvent.IsControlDown())
				{
					OnCtrlClick.ExecuteIfBound();
				}
				else if (InPointerEvent.IsAltDown())
				{
					OnAltClick.ExecuteIfBound();
				}
				else if (InPointerEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
				{
					OnMiddleButtonClick.ExecuteIfBound();
				}
				else
				{
					OnClick.ExecuteIfBound(GetCheckedState());
				}

				return EActiveTimerReturnType::Stop;
			}));
	}

	FOnCheckStateChanged OnClick;
	FSimpleDelegate OnCtrlClick;
	FSimpleDelegate OnAltClick;
	FSimpleDelegate OnDoubleClick;
	FSimpleDelegate OnMiddleButtonClick;

	TSharedPtr<FActiveTimerHandle> TimerHandle;
};
