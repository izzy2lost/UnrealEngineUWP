// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "MidiStepSequenceFactory.generated.h"

UCLASS()
class UMidiStepSequenceFactory : public UFactory
{
	GENERATED_BODY()
public:
	UMidiStepSequenceFactory();
	UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn);
};

