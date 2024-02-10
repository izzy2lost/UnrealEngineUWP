// Copyright Epic Games, Inc. All Rights Reserved.

#include "Attributes/AvaSceneTagAttribute.h"

bool UAvaSceneTagAttribute::ContainsTag(const FAvaTagHandle& InTagHandle) const
{
	return Tag.MatchesTag(InTagHandle);
}

bool UAvaSceneTagContainerAttribute::ContainsTag(const FAvaTagHandle& InTagHandle) const
{
	return TagContainer.ContainsTag(InTagHandle);
}
