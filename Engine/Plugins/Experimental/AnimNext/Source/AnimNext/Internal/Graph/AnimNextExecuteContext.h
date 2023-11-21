// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "Units/RigUnit.h"
#include "AnimNextExecuteContext.generated.h"

USTRUCT(BlueprintType)
struct FAnimNextExecuteContext : public FRigVMExecuteContext
{
	GENERATED_BODY()

	FAnimNextExecuteContext() = default;

	int32 GetLatentPinIndex() const { return LatentPinIndex; }
	void* GetDestinationPtr() const { return DestinationPtr; }

	virtual void Copy(const FRigVMExecuteContext* InOtherContext) override
	{
		Super::Copy(InOtherContext);

		const FAnimNextExecuteContext* OtherContext = (const FAnimNextExecuteContext*)InOtherContext;
		LatentPinIndex = OtherContext->LatentPinIndex;
		DestinationPtr = OtherContext->DestinationPtr;
	}

private:
	void SetupForExecution(int32 InLatentPinIndex, void* InDestinationPtr)
	{
		LatentPinIndex = InLatentPinIndex;
		DestinationPtr = InDestinationPtr;
	}

	// Call this to reset the context to its original state to detect stale usage, can't call it Reset due to virtual in base with that name
	void DebugReset()
	{
		LatentPinIndex = INDEX_NONE;
		DestinationPtr = nullptr;
	}

	int32 LatentPinIndex = INDEX_NONE;
	void* DestinationPtr = nullptr;

	friend struct FAnimNextGraphInstance;
};

USTRUCT(meta=(ExecuteContext="FAnimNextExecuteContext"))
struct FRigUnit_AnimNextBase : public FRigUnit
{
	GENERATED_BODY()
};
