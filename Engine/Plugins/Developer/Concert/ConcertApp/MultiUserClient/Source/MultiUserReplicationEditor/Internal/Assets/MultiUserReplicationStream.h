// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Replication/Data/ReplicationStreamDescription.h"
#include "Misc/Attribute.h"
#include "MultiUserReplicationStream.generated.h"

/** Wraps FObjectReplicationMap so its edition can be transacted in the editor. */
UCLASS()
class MULTIUSERREPLICATIONEDITOR_API UMultiUserReplicationStream : public UObject
{
	GENERATED_BODY()
public:

	/** The ID of the stream. Set to 0 for CDO to avoid issues with delta serialization.*/
	UPROPERTY()
	FGuid StreamId;

	/** The objects this stream will modify. */
	UPROPERTY()
	FObjectReplicationMap ReplicationMap;

	UMultiUserReplicationStream();
	
	FReplicationStreamDescription GenerateDescription() const;
	
	TAttribute<FObjectReplicationMap*> MakeReplicationMapGetterAttribute()
	{
		return TAttribute<FObjectReplicationMap*>::CreateLambda([WeakThis = TWeakObjectPtr<UMultiUserReplicationStream>(this)]()
		{
			return WeakThis.IsValid() ? &WeakThis->ReplicationMap : nullptr;
		});
	}
};
