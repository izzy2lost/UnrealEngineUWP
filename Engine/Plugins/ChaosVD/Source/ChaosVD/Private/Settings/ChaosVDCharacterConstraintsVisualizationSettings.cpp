// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDCharacterConstraintsVisualizationSettings.h"

void UChaosVDCharacterConstraintsVisualizationSettings::SetDataVisualizationFlags(EChaosVDCharacterGroundConstraintDataVisualizationFlags NewFlags)
{
	if (UChaosVDCharacterConstraintsVisualizationSettings* Settings = GetMutableDefault<UChaosVDCharacterConstraintsVisualizationSettings>())
	{
		Settings->GlobalCharacterGroundConstraintDataVisualizationFlags = NewFlags;
		Settings->BroadcastSettingsChanged();
	}
}

EChaosVDCharacterGroundConstraintDataVisualizationFlags UChaosVDCharacterConstraintsVisualizationSettings::GetDataVisualizationFlags()
{
	if (UChaosVDCharacterConstraintsVisualizationSettings* Settings = GetMutableDefault<UChaosVDCharacterConstraintsVisualizationSettings>())
	{
		return Settings->GlobalCharacterGroundConstraintDataVisualizationFlags;
	}

	return EChaosVDCharacterGroundConstraintDataVisualizationFlags::None;
}
