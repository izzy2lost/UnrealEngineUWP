// Copyright Epic Games, Inc. All Rights Reserved.

#include "SParameterPickerCombo.h"
#include "SParameterPicker.h"
#include "UncookedOnlyUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

namespace UE::AnimNext::Editor
{

void SParameterPickerCombo::Construct(const FArguments& InArgs)
{
	OnGetParameterNameDelegate = InArgs._OnGetParameterName;
	OnGetParameterTypeDelegate = InArgs._OnGetParameterType;
	OnGetParameterInstanceIdDelegate = InArgs._OnGetParameterInstanceId;

	FOnParameterPicked OnParameterPicked = InArgs._PickerArgs.OnParameterPicked;
	FOnInstanceIdChanged OnInstanceIdChanged = InArgs._PickerArgs.OnInstanceIdChanged;

	PickerArgs = InArgs._PickerArgs;

	PickerArgs.OnParameterPicked = FOnParameterPicked::CreateSPLambda(this, [this, OnParameterPicked](const FParameterBindingReference& InReference)
	{
		FSlateApplication::Get().DismissAllMenus();

		// Forward to the original delegate
		OnParameterPicked.ExecuteIfBound(InReference);

		RequestRefresh();
	});
	PickerArgs.OnInstanceIdChanged = FOnInstanceIdChanged::CreateSPLambda(this, [this, OnInstanceIdChanged](const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId)
	{
		// Forward to the original delegate
		OnInstanceIdChanged.ExecuteIfBound(InInstanceId);

		RequestRefresh();
	});
	if(OnGetParameterInstanceIdDelegate.IsBound())
	{
		PickerArgs.InstanceId = OnGetParameterInstanceIdDelegate.Execute();
	}
	PickerArgs.bFocusSearchWidget = true;

	ChildSlot
	[
		SNew(SComboButton)
		.ToolTipText_Lambda([this]()
		{
			return ParameterNameTooltipText;
		})
		.OnGetMenuContent_Lambda([this]()
		{
			if(OnGetParameterInstanceIdDelegate.IsBound())
			{
				PickerArgs.InstanceId = OnGetParameterInstanceIdDelegate.Execute();
			}

			return
				SNew(SParameterPicker)
				.Args(PickerArgs);
		})
		.ButtonContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(0.0f, 2.0f, 2.0f, 2.0f)
			[
				SNew(SImage)
				.Image_Lambda([this]()
				{
					return Icon;
				})
				.ColorAndOpacity_Lambda([this]()
				{
					return IconColor;
				})
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.Padding(0.0f, 2.0f, 0.0f, 2.0f)
			[
				SNew(STextBlock)
				.TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>( "SmallText" ))
				.Text_Lambda([this]()
				{
					return ParameterNameText;
				})
			]
		]
	];

	RequestRefresh();
}

void SParameterPickerCombo::RequestRefresh()
{
	if(!bRefreshRequested)
	{
		bRefreshRequested = true;
		RegisterActiveTimer(1.0f/60.0f, FWidgetActiveTimerDelegate::CreateLambda([this](double InCurrentTime, float InDeltaTime)
		{
			if(OnGetParameterNameDelegate.IsBound())
			{
				ParameterName = OnGetParameterNameDelegate.Execute();
			}

			if(OnGetParameterTypeDelegate.IsBound())
			{
				ParameterType = OnGetParameterTypeDelegate.Execute();
			}

			if(OnGetParameterInstanceIdDelegate.IsBound())
			{
				ParameterInstanceId = OnGetParameterInstanceIdDelegate.Execute();
			}

			ParameterNameText = UncookedOnly::FUtils::GetParameterDisplayNameText(ParameterName, ParameterInstanceId);
			ParameterNameTooltipText = UncookedOnly::FUtils::GetParameterTooltipText(ParameterName, ParameterInstanceId);
			PinType = UncookedOnly::FUtils::GetPinTypeFromParamType(ParameterType);
			Icon = FBlueprintEditorUtils::GetIconFromPin(PinType, true);
			IconColor = GetDefault<UEdGraphSchema_K2>()->GetPinTypeColor(PinType);

			bRefreshRequested = false;
			return EActiveTimerReturnType::Stop;
		}));
	}
}

}
