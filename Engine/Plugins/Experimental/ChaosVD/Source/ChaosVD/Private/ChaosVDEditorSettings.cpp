// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDEditorSettings.h"

void UChaosVDEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UChaosVDEditorSettings, GeometryVisibilityFlags))
	{
		VisibilitySettingsChangedDelegate.Broadcast(this);
	}
}
