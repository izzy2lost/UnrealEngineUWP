// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "SolverDataComponent.generated.h"

/**
 * Base class for all components that stores recorded solver data
 */
UCLASS(Abstract)
class USolverDataComponent : public UActorComponent
{
	GENERATED_BODY()

	virtual void ClearData() PURE_VIRTUAL(ISolverDataComponent::ClearData);
};
