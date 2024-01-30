// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSceneState.h"
#include "AvaSceneSettings.h"
#include "AvaTagHandle.h"

void UAvaSceneState::OnBeginPlay(UAvaSceneSettings* InSceneSettings)
{
	SceneSettingsWeak = InSceneSettings;
}

bool UAvaSceneState::AddTagAttribute(const FAvaTagHandle& InTagHandle)
{
	if (const FAvaTag* Tag = InTagHandle.GetTag())
	{
		bool bAlreadyInSet;
		ActiveTagAttributes.Add(*Tag, &bAlreadyInSet);
		return !bAlreadyInSet;
	}
	return false;
}

bool UAvaSceneState::RemoveTagAttribute(const FAvaTagHandle& InTagHandle)
{
	if (const FAvaTag* Tag = InTagHandle.GetTag())
	{
		return ActiveTagAttributes.Remove(*Tag) > 0;
	}
	return false;
}

bool UAvaSceneState::ContainsTagAttribute(const FAvaTagHandle& InTagHandle) const
{
	const FAvaTag* Tag = InTagHandle.GetTag();
	if (!Tag)
	{
		return false;
	}

	if (ActiveTagAttributes.Contains(*Tag))
	{
		return true;
	}

	UAvaSceneSettings* SceneSettings = SceneSettingsWeak.Get();
	return SceneSettings && SceneSettings->GetTagAttributes().ContainsTag(InTagHandle);
}
