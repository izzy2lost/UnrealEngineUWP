// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextActorComponentLibrary.h"
#include "Scheduler/AnimNextTickFunctionBinding.h"
#include "Components/ActorComponent.h"

FAnimNextTickFunctionBinding UAnimNextActorComponentLibrary::GetTick(UActorComponent* InComponent)
{
	FAnimNextTickFunctionBinding Binding;
	Binding.Object = InComponent;
	Binding.TickFunction = &InComponent->PrimaryComponentTick;
	return Binding;
}
