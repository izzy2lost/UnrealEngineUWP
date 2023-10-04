// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

#if WITH_EDITOR
#include "Misc/Guid.h"
#endif

namespace mu { class Model; }
class UCustomizableObject;


class FCustomizableObjectPrivateData
{
	TSharedPtr<mu::Model, ESPMode::ThreadSafe> MutableModel;

public:
	void SetModel(const TSharedPtr<mu::Model, ESPMode::ThreadSafe>& Model, const FGuid Identifier);
	const TSharedPtr<mu::Model, ESPMode::ThreadSafe>& GetModel();
	TSharedPtr<const mu::Model, ESPMode::ThreadSafe> GetModel() const;

#if WITH_EDITORONLY_DATA
	/** See UCustomizableObject::ParticipatingObjects. */
	CUSTOMIZABLEOBJECT_API TMap<TObjectPtr<const UObject>, FGuid>& GetParticipatingObjects(UCustomizableObject& Public);
#endif
	
	// See UCustomizableObjectSystem::LockObject. Must only be modified from the game thread
	bool bLocked = false;

#if WITH_EDITOR
	FGuid Identifier;

	bool bModelCompiledForCook = false;
	TArray<FString> CachedPlatformNames;
#endif
};

