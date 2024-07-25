// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextUncookedOnlyModule.h"

#include "Module/AnimNextModuleWorkspaceAssetUserData.h"
#include "UncookedOnlyUtils.h"
#include "Engine/Blueprint.h"
#include "Modules/ModuleManager.h"
#include "Param/AnimNextParamUniversalObjectLocator.h"
#include "Param/IParameterSourceType.h"
#include "Param/ObjectProxyType.h"
#include "Scheduler/AnimNextSchedule.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "MessageLogModule.h"

#define LOCTEXT_NAMESPACE "AnimNextUncookedOnlyModule"

namespace UE::AnimNext::UncookedOnly
{

void FModule::StartupModule()
{
	// TEMP: Bind the compilation function for schedules
	UAnimNextSchedule::CompileFunction = [](UAnimNextSchedule* InSchedule)
	{
		FUtils::CompileSchedule(InSchedule);
	};

	// TEMP: Bind the asset registry tags function for schedules
	UAnimNextSchedule::GetAssetRegistryTagsFunction = [](const UAnimNextSchedule* InSchedule, FAssetRegistryTagsContext Context)
	{
		FAnimNextParameterProviderAssetRegistryExports Exports;
        {
		    FUtils::GetScheduleParameters(InSchedule, Exports);
		    
		    FString TagValue;
		    FAnimNextParameterProviderAssetRegistryExports::StaticStruct()->ExportText(TagValue, &Exports, nullptr, nullptr, PPF_None, nullptr);
		    Context.AddTag(UObject::FAssetRegistryTag(UE::AnimNext::ExportsAnimNextAssetRegistryTag, TagValue, UObject::FAssetRegistryTag::TT_Hidden));
        }

		FWorkspaceOutlinerItemExports OutlinerExports;
		{
			FWorkspaceOutlinerItemExport& RootAssetExport = OutlinerExports.Exports.Add_GetRef(FWorkspaceOutlinerItemExport(InSchedule->GetFName(), InSchedule));
			RootAssetExport.GetData().InitializeAsScriptStruct(FAnimNextSchedulerData::StaticStruct());
			FAnimNextSchedulerData& AssetData = RootAssetExport.GetData().GetMutable<FAnimNextSchedulerData>();

			{
				FString TagValue;
				FWorkspaceOutlinerItemExports::StaticStruct()->ExportText(TagValue, &OutlinerExports, nullptr, nullptr, PPF_None, nullptr);
				Context.AddTag(UObject::FAssetRegistryTag(UE::Workspace::ExportsWorkspaceItemsRegistryTag, TagValue, UObject::FAssetRegistryTag::TT_Hidden));
			}
		}
	};
	
	// Ensure that any BP components that we care about contribute to the parameter pool
	OnGetExtraObjectTagsHandle = UObject::FAssetRegistryTag::OnGetExtraObjectTagsWithContext.AddLambda([](FAssetRegistryTagsContext Context)
	{
		const UObject* InObject = Context.GetObject();
		if(const UBlueprint* Blueprint = Cast<UBlueprint>(InObject))
		{
			FAnimNextParameterProviderAssetRegistryExports Exports;
			FUtils::GetBlueprintParameters(Blueprint, Exports);

			FString TagValue;
			FAnimNextParameterProviderAssetRegistryExports::StaticStruct()->ExportText(TagValue, &Exports, nullptr, nullptr, PPF_None, nullptr);
			Context.AddTag(UObject::FAssetRegistryTag(UE::AnimNext::ExportsAnimNextAssetRegistryTag, TagValue, UObject::FAssetRegistryTag::TT_Hidden));
		}
	});

	RegisterParameterSourceType(FAnimNextParamUniversalObjectLocator::StaticStruct(), MakeShared<FObjectProxyType>());

	// Register the compilation log (hidden from the main log set, it is displayed in the workspace editor)
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions LogInitOptions;
	LogInitOptions.bShowInLogWindow = false;
	LogInitOptions.MaxPageCount = 10;
	MessageLogModule.RegisterLogListing("AnimNextCompilerResults", LOCTEXT("CompilerResults", "AnimNext Compiler Results"), LogInitOptions);
}

void FModule::ShutdownModule()
{
	if(FMessageLogModule* MessageLogModule = FModuleManager::GetModulePtr<FMessageLogModule>("MessageLog"))
	{
		MessageLogModule->UnregisterLogListing("AnimNextCompilerResults");
	}

	UnregisterParameterSourceType(FAnimNextParamUniversalObjectLocator::StaticStruct());

	UAnimNextSchedule::GetAssetRegistryTagsFunction = nullptr;
	UAnimNextSchedule::CompileFunction = nullptr;

	UObject::FAssetRegistryTag::OnGetExtraObjectTagsWithContext.Remove(OnGetExtraObjectTagsHandle);
}

void FModule::RegisterParameterSourceType(const UScriptStruct* InInstanceIdStruct, TSharedPtr<IParameterSourceType> InType)
{
	ParameterSourceTypes.Add(InInstanceIdStruct, InType);
}

void FModule::UnregisterParameterSourceType(const UScriptStruct* InInstanceIdStruct)
{
	ParameterSourceTypes.Remove(InInstanceIdStruct);
}

TSharedPtr<IParameterSourceType> FModule::FindParameterSourceType(const UScriptStruct* InInstanceIdStruct) const
{
	return ParameterSourceTypes.FindRef(InInstanceIdStruct);
}

}

#undef LOCTEXT_NAMESPACE 

IMPLEMENT_MODULE(UE::AnimNext::UncookedOnly::FModule, AnimNextUncookedOnly);
