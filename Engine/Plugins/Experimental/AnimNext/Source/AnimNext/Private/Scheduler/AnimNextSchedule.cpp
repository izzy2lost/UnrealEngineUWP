// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/AnimNextSchedule.h"
#include "Tasks/Task.h"
#include "Async/TaskGraphInterfaces.h"
#include "EngineLogs.h"
#include "UObject/AssetRegistryTagsContext.h"

#if WITH_EDITOR
TUniqueFunction<void(UAnimNextSchedule*)> UAnimNextSchedule::CompileFunction;
TUniqueFunction<void(const UAnimNextSchedule*, FAssetRegistryTagsContext)> UAnimNextSchedule::GetAssetRegistryTagsFunction;
#endif

void UAnimNextSchedule::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	CompileSchedule();
#endif
}

#if WITH_EDITOR

void UAnimNextSchedule::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	CompileSchedule();
}

void UAnimNextSchedule::PostEditUndo()
{
	Super::PostEditUndo();

	CompileSchedule();
}

void UAnimNextSchedule::CompileSchedule()
{
	check(CompileFunction);

	CompileFunction(this);
}

void UAnimNextSchedule::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::GetAssetRegistryTags(OutTags);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UAnimNextSchedule::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	check(GetAssetRegistryTagsFunction);

	GetAssetRegistryTagsFunction(this, Context);
}

#endif // #if WITH_EDITOR