// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "UObject/WeakObjectPtrTemplatesFwd.h"

class AActor;
struct FGuid;

/** Interface for external implementations to interact with the Signature View Model */
class IRCSignatureItem
{
public:
	virtual void AddFieldEntities(TConstArrayView<FGuid> InFieldEntityIds) = 0;

	virtual void ApplySignature(TConstArrayView<TWeakObjectPtr<AActor>> InActors) = 0;
};
