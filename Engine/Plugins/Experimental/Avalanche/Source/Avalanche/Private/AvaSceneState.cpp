// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSceneState.h"
#include "Attributes/AvaSceneTagAttribute.h"
#include "AvaSceneSettings.h"
#include "AvaTagHandle.h"

void UAvaSceneState::SetSceneSettings(UAvaSceneSettings* InSceneSettings)
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

	const UAvaSceneSettings* SceneSettings = SceneSettingsWeak.Get();
	if (!SceneSettings)
	{
		return false;
	}

	bool bFoundTag = false;

	SceneSettings->ForEachSceneAttributeOfType<UAvaSceneTagAttributeBase>(
		[&InTagHandle, &bFoundTag](const UAvaSceneTagAttributeBase& InTagAttribute)
		{
			bFoundTag = InTagAttribute.ContainsTag(InTagHandle);
			// continue iteration if tag not found
			return !bFoundTag;
		});

	return bFoundTag;
}
