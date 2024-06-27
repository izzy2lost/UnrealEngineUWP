// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineBaseTypes.h"
#include "Templates/SharedPointer.h"
#include "UObject/Object.h"

#include "UnrealEdViewportToolbarContext.generated.h"

namespace UE::UnrealEd
{

DECLARE_DELEGATE_RetVal_OneParam(bool, IsViewModeSupportedDelegate, EViewModeIndex);

}

class SEditorViewport;

UCLASS()
class UNREALED_API UUnrealEdViewportToolbarContext : public UObject
{
	GENERATED_BODY()

public:
	TWeakPtr<SEditorViewport> Viewport;
	UE::UnrealEd::IsViewModeSupportedDelegate IsViewModeSupported;
};
