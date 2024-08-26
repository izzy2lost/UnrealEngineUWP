// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlendStack/AnimNode_BlendStack.h"
#include "Chimera/ChimeraLibrary.h"
#include "AnimNode_Chimera.generated.h"

struct FAnimationInitializeContext;
struct FNodeDebugData;

USTRUCT(BlueprintInternalUseOnly, Experimental)
struct CHIMERA_API FAnimNode_Chimera : public FAnimNode_BlendStack_Standalone
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links, meta = (DisplayPriority = 0))
	FPoseLink Source;

	UPROPERTY(EditAnywhere, Category = Chimera, meta = (PinHiddenByDefault))
	TArray<FChimeraAvailability> Availabilities;

	UPROPERTY(EditAnywhere, Category = Chimera, meta = (PinHiddenByDefault))
	bool bValidateResultAgainstAvailabilities = true;

	// time from the beginning of the interaction to warp to full translation alignment
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Warping, meta = (PinHiddenByDefault, ClampMin = "0"))
	float InitialTranslationWarpTime = 0.2f;

	// time from the beginning of the interaction to warp to full rotation alignment
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Warping, meta = (PinHiddenByDefault, ClampMin = "0"))
	float InitialRotationWarpTime = 0.2f;

	// tunable animation transition blend time 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Blending, meta = (PinHiddenByDefault, ClampMin = "0"))
	float BlendTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Blending, meta = (PinHiddenByDefault, UseAsBlendProfile = true))
	TObjectPtr<UBlendProfile> BlendProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Blending, meta = (PinHiddenByDefault))
	EAlphaBlendOption BlendOption = EAlphaBlendOption::Linear;

	// tunable animation transition blend time 
	UPROPERTY(EditAnywhere, Category = Blending)
	bool bUseInertialBlend = false;

	// Reset the blend stack if it has become relevant to the graph after not being updated on previous frames.
	UPROPERTY(EditAnywhere, Category = Blending)
	bool bResetOnBecomingRelevant = true;

	// FAnimNode_Base interface
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;

	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	// End of FAnimNode_Base interface

protected:
	float BlendLerp = 0.f;
	float TranslationWarpLerp = 0.f;
	float RotationWarpLerp = 0.f;

	// Update Counter for detecting being relevant
	FGraphTraversalCounter UpdateCounter;

	// wanted world transform for full aligment interaction
	FTransform FullAlignedActorRootBoneTransform = FTransform::Identity;
};
