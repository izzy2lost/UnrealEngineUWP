// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "WidgetBlueprint.h"

namespace UE::UMGWidgetPreview::Private
{
	struct FWidgetTypeTuple
	{
		explicit FWidgetTypeTuple(const UUserWidget* InUserWidget)
		{
			check(InUserWidget);

			ClassDefaultObject = InUserWidget;
			BlueprintGeneratedClass = Cast<UWidgetBlueprintGeneratedClass>(ClassDefaultObject->GetClass());
			Blueprint = Cast<UWidgetBlueprint>(BlueprintGeneratedClass->ClassGeneratedBy);
		}

		const UUserWidget* ClassDefaultObject = nullptr;
		UWidgetBlueprint* Blueprint = nullptr;
		UWidgetBlueprintGeneratedClass* BlueprintGeneratedClass = nullptr;
	};
}
