// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDCharacterConstraintsVisualizationSettings.h"

void UChaosVDCharacterConstraintsVisualizationSettings::SetCharacterGroundConstraintDataVisualizationFlags(EChaosVDCharacterGroundConstraintDataVisualizationFlags NewFlags)
{
	if (UChaosVDCharacterConstraintsVisualizationSettings* Settings = GetMutableDefault<UChaosVDCharacterConstraintsVisualizationSettings>())
	{
		Settings->GlobalCharacterGroundConstraintDataVisualizationFlags = NewFlags;
		Settings->BroadcastSettingsChanged(Settings);
	}
}

EChaosVDCharacterGroundConstraintDataVisualizationFlags UChaosVDCharacterConstraintsVisualizationSettings::GetCharacterGroundConstraintDataVisualizationFlags()
{
	if (UChaosVDCharacterConstraintsVisualizationSettings* Settings = GetMutableDefault<UChaosVDCharacterConstraintsVisualizationSettings>())
	{
		return Settings->GlobalCharacterGroundConstraintDataVisualizationFlags;
	}

	return EChaosVDCharacterGroundConstraintDataVisualizationFlags::None;
}
