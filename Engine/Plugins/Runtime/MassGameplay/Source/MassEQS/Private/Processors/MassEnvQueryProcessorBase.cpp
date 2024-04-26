// Copyright Epic Games, Inc. All Rights Reserved.

#include "Processors/MassEnvQueryProcessorBase.h"

#include "MassEQSSubsystem.h"

void UMassEnvQueryProcessorBase::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);
	UMassEQSSubsystem* MassEQSSubsystem = Owner.GetWorld()->GetSubsystem<UMassEQSSubsystem>();
	check(MassEQSSubsystem)

	if (CorrespondingRequestClass)
	{
		CachedRequestQueryIndex = MassEQSSubsystem->GetRequestQueueIndex(CorrespondingRequestClass);
	}
}