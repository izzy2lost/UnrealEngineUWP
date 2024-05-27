// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMTextureSetFilter.h"

FDMTextureSetFilter::FDMTextureSetFilter()
	: FilterStrings({TEXT("_")})
	, MaterialProperties({{EMaterialProperty::MP_BaseColor, EDMTextureChannelMask::RGBA}})
{
}

bool FDMTextureSetFilter::MatchesFilter(const FString& InAssetName) const
{
	for (const FString& FilterString : FilterStrings)
	{
		if (FilterString.StartsWith(TEXT("_")))
		{
			if (InAssetName.EndsWith(FilterString))
			{
				return true;
			}
		}
		else if (InAssetName.Find(FilterString) != INDEX_NONE)
		{
			return true;
		}
	}

	return false;
}
