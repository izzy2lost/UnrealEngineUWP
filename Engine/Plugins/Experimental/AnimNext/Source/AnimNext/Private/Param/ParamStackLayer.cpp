// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParamStackLayer.h"
#include "Param/ParamEntry.h"
#include "Param/ParamResult.h"
#include "Param/ParamUtils.h"

namespace UE::AnimNext
{

FParamStackLayer::FParamStackLayer(TConstArrayView<TPair<FParamId, Private::FParamEntry>> InParams)
{
	MinParamId = MAX_uint32;
	uint32 MaxParamId = 0;
	for (uint32 ParamIndex = 0; ParamIndex < static_cast<uint32>(InParams.Num()); ++ParamIndex)
	{
		MinParamId = FMath::Min(InParams[ParamIndex].Key.ToInt(), MinParamId);
		MaxParamId = FMath::Max(InParams[ParamIndex].Key.ToInt(), MaxParamId);
	}

	if (MinParamId <= MaxParamId)
	{
		const uint32 ParamRangeSize = (MaxParamId - MinParamId) + 1;
		Params.SetNumZeroed(ParamRangeSize);
		for (uint32 ParamIndex = 0; ParamIndex < static_cast<uint32>(InParams.Num()); ++ParamIndex)
		{
			const TPair<FParamId, Private::FParamEntry>& Pair = InParams[ParamIndex];
			const uint32 LocalParamIndex = Pair.Key.ToInt() - MinParamId;
			Params[LocalParamIndex] = Pair.Value;
		}
	}
}


FParamResult FParamStackLayer::GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData) const
{
	const uint32 LocalParamIndex = InId.ToInt() - MinParamId;

	const Private::FParamEntry& Param = Params[LocalParamIndex];
	if (!Param.IsValid())
	{
		return EParamResult::NotInScope;
	}

	FParamCompatibility Compatibility = UE::AnimNext::FParamUtils::GetCompatibility(InTypeHandle, Param.GetTypeHandle());
	if (!Compatibility.IsCompatible())
	{
		return EParamResult::TypeError;
	}

	OutParamData = Param.GetData();
	return EParamResult::Success;
}

FParamResult FParamStackLayer::GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility) const
{
	const uint32 LocalParamIndex = InId.ToInt() - MinParamId;

	const Private::FParamEntry& Param = Params[LocalParamIndex];
	if (!Param.IsValid())
	{
		return EParamResult::NotInScope;
	}

	OutParamTypeHandle = Param.GetTypeHandle();

	FParamCompatibility Compatibility = UE::AnimNext::FParamUtils::GetCompatibility(InTypeHandle, OutParamTypeHandle);
	if (Compatibility < InRequiredCompatibility)
	{
		return EParamResult::TypeError;
	}

	OutParamData = Param.GetData();

	if (Compatibility == InRequiredCompatibility)
	{
		return EParamResult::Success | EParamResult::TypeCompatible;
	}

	return EParamResult::Success;
}

FParamResult FParamStackLayer::GetMutableParamData(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData)
{
	const uint32 LocalParamIndex = InId.ToInt() - MinParamId;

	Private::FParamEntry& Param = Params[LocalParamIndex];
	if (!Param.IsValid())
	{
		return EParamResult::NotInScope;
	}

	FParamResult AccessResult = EParamResult::Success;
	FParamCompatibility Compatibility = UE::AnimNext::FParamUtils::GetCompatibility(InTypeHandle, Param.GetTypeHandle());
	if (!Compatibility.IsCompatible())
	{
		AccessResult.Result |= EParamResult::TypeError;
	}

	AccessResult.Result |= !Param.IsMutable() ? EParamResult::MutabilityError : EParamResult::Success;
	if (AccessResult.IsSuccessful())
	{
		OutParamData = Param.GetMutableData();
	}

	return AccessResult;
}

FParamResult FParamStackLayer::GetMutableParamData(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility)
{
	const uint32 LocalParamIndex = InId.ToInt() - MinParamId;

	Private::FParamEntry& Param = Params[LocalParamIndex];
	if (!Param.IsValid())
	{
		return EParamResult::NotInScope;
	}

	OutParamTypeHandle = Param.GetTypeHandle();

	FParamResult AccessResult = EParamResult::Success;
	FParamCompatibility Compatibility = UE::AnimNext::FParamUtils::GetCompatibility(InTypeHandle, OutParamTypeHandle);
	if (Compatibility < InRequiredCompatibility)
	{
		AccessResult.Result |= EParamResult::TypeError;
	}
	else if (Compatibility == InRequiredCompatibility)
	{
		AccessResult.Result |= EParamResult::TypeCompatible;
	}

	AccessResult.Result |= !Param.IsMutable() ? EParamResult::MutabilityError : EParamResult::Success;
	if (AccessResult.IsSuccessful())
	{
		OutParamData = Param.GetMutableData();
	}

	return AccessResult;
}

}