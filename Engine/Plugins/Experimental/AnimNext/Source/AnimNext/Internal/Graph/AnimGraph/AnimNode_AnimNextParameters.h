// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "Param/ParamStackLayerHandle.h"
#include "AnimNode_AnimNextParameters.generated.h"

class IAnimNextParameterSourceInterface;
class UAnimNextParameters;

USTRUCT(BlueprintInternalUseOnly)
struct FAnimNode_AnimNextParameters : public FAnimNode_Base
{
	GENERATED_BODY()

	friend class UAnimGraphNode_AnimNextParameters;

	FAnimNode_AnimNextParameters() = default;
	FAnimNode_AnimNextParameters(const FAnimNode_AnimNextParameters& InOther);
	FAnimNode_AnimNextParameters& operator=(const FAnimNode_AnimNextParameters& InOther);
	FAnimNode_AnimNextParameters(FAnimNode_AnimNextParameters&& InOther) noexcept;
	FAnimNode_AnimNextParameters& operator=(FAnimNode_AnimNextParameters&& InOther) noexcept;
	virtual ~FAnimNode_AnimNextParameters() override = default;

private:
	UPROPERTY(EditAnywhere, Category = Links)
	FPoseLink Source;

	UPROPERTY(EditAnywhere, Category = Settings)
	TScriptInterface<IAnimNextParameterSourceInterface> Parameters;

	// Cache previous param block so we know when it changes via pin
	IAnimNextParameterSourceInterface* PreviousParameters = nullptr;

	// Cached layer
	UE::AnimNext::FParamStackLayerHandle ParamLayerHandle;

private:
	// FAnimNode_Base
	ANIMNEXT_API virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	ANIMNEXT_API virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	ANIMNEXT_API virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	ANIMNEXT_API virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	ANIMNEXT_API virtual void GatherDebugData(FNodeDebugData& DebugData) override;
};
