// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextSchedulePort.generated.h"

struct FAnimNextSchedulePortTask;

namespace UE::AnimNext
{
	struct FScheduleContext;
}

USTRUCT()
struct FAnimNextSchedulePort
{
	GENERATED_BODY()

private:
	friend struct FAnimNextSchedulePortTask;
	friend class UAnimNextComponent;

	// Object the port is bound to
	UPROPERTY()
	TWeakObjectPtr<UObject> Object;

	// Static data the port will write to
	// TODO: This should probably be a TUniqueFunction<TArrayView<uint8>(void)>
	TArrayView<uint8> Data;
};
