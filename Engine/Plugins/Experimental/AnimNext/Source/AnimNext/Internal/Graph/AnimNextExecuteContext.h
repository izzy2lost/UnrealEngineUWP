// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "Units/RigUnit.h"
#include "DecoratorBase/DecoratorPtr.h"
#include "AnimNextExecuteContext.generated.h"

class UAnimNextGraph;

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

	FAnimNextExecuteContext() = default;

	const UE::AnimNext::FContext& GetContext() const
	{
		check(Context);
		return *Context;
	}

	const UAnimNextGraph* GetGraph() const { return Graph; }

	const TArrayView<const uint8>& GetSharedDataBuffer() const { return SharedDataBuffer; }

	UE::AnimNext::FWeakDecoratorPtr GetGraphInstancePtr() const { return GraphInstancePtr; }

	EAnimNextGraphSimulationSteps GetSimulationSteps() const { return SimulationSteps; }

	virtual void Copy(const FRigVMExecuteContext* InOtherContext) override
	{
		Super::Copy(InOtherContext);

		const FAnimNextExecuteContext* OtherContext = (const FAnimNextExecuteContext*)InOtherContext;
		Context = OtherContext->Context;
		Graph = OtherContext->Graph; 
		SharedDataBuffer = OtherContext->SharedDataBuffer;
		GraphInstancePtr = OtherContext->GraphInstancePtr;
		SimulationSteps = OtherContext->SimulationSteps;
	}

private:
	void SetContextData(const UE::AnimNext::FContext& InContext)
	{
		Context = &InContext;
	}

	void InitializeWithGraph(const UAnimNextGraph* InGraph, TArrayView<const uint8> InSharedDataBuffer, UE::AnimNext::FWeakDecoratorPtr InGraphInstancePtr)
	{
		Graph = InGraph;
		SharedDataBuffer = InSharedDataBuffer;
		GraphInstancePtr = InGraphInstancePtr;
	}

	void SetSimulationSteps(EAnimNextGraphSimulationSteps InSimulationSteps)
	{
		SimulationSteps = InSimulationSteps;
	}

	// Call this to reset the context to its original state to detect stale usage, can't call it Reset due to virtual in base with that name
	void DebugReset()
	{
		Context = nullptr;
		Graph = nullptr;
		SharedDataBuffer = TArrayView<const uint8>();
		GraphInstancePtr.Reset();
		SimulationSteps = EAnimNextGraphSimulationSteps::None;
	}

	const UE::AnimNext::FContext* Context = nullptr;
	const UAnimNextGraph* Graph = nullptr;
	TArrayView<const uint8> SharedDataBuffer;
	UE::AnimNext::FWeakDecoratorPtr GraphInstancePtr;
	EAnimNextGraphSimulationSteps SimulationSteps = EAnimNextGraphSimulationSteps::None;

	friend class UAnimNextGraph;
	friend class UAnimNextParameterBlock;
};

USTRUCT(meta=(ExecuteContext="FAnimNextExecuteContext"))
struct FRigUnit_AnimNextBase : public FRigUnit
{
	GENERATED_BODY()
};
