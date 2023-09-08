// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFeaturePluginOperationResult.h"


namespace UE::GameFeatures
{
	FResult::FResult(FErrorCodeType ErrorCodeIn)
		: ErrorCode(MoveTemp(ErrorCodeIn))
		, OptionalErrorText()
	{
	}

	FResult::FResult(FErrorCodeType ErrorCodeIn, FText ErrorTextIn)
		: ErrorCode(MoveTemp(ErrorCodeIn))
		, OptionalErrorText(MoveTemp(ErrorTextIn))
	{
	}

	FString ToString(const FResult& Result)
	{
		TStringBuilder<512> Out;
		if (Result.HasValue())
		{
			Out << TEXT("Success");
		}
		else
		{
			Out << TEXT("ErrorCode=") << Result.GetError();
			if (!Result.OptionalErrorText.IsEmpty())
			{
				Out << TEXT(", ErrorText=") << Result.OptionalErrorText.ToString();
			}
		}
		return Out.ToString();
	}
}	// namespace UE::GameFeatures