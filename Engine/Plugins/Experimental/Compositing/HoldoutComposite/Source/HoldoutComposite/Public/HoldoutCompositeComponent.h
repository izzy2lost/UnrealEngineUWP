// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Components/SceneComponent.h"

#include "HoldoutCompositeComponent.generated.h"

UCLASS(ClassGroup = Rendering, editinlinenew, meta = (BlueprintSpawnableComponent), MinimalAPI)
class UHoldoutCompositeComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UActorComponent interface
	HOLDOUTCOMPOSITE_API virtual void OnRegister() override;
	HOLDOUTCOMPOSITE_API virtual void OnUnregister() override;
	//~ End UActorComponent interface
	
	//~ Begin USceneComponent Interface.
	HOLDOUTCOMPOSITE_API virtual void OnAttachmentChanged() override;
	HOLDOUTCOMPOSITE_API virtual void DetachFromComponent(const FDetachmentTransformRules& DetachmentRules) override;
	//~ End USceneComponent Interface

private:

	/* Private implementation of the register method. */
	void RegisterCompositeImpl();

	/* Private implementation of the unregister method. */
	void UnregisterCompositeImpl();

	/* Primitive holdout property value of the parent component. */
	UPROPERTY()
	TOptional<bool> bCachedParentHoldout = false;
};

