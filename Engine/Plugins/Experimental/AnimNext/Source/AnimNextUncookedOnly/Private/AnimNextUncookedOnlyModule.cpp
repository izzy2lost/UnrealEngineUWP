// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UncookedOnlyUtils.h"
#include "Modules/ModuleManager.h"
#include "Scheduler/AnimNextSchedule.h"

namespace UE::AnimNext::UncookedOnly
{
	class FModule : public IModuleInterface
	{
	private:
		virtual void StartupModule() override
		{
			// TEMP: Bind the compilation function for schedules
			UAnimNextSchedule::CompileFunction = [](UAnimNextSchedule* InSchedule)
			{
				FUtils::CompileSchedule(InSchedule);
			};
		}

		virtual void ShutdownModule() override
		{
			UAnimNextSchedule::CompileFunction = nullptr;
		}
	};
}

IMPLEMENT_MODULE(UE::AnimNext::UncookedOnly::FModule, AnimNextUncookedOnly);
