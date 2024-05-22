// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "ChaosVDSolverDataComponent.generated.h"

class FChaosVDScene;
/**
 * Base class for all components that stores recorded solver data
 */
UCLASS(Abstract)
class UChaosVDSolverDataComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	virtual void ClearData() PURE_VIRTUAL(UChaosVDSolverDataComponent::ClearData);
	virtual void SetScene(const TWeakPtr<FChaosVDScene>& InSceneWeakPtr);

protected:

	TWeakPtr<FChaosVDScene> SceneWeakPtr;
};

