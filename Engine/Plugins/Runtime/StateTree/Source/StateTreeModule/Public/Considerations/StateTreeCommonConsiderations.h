// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeConsiderationBase.h"
#include "StateTreeAnyEnum.h"
#include "StateTreeCommonConsiderations.generated.h"

USTRUCT()
struct STATETREEMODULE_API FStateTreeFloatConsiderationInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float RawScore = .0f;
};

/**
 * Consideration using a Float Parameter as raw score.
 * You can use the parameter as a float constant or bind it to a property that changes value over time.
 */
USTRUCT(DisplayName = "Float")
struct STATETREEMODULE_API FStateTreeFloatConsideration : public FStateTreeConsiderationCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeFloatConsiderationInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

protected:
	virtual float ComputeRawScore(FStateTreeExecutionContext& Context) const override;
};
