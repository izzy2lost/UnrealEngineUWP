// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaSceneAttribute.h"
#include "AvaTagHandle.h"
#include "AvaTagHandleContainer.h"
#include "AvaSceneTagAttribute.generated.h"

/** Base implementation of Scene Attributes that represent a tag in some form */
UCLASS(MinimalAPI, Abstract)
class UAvaSceneTagAttributeBase : public UAvaSceneAttribute
{
	GENERATED_BODY()

public:
	virtual bool ContainsTag(const FAvaTagHandle& InTagHandle) const
	{
		return false;
	}
};

/** Scene Tag Attributes are scene attributes that hold a tag container handle */
UCLASS(MinimalAPI, DisplayName="Tag Attribute")
class UAvaSceneTagAttribute : public UAvaSceneTagAttributeBase
{
	GENERATED_BODY()

public:
	//~ Begin UAvaSceneTagAttributeBase
	virtual bool ContainsTag(const FAvaTagHandle& InTagHandle) const override;
	//~ End UAvaSceneTagAttributeBase

	UPROPERTY(EditAnywhere, Category="Attributes")
	FAvaTagHandle Tag;
};

/** Scene Tag Attributes are scene attributes that hold a tag container handle */
UCLASS(MinimalAPI, DisplayName="Tag Container Attribute")
class UAvaSceneTagContainerAttribute : public UAvaSceneTagAttributeBase
{
	GENERATED_BODY()

public:
	//~ Begin UAvaSceneTagAttributeBase
	virtual bool ContainsTag(const FAvaTagHandle& InTagHandle) const override;
	//~ End UAvaSceneTagAttributeBase

	UPROPERTY(EditAnywhere, Category="Attributes")
	FAvaTagHandleContainer TagContainer;
};
