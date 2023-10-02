// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Param/IAnimNextParameterSourceInterface.h"
#include "AnimNextParameterSourceRef.generated.h"

class UAnimNextParameterBlock;
class IAnimNextParameterSourceInterface;

UENUM()
enum class EAnimNextParameterSourceRefType : int32
{
	// The hosting component provides parameters via its variables
	Self,

	// An external parameter block asset
	Asset,

	// Another component in this actor provides parameters via its variables
	Component,

	// This actor provides parameters via its variables
	Actor,

	// Parameters are provided inline
	Inline
};

// Wrapper for different kinds of parameter source
USTRUCT()
struct FAnimNextParameterSourceRef 
#if CPP
	: public IAnimNextParameterSourceInterface
#endif
{
	GENERATED_BODY()

	// Get the parameter source that this reference refers to
	const IAnimNextParameterSourceInterface* Get(const UObject* InContextObject) const;

#if CPP
	// IAnimNextParameterSourceInterface interface
	virtual void UpdateLayer(UE::AnimNext::FParamStackLayerHandle& InHandle) const override {}
	virtual UE::AnimNext::FParamStackLayerHandle CacheLayer() const override;
#endif

	// How the parameters are specified
	UPROPERTY(EditAnywhere, Category = "Parameters")
	EAnimNextParameterSourceRefType Type = EAnimNextParameterSourceRefType::Asset;

	// External parameter source interface
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditCondition = "Type == FAnimNextParameterSourceRefType::Asset", EditConditionHides))
	TScriptInterface<IAnimNextParameterSourceInterface> Asset;

	// Component ref to allow tracking of other components in an actor
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditCondition = "Type == FAnimNextParameterSourceRefType::Component", EditConditionHides))
	FComponentReference Component;

	// Properties provided inline
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditCondition = "Type == FAnimNextParameterSourceRefType::Inline", EditConditionHides))
	FInstancedPropertyBag InlineParameters;
};
