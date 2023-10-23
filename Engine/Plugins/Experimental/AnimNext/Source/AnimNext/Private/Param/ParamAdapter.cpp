// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParamAdapter.h"

#include "Param/ParamUtils.h"
#include "Scheduler/AnimNextSchedulerEntry.h"
#include "Scheduler/ScheduleContext.h"

namespace UE::AnimNext
{

FParamResult FParamAdapter::GetParamData(FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility) const
{
	OutParamTypeHandle = Definition.TypeHandle;

	const FParamCompatibility Compatibility = FParamUtils::GetCompatibility(InTypeHandle, OutParamTypeHandle);
	if (Compatibility < InRequiredCompatibility)
	{
		return EParamResult::TypeError;
	}

	UObject* ContextObject = FScheduleContext::Get().Entry->ResolvedObject;
	if(const uint8* Data = Function(ContextObject, Definition.Id))
	{
		OutParamData = TConstArrayView<uint8>(Data, Definition.TypeHandle.GetSize());
	}
	else
	{
		return EParamResult::NotInScope;
	}

	if (Compatibility == InRequiredCompatibility)
	{
		return EParamResult::Success | EParamResult::TypeCompatible;
	}

	return EParamResult::Success;
}

FParamResult FParamAdapter::GetMutableParamData(FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility) const
{
	if(!EnumHasAnyFlags(Definition.Flags, EParamDefinitionFlags::Mutable))
	{
		return EParamResult::MutabilityError;
	}

	OutParamTypeHandle = Definition.TypeHandle;

	const FParamCompatibility Compatibility = FParamUtils::GetCompatibility(InTypeHandle, OutParamTypeHandle);
	if (Compatibility < InRequiredCompatibility)
	{
		return EParamResult::TypeError;
	}

	UObject* ContextObject = FScheduleContext::Get().Entry->ResolvedObject;
	if(uint8* Data = Function(ContextObject, Definition.Id))
	{
		OutParamData = TArrayView<uint8>(Data, Definition.TypeHandle.GetSize());
	}
	else
	{
		return EParamResult::NotInScope;
	}

	if (Compatibility == InRequiredCompatibility)
	{
		return EParamResult::Success | EParamResult::TypeCompatible;
	}

	return EParamResult::Success;
}

}
