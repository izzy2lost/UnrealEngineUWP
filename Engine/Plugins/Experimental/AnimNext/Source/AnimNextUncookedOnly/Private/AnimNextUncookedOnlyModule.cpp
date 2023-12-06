// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "K2Node_CallFunction.h"
#include "UncookedOnlyUtils.h"
#include "Component/AnimNextComponentParameter.h"
#include "Component/AnimNextComponent.h"
#include "Engine/SCS_Node.h"
#include "Modules/ModuleManager.h"
#include "Param/AnimNextTag.h"
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

			// TEMP: Bind the asset registry tags function for schedules
			UAnimNextSchedule::GetAssetRegistryTagsFunction = [](const UAnimNextSchedule* InSchedule, TArray<UObject::FAssetRegistryTag>& OutTags)
			{
				FAnimNextParameterProviderAssetRegistryExports Exports;
				FUtils::GetScheduleParameters(InSchedule, Exports);
				
				FString TagValue;
				FAnimNextParameterProviderAssetRegistryExports::StaticStruct()->ExportText(TagValue, &Exports, nullptr, nullptr, PPF_None, nullptr);
				OutTags.Add(UObject::FAssetRegistryTag(UE::AnimNext::ExportsAnimNextAssetRegistryTag, TagValue, UObject::FAssetRegistryTag::TT_Hidden));
			};
			
			// Ensure that any BP components that we care about contribute to the parameter pool
			OnGetExtraObjectTagsHandle = UObject::FAssetRegistryTag::OnGetExtraObjectTags.AddLambda([](const UObject* InObject, TArray<UObject::FAssetRegistryTag>& OutTags)
			{
				if(const UBlueprint* Blueprint = Cast<UBlueprint>(InObject))
				{
					FAnimNextParameterProviderAssetRegistryExports Exports;
					FUtils::GetBlueprintParameters(Blueprint, Exports);

					FString TagValue;
					FAnimNextParameterProviderAssetRegistryExports::StaticStruct()->ExportText(TagValue, &Exports, nullptr, nullptr, PPF_None, nullptr);
					OutTags.Add(UObject::FAssetRegistryTag(UE::AnimNext::ExportsAnimNextAssetRegistryTag, TagValue, UObject::FAssetRegistryTag::TT_Hidden));
				}
			});
		}

		virtual void ShutdownModule() override
		{
			UAnimNextSchedule::GetAssetRegistryTagsFunction = nullptr;
			UAnimNextSchedule::CompileFunction = nullptr;

			UObject::FAssetRegistryTag::OnGetExtraObjectTags.Remove(OnGetExtraObjectTagsHandle);
		}

		FDelegateHandle OnGetExtraObjectTagsHandle;
	};
}

IMPLEMENT_MODULE(UE::AnimNext::UncookedOnly::FModule, AnimNextUncookedOnly);
