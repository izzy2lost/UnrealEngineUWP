// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTagLibrary.h"
#include "AvaTagHandle.h"
#include "AvaTagHandleContainer.h"
#include "AvaTagSoftHandle.h"

FAvaTag UAvaTagLibrary::ResolveTagHandle(const FAvaTagHandle& InTagHandle)
{
	if (const FAvaTag* Tag = InTagHandle.GetTag())
	{
		return *Tag;
	}
	return FAvaTag();
}

TArray<FAvaTag> UAvaTagLibrary::ResolveTagHandles(const FAvaTagHandleContainer& InTagHandleContainer)
{
	return InTagHandleContainer.ResolveTags();
}

FAvaTagHandle UAvaTagLibrary::ResolveTagSoftHandle(const FAvaTagSoftHandle& InTagSoftHandle)
{
	return InTagSoftHandle.MakeTagHandle();
}
