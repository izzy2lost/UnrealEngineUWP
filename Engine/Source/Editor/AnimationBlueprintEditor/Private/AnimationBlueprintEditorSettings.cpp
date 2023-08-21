// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimationBlueprintEditorSettings.h"
#include "BlueprintActionDatabase.h"

void UAnimationBlueprintEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FProperty* ChangedProperty = PropertyChangedEvent.Property;
	if (ChangedProperty)
	{
		// Refresh the action database to add/remove asset names from corresponding asset player's blueprint context actions.
		if (ChangedProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimationBlueprintEditorSettings, bShowAssetsInBlueprintContextMenu))
		{
			FBlueprintActionDatabase::Get().RefreshAll();
		}
	}

	OnSettingsChange.Broadcast(this, PropertyChangedEvent.ChangeType);
}