// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "Units/RigUnit.h"
#include "DecoratorBase/DecoratorPtr.h"

#include "AnimNextExecuteContext.generated.h"

namespace UE::AnimNext
{
	struct FContext;
}

/**
  * An enum to control which simulation steps should be performed when the animation graph is processed.
  */
enum class EAnimNextGraphSimulationSteps
{
	// No step is performed
	None = 0x00,

	// Performs the update step
	Update = 0x01,

	// Performs the evaluate step
	Evaluate = 0x02,

	// Perform all simulation steps
	All = 0xff,
};

ENUM_CLASS_FLAGS(EAnimNextGraphSimulationSteps)

USTRUCT(BlueprintType)
struct FAnimNextExecuteContext : public FRigVMExecuteContext
{
	GENERATED_BODY()

	FAnimNextExecuteContext()
		: FRigVMExecuteContext()
		, Context(nullptr)
	{
	}

	const UE::AnimNext::FContext& GetContext() const
	{
		check(Context);
		return *Context;
	}

	const TArrayView<const uint8>& GetSharedDataBuffer() const { return SharedDataBuffer; }

	UE::AnimNext::FWeakDecoratorPtr GetGraphInstancePtr() const { return GraphInstancePtr; }

	EAnimNextGraphSimulationSteps GetSimulationSteps() const { return SimulationSteps; }

	virtual void Copy(const FRigVMExecuteContext* InOtherContext) override
	{
		Super::Copy(InOtherContext);

		const FAnimNextExecuteContext* OtherContext = (const FAnimNextExecuteContext*)InOtherContext;
		Context = OtherContext->Context;
		SharedDataBuffer = OtherContext->SharedDataBuffer;
		GraphInstancePtr = OtherContext->GraphInstancePtr;
	}

private:
	void SetContextData(const UE::AnimNext::FContext& InContext)
	{
		Context = &InContext;
	}

	void InitializeWithGraph(TArrayView<const uint8> InSharedDataBuffer, UE::AnimNext::FWeakDecoratorPtr InGraphInstancePtr)
	{
		SharedDataBuffer = InSharedDataBuffer;
		GraphInstancePtr = InGraphInstancePtr;
	}

	void SetSimulationSteps(EAnimNextGraphSimulationSteps InSimulationSteps)
	{
		SimulationSteps = InSimulationSteps;
	}

	const UE::AnimNext::FContext* Context;

	TArrayView<const uint8> SharedDataBuffer;
	UE::AnimNext::FWeakDecoratorPtr GraphInstancePtr;
	EAnimNextGraphSimulationSteps SimulationSteps;

	friend class UAnimNextGraph;
	friend class UAnimNextParameterBlock;
};

USTRUCT(meta=(ExecuteContext="FAnimNextExecuteContext"))
struct FRigUnit_AnimNextBase : public FRigUnit
{
	GENERATED_BODY()
};
