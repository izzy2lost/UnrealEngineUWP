// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEffectorsEditorStyle.h"
#include "AvaClonerEffectorShared.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/StyleColors.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateStyleRegistry.h"

FAvaEffectorsEditorStyle::FAvaEffectorsEditorStyle()
	: FSlateStyleSet(TEXT("AvalancheEffectorsEditor"))
{
	const FVector2f Icon20x20(20.0f, 20.0f);
	const FVector2f Icon32x32(32.0f, 32.0f);
	
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Avalanche"));
	
	check(Plugin.IsValid());
	
	ContentRootDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"));

	Set("AvalancheEffectorsEditor.Tool_Actor_Effector", new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/effector", Icon20x20));
	Set("AvalancheEffectorsEditor.Tool_Actor_Cloner", new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/cloner", Icon20x20));
	Set("Tool_Actor_Effector", new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/effector", Icon20x20));
	Set("Tool_Actor_Cloner", new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/cloner", Icon20x20));

	// Easing
	if (const UEnum* EasingEnum = StaticEnum<EAvaClonerEasing>())
	{
		for (int32 Idx = 0; Idx < EasingEnum->GetMaxEnumValue(); Idx++)
		{
			FText ValueText;
			EasingEnum->GetDisplayValueAsText(static_cast<EAvaClonerEasing>(Idx), ValueText);
			const FString EasingString = ValueText.ToString().Replace(TEXT(" "), TEXT(""));
			const FName EasingName(TEXT("AvalancheIcons.Easing.") + EasingString);
			
			Set(EasingName, new IMAGE_BRUSH_SVG("Icons/ClonerIcons/" + EasingString, Icon32x32));
		}
	}
	
	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FAvaEffectorsEditorStyle::~FAvaEffectorsEditorStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}