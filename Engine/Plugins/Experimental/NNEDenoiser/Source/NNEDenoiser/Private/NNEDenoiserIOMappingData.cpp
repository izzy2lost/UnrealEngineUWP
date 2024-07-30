// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEDenoiserIOMappingData.h"

namespace UE::NNEDenoiser
{
	
EResourceName ToResourceName(EInputResourceName Name)
{
	switch(Name)
	{
		case EInputResourceName::I_Color: return EResourceName::Color;
		case EInputResourceName::I_Albedo: return EResourceName::Albedo;
		case EInputResourceName::I_Normal: return EResourceName::Normal;
		case EInputResourceName::I_Output: return EResourceName::Output;
		default: checkNoEntry();
	}
	return EResourceName::Color;
}

EResourceName ToResourceName(EOutputResourceName Name)
{
	switch(Name)
	{
		case EOutputResourceName::O_Output: return EResourceName::Output;
		default: checkNoEntry();
	}
	return EResourceName::Color;
}

EResourceName ToResourceName(ETemporalInputResourceName Name)
{
	switch(Name)
	{
		case ETemporalInputResourceName::TI_Color: return EResourceName::Color;
		case ETemporalInputResourceName::TI_Albedo: return EResourceName::Albedo;
		case ETemporalInputResourceName::TI_Normal: return EResourceName::Normal;
		case ETemporalInputResourceName::TI_Flow: return EResourceName::Flow;
		case ETemporalInputResourceName::TI_Output: return EResourceName::Output;
		default: checkNoEntry();
	}
	return EResourceName::Color;
}

EResourceName ToResourceName(ETemporalOutputResourceName Name)
{
	switch(Name)
	{
		case ETemporalOutputResourceName::TO_Output: return EResourceName::Output;
		default: checkNoEntry();
	}
	return EResourceName::Color;
}

} // namespace UE::NNEDenoiser