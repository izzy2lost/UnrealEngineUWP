// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "Units/RigUnit.h"
#include "DecoratorBase/DecoratorPtr.h"
#include "AnimNextExecuteContext.generated.h"

struct FAnimNextGraphInstance;
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
		check(Context != nullptr);
		return *Context;
	}

	FAnimNextGraphInstance& GetGraphInstance() const
	{
		check(GraphInstance != nullptr);
		return *GraphInstance;
	}

	EAnimNextGraphSimulationSteps GetSimulationSteps() const { return SimulationSteps; }

	virtual void Copy(const FRigVMExecuteContext* InOtherContext) override
	{
		Super::Copy(InOtherContext);

		const FAnimNextExecuteContext* OtherContext = (const FAnimNextExecuteContext*)InOtherContext;
		Context = OtherContext->Context;
		GraphInstance = OtherContext->GraphInstance;
		SimulationSteps = OtherContext->SimulationSteps;
	}

private:
	void SetContextData(const UE::AnimNext::FContext& InContext)
	{
		Context = &InContext;
	}

	void SetGraphInstance(FAnimNextGraphInstance& InGraphInstance)
	{
		GraphInstance = &InGraphInstance;
	}

	void SetSimulationSteps(EAnimNextGraphSimulationSteps InSimulationSteps)
	{
		SimulationSteps = InSimulationSteps;
	}

	// Call this to reset the context to its original state to detect stale usage, can't call it Reset due to virtual in base with that name
	void DebugReset()
	{
		Context = nullptr;
		GraphInstance = nullptr;
		SimulationSteps = EAnimNextGraphSimulationSteps::None;
	}

	const UE::AnimNext::FContext* Context = nullptr;
	FAnimNextGraphInstance* GraphInstance = nullptr;
	EAnimNextGraphSimulationSteps SimulationSteps = EAnimNextGraphSimulationSteps::None;

	friend class UAnimNextGraph;
	friend class UAnimNextParameterBlock;
};

USTRUCT(meta=(ExecuteContext="FAnimNextExecuteContext"))
struct FRigUnit_AnimNextBase : public FRigUnit
{
	GENERATED_BODY()
};
