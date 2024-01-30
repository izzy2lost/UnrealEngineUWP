// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/IDelegateInstance.h"
#include "Styling/SlateStyle.h"

struct FActorModifierCoreMetadata;

class FAvaModifiersEditorStyle : public FSlateStyleSet
{
public:
	FAvaModifiersEditorStyle();

	virtual ~FAvaModifiersEditorStyle() override;

	static FAvaModifiersEditorStyle& Get()
	{
		static FAvaModifiersEditorStyle StyleSet;
		return StyleSet;
	}

	const FSlateColor& GetModifierCategoryColor(FName CategoryName);

private:
	void OnModifierClassRegistered(const FActorModifierCoreMetadata& InMetadata);

	TMap<FName, FSlateColor> ModifierCategoriesColors;
};
