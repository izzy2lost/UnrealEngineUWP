// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "Units/RigUnit.h"
#include "AnimNextExecuteContext.generated.h"

namespace UE::AnimNext
{
	struct FLatentPropertyHandle;
}

USTRUCT(BlueprintType)
struct FAnimNextExecuteContext : public FRigVMExecuteContext
{
	GENERATED_BODY()

	FAnimNextExecuteContext() = default;

	const TConstArrayView<UE::AnimNext::FLatentPropertyHandle>& GetLatentHandles() const { return LatentHandles; }
	void* GetDestinationBasePtr() const { return DestinationBasePtr; }
	bool IsFrozen() const { return bIsFrozen; }

	virtual void Copy(const FRigVMExecuteContext* InOtherContext) override
	{
		Super::Copy(InOtherContext);

		const FAnimNextExecuteContext* OtherContext = (const FAnimNextExecuteContext*)InOtherContext;
		LatentHandles = OtherContext->LatentHandles;
		DestinationBasePtr = OtherContext->DestinationBasePtr;
		bIsFrozen = OtherContext->bIsFrozen;
	}

private:
	void SetupForExecution(const TConstArrayView<UE::AnimNext::FLatentPropertyHandle>& InLatentHandles, void* InDestinationBasePtr, bool bInIsFrozen)
	{
		LatentHandles = InLatentHandles;
		DestinationBasePtr = InDestinationBasePtr;
		bIsFrozen = bInIsFrozen;
	}

	// Call this to reset the context to its original state to detect stale usage, can't call it Reset due to virtual in base with that name
	void DebugReset()
	{
		LatentHandles = TConstArrayView<UE::AnimNext::FLatentPropertyHandle>();
		DestinationBasePtr = nullptr;
		bIsFrozen = false;
	}

	TConstArrayView<UE::AnimNext::FLatentPropertyHandle> LatentHandles;
	void* DestinationBasePtr = nullptr;
	bool bIsFrozen = false;

	friend struct FAnimNextGraphInstance;
};

USTRUCT(meta=(ExecuteContext="FAnimNextExecuteContext"))
struct FRigUnit_AnimNextBase : public FRigUnit
{
	GENERATED_BODY()
};
