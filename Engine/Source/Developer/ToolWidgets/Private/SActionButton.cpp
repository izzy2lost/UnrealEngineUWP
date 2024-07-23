// Copyright Epic Games, Inc. All Rights Reserved.

#include "SActionButton.h"

#include "Styling/StyleColors.h"
#include "ToolWidgetsStyle.h"
#include "ToolWidgetsStylePrivate.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"

namespace UE::ToolWidgets::Private
{
	inline FName ActionButtonTypeToStyleName(const EActionButtonType ActionButtonType)
	{
		static TMap<EActionButtonType, const FName> Lookup = {
			{ EActionButtonType::Default, TEXT("ActionButton") },
			{ EActionButtonType::Simple, TEXT("SimpleComboButton") },
			{ EActionButtonType::Positive, TEXT("PositiveActionButton") },
			{ EActionButtonType::Warning, TEXT("NegativeActionButton") },
			{ EActionButtonType::Error, TEXT("NegativeActionButton") },
		};

		if (const FName* FoundName = Lookup.Find(ActionButtonType))
		{
			return *FoundName;
		}

		return NAME_None;
	}
}

void SActionButton::Construct(const FArguments& InArgs)
{
	// Get overridden or default
	ActionButtonType = InArgs._ActionButtonType.Get(EActionButtonType::Default);

	// If style not explicitly set, derive from ActionButtonType
	if (!InArgs._ActionButtonStyle)
	{
		const FName StyleName = UE::ToolWidgets::Private::ActionButtonTypeToStyleName(ActionButtonType.Get());
		check(StyleName != NAME_None);

		ActionButtonStyle = &UE::ToolWidgets::FToolWidgetsStyle::Get().GetWidgetStyle<FActionButtonStyle>(StyleName);
	}
	else
	{
		ActionButtonStyle = InArgs._ActionButtonStyle;

		// ActionButtonStyle was specified, but ActionButtonType was not, so derive one from the other
		if (!InArgs._ActionButtonType.IsSet())
		{
			ActionButtonType = ActionButtonStyle->GetActionButtonType();
		}
	}

	ButtonStyle = InArgs._ButtonStyle ? InArgs._ButtonStyle : &ActionButtonStyle->ButtonStyle;
	IconButtonStyle = InArgs._IconButtonStyle ? InArgs._IconButtonStyle : &ActionButtonStyle->GetIconButtonStyle();
	ComboButtonStyle = InArgs._ComboButtonStyle ? InArgs._ComboButtonStyle : &ActionButtonStyle->ComboButtonStyle;
	TextBlockStyle = InArgs._TextBlockStyle ? InArgs._TextBlockStyle : &ActionButtonStyle->TextBlockStyle;

	const EHorizontalAlignment HorizontalContentAlignment = InArgs._HorizontalContentAlignment.IsSet()
		? InArgs._HorizontalContentAlignment.Get(EHorizontalAlignment::HAlign_Center)
		: static_cast<EHorizontalAlignment>(ActionButtonStyle->HorizontalContentAlignment);

	const bool bHasDownArrow = InArgs._HasDownArrow.IsSet() ? InArgs._HasDownArrow.Get(false) : ActionButtonStyle->bHasDownArrow;

	// Check for widget level override, then style override, otherwise unset
	const TAttribute<const FSlateBrush*> Icon = InArgs._Icon.IsSet()
		? InArgs._Icon.Get()
		: ActionButtonStyle->IconBrush.IsSet()
		? &ActionButtonStyle->IconBrush.GetValue()
		: nullptr;

	const bool bHasIcon = Icon.Get() || Icon.IsBound();

	// Check for widget level override, then style override, otherwise get from ActionButtonType
	const TAttribute<FSlateColor> IconColorAndOpacity = InArgs._IconColorAndOpacity.IsSet()
		? InArgs._IconColorAndOpacity.Get()
		: ActionButtonStyle->IconColorAndOpacity.IsSet()
		? TAttribute<FSlateColor>(ActionButtonStyle->IconColorAndOpacity.GetValue())
		: TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SActionButton::GetIconColorAndOpacity));

	TAttribute<FText> Text = InArgs._Text;

	static constexpr float DefaultIconHeight = UE::ToolWidgets::Private::FToolWidgetsStylePrivate::FActionButton::DefaultIconHeight;
	static constexpr float IconTextPadding = UE::ToolWidgets::Private::FToolWidgetsStylePrivate::FActionButton::DefaultIconLabelSpacing;

	const TSharedRef<SHorizontalBox> ButtonContentContainer = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(0)
		[
			SNew(SWidgetSwitcher)
			.WidgetIndex(bHasIcon ? 1 : 0)

			+ SWidgetSwitcher::Slot()
			[
				SNew(SSpacer)
				.Size(FVector2D{ 0, DefaultIconHeight })
			]

			+ SWidgetSwitcher::Slot()
			[
				SNew(SImage)
				.Image(Icon)
				.ColorAndOpacity(IconColorAndOpacity)
				.Visibility(bHasIcon ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
			]
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.FillWidth(1.0f)
		.Padding(FMargin(bHasIcon ? IconTextPadding : 0, 0, 0, 0))
		[
			SNew(STextBlock)
			.TextStyle(TextBlockStyle)
			.Text(InArgs._Text)
			.Visibility_Lambda([Text]()
			{
				return Text.Get(FText::GetEmpty()).IsEmpty()
					? EVisibility::Collapsed
					: EVisibility::Visible;
			})
		];

	// Treated as a ComboButton if OnClicked is not bound
	const bool bIsComboButton = !InArgs._OnClicked.IsBound();

	if (bIsComboButton)
	{
		static const FMargin DefaultComboButtonContentPadding = FMargin(
			UE::ToolWidgets::Private::FToolWidgetsStylePrivate::FActionButton::DefaultHorizontalPadding,
			UE::ToolWidgets::Private::FToolWidgetsStylePrivate::FActionButton::DefaultVerticalPadding);

		const TAttribute<FMargin> ComboButtonContentPadding = InArgs._ButtonContentPadding.IsSet()
			? InArgs._ButtonContentPadding.Get(DefaultComboButtonContentPadding)
			: ActionButtonStyle->ComboButtonContentPadding.IsSet()
			? ActionButtonStyle->ComboButtonContentPadding.GetValue()
			: ActionButtonStyle->ComboButtonStyle.ContentPadding;

		ChildSlot
		[
			SAssignNew(ComboButton, SComboButton)
			.HasDownArrow(bHasDownArrow)
			.ContentPadding(ComboButtonContentPadding)
			.ButtonStyle(bHasIcon ? IconButtonStyle : ButtonStyle)
			.ComboButtonStyle(ComboButtonStyle)
			.IsEnabled(InArgs._IsEnabled)
			.ToolTipText(InArgs._ToolTipText)
			.HAlign(HorizontalContentAlignment)
			.VAlign(VAlign_Center)
			.ButtonContent()
			[
				ButtonContentContainer
			]
			.MenuContent()
			[
				InArgs._MenuContent.Widget
			]
			.OnGetMenuContent(InArgs._OnGetMenuContent)
			.OnMenuOpenChanged(InArgs._OnMenuOpenChanged)
			.OnComboBoxOpened(InArgs._OnComboBoxOpened)
		];
	}
	else
	{
		static const FMargin DefaultButtonContentPadding = FMargin(
			UE::ToolWidgets::Private::FToolWidgetsStylePrivate::FActionButton::DefaultHorizontalPadding,
			UE::ToolWidgets::Private::FToolWidgetsStylePrivate::FActionButton::DefaultVerticalPadding);

		const TAttribute<FMargin> ButtonContentPadding = InArgs._ButtonContentPadding.IsSet()
			? InArgs._ButtonContentPadding.Get(DefaultButtonContentPadding)
			: ActionButtonStyle->ButtonContentPadding.IsSet()
			? ActionButtonStyle->ButtonContentPadding.GetValue()
			: DefaultButtonContentPadding;

		ChildSlot
		[
			SAssignNew(Button, SButton)
			.ContentPadding(ButtonContentPadding)
			.ButtonStyle(bHasIcon ? IconButtonStyle : ButtonStyle)
			.IsEnabled(InArgs._IsEnabled)
			.ToolTipText(InArgs._ToolTipText)
			.HAlign(HorizontalContentAlignment)
			.VAlign(VAlign_Center)
			.OnClicked(InArgs._OnClicked)
			[
				ButtonContentContainer
			]
		];
	}
}

void SActionButton::SetMenuContentWidgetToFocus(TWeakPtr<SWidget> InWidget)
{
	check(ComboButton.IsValid());
	ComboButton->SetMenuContentWidgetToFocus(InWidget);
}

void SActionButton::SetIsMenuOpen(bool bInIsOpen, bool bInIsFocused)
{
	check(ComboButton.IsValid());
	ComboButton->SetIsOpen(bInIsOpen, bInIsFocused);
}

FSlateColor SActionButton::GetIconColorAndOpacity() const
{
	switch (ActionButtonType.Get(EActionButtonType::Default))
	{
	case EActionButtonType::Default:
	case EActionButtonType::Simple:
	default:
		return FSlateColor::UseForeground();

	case EActionButtonType::Positive:
		return FStyleColors::AccentGreen;

	case EActionButtonType::Warning:
		return FStyleColors::Warning;

	case EActionButtonType::Error:
		return FStyleColors::Error;
	}
}
