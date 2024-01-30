// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Math/Transform.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class IAvaViewportClient;
class AActor;

/**
 * Resets child actors to their original transforms after the parent actor
 * is moved. Will fail if an actor and its grandchild are both selected.
 * Moving the child actor (actor->child->grandchild) will reset the relative
 * transform of the grandchild. To be honest, I'm not sure how UE would handle
 * this by default anyway...
 */

class AVALANCHEVIEWPORT_API FAvaChildTransformLockOperation
{
public:
	FAvaChildTransformLockOperation(TSharedRef<IAvaViewportClient> InAvaViewportClient);

	void Save();
	void Restore();

protected:
	TWeakPtr<IAvaViewportClient> AvaViewportClientWeak;

	TMap<TWeakObjectPtr<AActor>, FTransform> ChildActorTransforms;

	bool bAllowModify;	

	void AttemptModifyOnLockedActors();
};
