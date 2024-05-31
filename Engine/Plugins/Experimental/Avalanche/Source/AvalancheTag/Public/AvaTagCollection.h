// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTag.h"
#include "AvaTagId.h"
#include "Containers/Map.h"
#include "UObject/Object.h"
#include "AvaTagCollection.generated.h"

/**
 * Tag Collection that identifies a tag with an underlying Tag Id Guid
 * and provides Tag reference capabilities
 */
UCLASS(MinimalAPI)
class UAvaTagCollection : public UObject
{
	GENERATED_BODY()

public:
	AVALANCHETAG_API const FAvaTag* GetTag(const FAvaTagId& InTagId) const;

	/** Returns the keys of the Tag Map */
	AVALANCHETAG_API TArray<FAvaTagId> GetTagIds() const;

	AVALANCHETAG_API static FName GetTagMapName();

private:
	UPROPERTY(EditAnywhere, Category="Tag")
	TMap<FAvaTagId, FAvaTag> Tags;
};
