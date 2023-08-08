// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "Units/RigUnit.h"
#include "AnimNextExecuteContext.generated.h"

namespace UE::AnimNext
{
	struct FContext;
}

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

	void SetContextData(const UE::AnimNext::FContext& InContext)
	{
		Context = &InContext;
	}

	virtual void Copy(const FRigVMExecuteContext* InOtherContext) override
	{
		Super::Copy(InOtherContext);

		const FAnimNextExecuteContext* OtherContext = (const FAnimNextExecuteContext*)InOtherContext;
		Context = OtherContext->Context;
	}

private:
	const UE::AnimNext::FContext* Context;
};

USTRUCT(meta=(ExecuteContext="FAnimNextExecuteContext"))
struct FRigUnit_AnimNextBase : public FRigUnit
{
	GENERATED_BODY()
};
