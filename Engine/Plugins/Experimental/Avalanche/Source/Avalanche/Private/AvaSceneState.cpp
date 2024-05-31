// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSceneState.h"
#include "AvaSceneSettings.h"
#include "AvaTagHandle.h"
#include "AvaTagHandleContainer.h"
#include "Tags/AvaTagAttribute.h"
#include "Tags/AvaTagAttributeBase.h"

void UAvaSceneState::Initialize(UAvaSceneSettings* InSceneSettings)
{
	SceneAttributes.Reset();
	if (InSceneSettings)
	{
		SceneAttributes = InSceneSettings->GetSceneAttributes();
	}
}

bool UAvaSceneState::AddTagAttribute(const FAvaTagHandle& InTagHandle)
{
	if (!InTagHandle.IsValid())
	{
		return false;
	}

	// Return true if already existing
	if (ContainsTagAttribute(InTagHandle))
	{
		return true;
	}

	UAvaTagAttribute* TagAttribute = NewObject<UAvaTagAttribute>(this, NAME_None, RF_Transient);
	check(TagAttribute);
	TagAttribute->Tag = InTagHandle;

	SceneAttributes.Add(TagAttribute);
	return true;
}

bool UAvaSceneState::RemoveTagAttribute(const FAvaTagHandle& InTagHandle)
{
	uint32 TagsCleared = 0;

	for (UAvaAttribute* Attribute : SceneAttributes)
	{
		UAvaTagAttributeBase* TagAttribute = Cast<UAvaTagAttributeBase>(Attribute);
		if (!TagAttribute)
		{
			continue;
		}

		// Attempt to clear the given tag handle for the attribute. This will return true if it did remove the entry
		// Do not remove the attribute itself from the list as it could still have valid tags, or later have valid tags
		if (TagAttribute->ClearTagHandle(InTagHandle))
		{
			++TagsCleared;
		}
	}

	return TagsCleared > 0;
}

bool UAvaSceneState::ContainsTagAttribute(const FAvaTagHandle& InTagHandle) const
{
	return SceneAttributes.ContainsByPredicate(
		[&InTagHandle](UAvaAttribute* InAttribute)
		{
			UAvaTagAttributeBase* TagAttribute = Cast<UAvaTagAttributeBase>(InAttribute);
			return TagAttribute && TagAttribute->ContainsTag(InTagHandle);
		});
}
