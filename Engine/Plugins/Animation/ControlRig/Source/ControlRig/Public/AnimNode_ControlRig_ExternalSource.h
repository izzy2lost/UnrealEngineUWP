// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNode_ControlRigBase.h"
#include "AnimNode_ControlRig_ExternalSource.generated.h"

/**
 * Animation node that allows animation ControlRig output to be used in an animation graph
 */
USTRUCT()
struct CONTROLRIG_API FAnimNode_ControlRig_ExternalSource : public FAnimNode_ControlRigBase
{
	GENERATED_BODY()

	FAnimNode_ControlRig_ExternalSource();

	void SetControlRig(UBaseControlRig* InControlRig);
	virtual UBaseControlRig* GetControlRig() const override;
	virtual TSubclassOf<UBaseControlRig> GetControlRigClass() const override;
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;

private:
	UPROPERTY(transient)
	TWeakObjectPtr<UBaseControlRig> ControlRig;
};

