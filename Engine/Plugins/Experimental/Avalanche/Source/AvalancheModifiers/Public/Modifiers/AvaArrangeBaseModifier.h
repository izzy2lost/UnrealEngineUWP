// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaAttachmentBaseModifier.h"
#include "Extensions/AvaRenderStateUpdateModifierExtension.h"
#include "Extensions/AvaTransformUpdateModifierExtension.h"
#include "AvaArrangeBaseModifier.generated.h"

class AActor;

/**
 * Abstract base class for modifiers dealing with arrangement and attachment actors on self
 */
UCLASS(Abstract)
class AVALANCHEMODIFIERS_API UAvaArrangeBaseModifier : public UAvaAttachmentBaseModifier
	, public IAvaRenderStateUpdateHandler
	, public IAvaTransformUpdateHandler
{
	GENERATED_BODY()
	
protected:
	//~ Begin UActorModifierCoreBase
	virtual void OnModifierAdded(EActorModifierCoreEnableReason InReason) override;
	virtual void OnModifierDisabled(EActorModifierCoreDisableReason InReason) override;
	virtual void OnModifiedActorTransformed() override;
	//~ End UActorModifierCoreBase
	
	//~ Begin IAvaSceneTreeUpdateModifierExtension
	virtual void OnSceneTreeTrackedActorChildrenChanged(int32 InIdx, const TSet<TWeakObjectPtr<AActor>>& InPreviousChildrenActors, const TSet<TWeakObjectPtr<AActor>>& InNewChildrenActors) override;
	virtual void OnSceneTreeTrackedActorDirectChildrenChanged(int32 InIdx, const TArray<TWeakObjectPtr<AActor>>& InPreviousChildrenActors, const TArray<TWeakObjectPtr<AActor>>& InNewChildrenActors) override;
	//~ End IAvaSceneTreeUpdateModifierExtension

	//~ Begin IAvaRenderStateUpdateExtension
	virtual void OnRenderStateUpdated(AActor* InActor, UActorComponent* InComponent) override {}
	virtual void OnActorVisibilityChanged(AActor* InActor) override {}
	//~ End IAvaRenderStateUpdateExtension

	//~ Begin IAvaTransformUpdateExtension
	virtual void OnTransformUpdated(AActor* InActor, bool bInParentMoved) override {}
	//~ End IAvaTransformUpdateExtension

	/** Used to track self modified actor for changes */
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	FAvaSceneTreeActor ReferenceActor;
};
