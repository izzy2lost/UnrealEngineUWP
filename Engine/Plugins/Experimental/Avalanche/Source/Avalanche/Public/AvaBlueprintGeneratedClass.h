// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Package.h"

#include "AvaBlueprintGeneratedClass.generated.h"

class AAvaActor;
class UAvaSequence;

UCLASS(MinimalAPI, DisplayName = "Motion Design Blueprint Generated Class")
class UAvaBlueprintGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_BODY()

public:
	void UpdateProperties(AAvaActor* InActor);

	UPROPERTY()
	TArray<TObjectPtr<UAvaSequence>> Animations;
};
