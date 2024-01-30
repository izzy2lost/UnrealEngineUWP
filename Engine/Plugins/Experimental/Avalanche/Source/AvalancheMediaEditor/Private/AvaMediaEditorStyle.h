// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyle.h"

struct FSlateBrush;
class UTexture2D;

class FAvaMediaEditorStyle
{
public:

	static void Initialize();

	static void Shutdown();
	
	static const ISlateStyle& Get();

	static FName GetStyleSetName();

	static const FLinearColor& GetColor(FName PropertyName, const ANSICHAR* Specifier = nullptr);

	static const FSlateBrush* GetBrush(FName PropertyName, const ANSICHAR* Specifier = nullptr);

	template< typename WidgetStyleType >
	static const WidgetStyleType& GetWidgetStyle(FName PropertyName, const ANSICHAR* Specifier = nullptr)
	{
		return StyleInstance->GetWidgetStyle<WidgetStyleType>(PropertyName, Specifier);
	}

private:

	static TSharedRef<FSlateStyleSet> Create();
	static TSharedPtr<FSlateStyleSet> StyleInstance;
	static TMap<FName,UTexture2D*> Sprites;
};
