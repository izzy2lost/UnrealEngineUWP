// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShallowWaterSettings.h"

UShallowWaterSettings::UShallowWaterSettings()
{

	
	DefaultShallowWaterNiagaraSimulation = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/NiagaraFluids/Templates/Liquid/2D/Systems/ShallowWater/Grid2D_SW_FollowPlayer.Grid2D_SW_FollowPlayer")));

}

FName UShallowWaterSettings::GetCategoryName() const
{
	return FName("Plugins");
}
