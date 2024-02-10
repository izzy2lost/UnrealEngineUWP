// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "AvaSceneAttribute.generated.h"

/** Scene Attributes are objects that are added to a Scene to describe the scene to other systems */
UCLASS(MinimalAPI, Abstract, EditInlineNew, DisplayName="Motion Design Scene Attribute")
class UAvaSceneAttribute : public UObject
{
	GENERATED_BODY()
};
