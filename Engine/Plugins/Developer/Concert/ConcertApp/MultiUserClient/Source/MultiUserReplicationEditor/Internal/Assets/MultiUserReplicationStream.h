// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Replication/Data/ReplicationStream.h"
#include "Misc/Attribute.h"
#include "MultiUserReplicationStream.generated.h"

/** Wraps FConcertObjectReplicationMap so its edition can be transacted in the editor and saved in presets. */
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
	FConcertObjectReplicationMap ReplicationMap;
	
	/**
	 * The frequency setting the stream has.
	 *
	 * TODO UE-219834:
	 * Currently, this is only written to by the preset system.
	 * In the future, FFrequencyChangeTracker could / should use this so changes to frequencies can be transacted, as well.
	 */
	UPROPERTY()
	FConcertStreamFrequencySettings FrequencySettings;

	UMultiUserReplicationStream();

	/** Util that generates the description of this stream for network requests. */
	FConcertReplicationStream GenerateDescription() const;

	/** Util that returns ReplicationMap. */
	TAttribute<FConcertObjectReplicationMap*> MakeReplicationMapGetterAttribute()
	{
		return TAttribute<FConcertObjectReplicationMap*>::CreateLambda([WeakThis = TWeakObjectPtr<UMultiUserReplicationStream>(this)]()
		{
			return WeakThis.IsValid() ? &WeakThis->ReplicationMap : nullptr;
		});
	}
	
	/** Copies the stream content of OtherStream. */
	void Copy(UMultiUserReplicationStream& OtherStream);
};
