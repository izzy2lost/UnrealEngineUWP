// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "AvaActor.generated.h"

/** Avalanche Actor */
UCLASS(MinimalAPI, DisplayName = "Motion Design Actor")
class AAvaActor : public AActor
{
	GENERATED_BODY()

public:
	AAvaActor();

	void SetIsPlaceholder(bool bInIsPlaceholder = false) { bIsPlaceholder = bInIsPlaceholder; }

	bool GetIsPlaceholder() const { return bIsPlaceholder; }

	bool UpdateGeneratedClass(bool bForceUpdate = false);

	//~ Begin AActor
	virtual void PostActorCreated() override;

#if WITH_EDITOR
	virtual FString GetDefaultActorLabel() const override;
#endif
	//~ End AActor

	//~ Begin UObject
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	//~ End UObject

private:
	bool bGeneratedClassUpdated = false;
	bool bIsPlaceholder = false;
};
