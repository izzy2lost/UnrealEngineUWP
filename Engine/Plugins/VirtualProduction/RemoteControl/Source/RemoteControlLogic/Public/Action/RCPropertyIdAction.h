// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Action/RCAction.h"
#include "RCPropertyIdAction.generated.h"

class URCVirtualPropertySelfContainer;

/**
 * Action for PropertyId
 */
UCLASS()
class REMOTECONTROLLOGIC_API URCPropertyIdAction : public URCAction
{
	GENERATED_BODY()
	virtual ~URCPropertyIdAction() override;

public:
	//~ BEGIN : URCAction Interface
	virtual void Execute() const override;
	virtual void UpdateEntityIds(const TMap<FGuid, FGuid>& InEntityIdMap) override;
	//~ END : URCAction Interface
	
	//~ BEGIN : UObject Interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ END : URCAction Interface

	void UpdatePropertyId();

	void Initialize();

	void OnEntityUnexposed(URemoteControlPreset* InPreset, const FGuid& InGuid);

public:
	/** Holds the field identifier associated with this. */
	UPROPERTY()
	FName PropertyId = NAME_None;

	/** Virtual Property Container */
	UPROPERTY()
	TMap<FName, TObjectPtr<URCVirtualPropertySelfContainer>> PropertySelfContainer;

	/** Cached Virtual Property Container */
	UPROPERTY()
	TMap<FName, TObjectPtr<URCVirtualPropertySelfContainer>> CachedPropertySelfContainer;

private:
	/** Holds the default Object. */
	UPROPERTY(Transient)
	TObjectPtr<UObject> DefaultObject = nullptr;
};
