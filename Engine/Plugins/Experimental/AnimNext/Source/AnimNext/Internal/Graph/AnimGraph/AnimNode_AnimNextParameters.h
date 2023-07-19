// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "Param/ParamStack.h"
#include "AnimNode_AnimNextParameters.generated.h"

class UAnimNextParameterBlock;

USTRUCT(BlueprintInternalUseOnly)
struct FAnimNode_AnimNextParameters : public FAnimNode_Base
{
	GENERATED_BODY()

	friend class UAnimGraphNode_AnimNextParameters;

	FAnimNode_AnimNextParameters() = default;
	FAnimNode_AnimNextParameters(const FAnimNode_AnimNextParameters& InOther);
	FAnimNode_AnimNextParameters& operator=(const FAnimNode_AnimNextParameters& InOther);
	FAnimNode_AnimNextParameters(FAnimNode_AnimNextParameters&& InOther);
	FAnimNode_AnimNextParameters& operator=(FAnimNode_AnimNextParameters&& InOther);
	~FAnimNode_AnimNextParameters() = default;

private:
	UPROPERTY(EditAnywhere, Category = Links)
	FPoseLink Source;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = Settings, meta=(FoldProperty, PinHiddenByDefault))
	TObjectPtr<UAnimNextParameterBlock> ParameterBlock;
#endif

	// Cache previous param block so we know when it changes via pin
	UAnimNextParameterBlock* PreviousParameterBlock = nullptr;

	// Cached layer
	UE::AnimNext::FParamStack::FLayerHandle ParamLayerHandle;

	// Property bag for layer backing storage
	UPROPERTY(Transient)
	FInstancedPropertyBag PropertyBag;

private:
	// FAnimNode_Base
	ANIMNEXT_API virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	ANIMNEXT_API virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	ANIMNEXT_API virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	ANIMNEXT_API virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	ANIMNEXT_API virtual void GatherDebugData(FNodeDebugData& DebugData) override;

	UAnimNextParameterBlock* GetParameterBlock() const;
};
