// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyle.h"

struct FLinearColor;
struct FSlateBrush;

class FAvaOutlinerStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static FName GetStyleSetName();
	static const ISlateStyle& Get();

	static const FLinearColor& GetColor(FName InPropertyName, const ANSICHAR* InSpecifier = nullptr);
	static const FSlateBrush* GetBrush(FName InPropertyName, const ANSICHAR* InSpecifier = nullptr);

	template<typename InWidgetStyleType>
	static const InWidgetStyleType& GetWidgetStyle(FName InPropertyName, const ANSICHAR* InSpecifier = nullptr)
	{
		return StyleInstance->GetWidgetStyle<InWidgetStyleType>(InPropertyName, InSpecifier);
	}

private:
	static TSharedRef<FSlateStyleSet> Create();
	static TSharedPtr<FSlateStyleSet> StyleInstance;
};
