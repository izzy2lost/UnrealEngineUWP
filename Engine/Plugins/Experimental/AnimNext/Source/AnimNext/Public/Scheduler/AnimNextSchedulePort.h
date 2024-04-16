// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAnimNextScheduleTermInterface.h"
#include "AnimNextSchedulePort.generated.h"

class UAnimNextSchedule;
struct FAnimNextSchedulePortTask;
struct FAnimNextEditorParam;

namespace UE::AnimNext
{
	struct FScheduleTermContext;
}

UCLASS(Abstract)
class ANIMNEXT_API UAnimNextSchedulePort : public UObject, public IAnimNextScheduleTermInterface
{
	GENERATED_BODY()

private:
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend struct FAnimNextSchedulePortTask;

	// Run the port's logic
	virtual void Run(const UE::AnimNext::FScheduleTermContext& InContext) const PURE_VIRTUAL(UAnimNextSchedulePort::Run, )

	// Get any required parameters for this port
	virtual TConstArrayView<FAnimNextEditorParam> GetRequiredParameters() const PURE_VIRTUAL(UAnimNextSchedulePort::GetRequiredParameters, return TConstArrayView<FAnimNextEditorParam>(); )
};
