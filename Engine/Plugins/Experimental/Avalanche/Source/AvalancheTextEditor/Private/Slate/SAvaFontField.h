// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class FAvaFontView;
class SAvaFontSelector;
class STextBlock;
enum class ECheckBoxState : uint8;

/**
 * Represents a font selection field 
 */
DECLARE_DELEGATE_OneParam(FOnAvaFontFieldModified, const TSharedPtr<FAvaFontView>&)

class SAvaFontField : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvaFontField)
		:_AvalancheFont(),
		_OnAvaFontFieldModified()
		{
		}

		SLATE_ATTRIBUTE(TSharedPtr<FAvaFontView>, AvalancheFont)
		SLATE_EVENT(FOnAvaFontFieldModified, OnAvaFontFieldModified)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void UpdateFont(const TSharedPtr<FAvaFontView>& InAvaFont);
	void Select() { bIsSelected = true; }
	void Deselect() { bIsSelected = false; }

protected:
	ECheckBoxState GetFavoriteState() const;
	FSlateColor GetToggleFavoriteColor() const;
	FReply OnToggleFavoriteClicked();
	EVisibility GetFavoriteVisibility() const;
	EVisibility GetLocallyAvailableIconVisibility() const;

	bool IsSelected() const { return bIsSelected; }

	FOnAvaFontFieldModified OnAvaFontFieldModified;

	FText FontName;

	TSharedPtr<STextBlock> LeftFontNameText;
	TSharedPtr<STextBlock> RightFontNameText;

	TAttribute<TSharedPtr<FAvaFontView>> AvalancheFont;

	TSharedPtr<FSlateBrush> CheckBoxBg;

	bool bIsSelected = false;
};
