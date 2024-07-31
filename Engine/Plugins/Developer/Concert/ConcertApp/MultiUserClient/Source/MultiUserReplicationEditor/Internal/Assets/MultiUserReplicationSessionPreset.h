// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MultiUserReplicationClientContent.h"
#include "Replication/Messages/Muting.h"
#include "UObject/Object.h"
#include "MultiUserReplicationSessionPreset.generated.h"

struct FConcertClientInfo;

USTRUCT()
struct FMultiUserMuteSessionContent
{
	GENERATED_BODY()

	/** The argument to put into FConcertReplication_ChangeMuteState_Request::ObjectsToMute. */
	UPROPERTY()
	TMap<FSoftObjectPath, FConcertReplication_ObjectMuteSetting> MutedObjects;

	/** The argument to put into FConcertReplication_ChangeMuteState_Request::ObjectsToUnmute. */
	UPROPERTY()
	TMap<FSoftObjectPath, FConcertReplication_ObjectMuteSetting> UnmutedObjects;

	FMultiUserMuteSessionContent() = default;
	FMultiUserMuteSessionContent(
		TMap<FSoftObjectPath, FConcertReplication_ObjectMuteSetting> MutedObjects,
		TMap<FSoftObjectPath, FConcertReplication_ObjectMuteSetting> UnmutedObjects
		)
		: MutedObjects(MoveTemp(MutedObjects))
		, UnmutedObjects(MoveTemp(UnmutedObjects))
	{}
};

/** Stores per-client replication settings so it can be loaded by a user to quickly set up a session. */
UCLASS()
class MULTIUSERREPLICATIONEDITOR_API UMultiUserReplicationSessionPreset : public UObject
{
	GENERATED_BODY()
public:

	/********** Clients **********/

	/** @return The client preset that matches ClientInfo.DisplayName. If there are multiple, returns the one that matches ClientInfo.DeviceName, as well. */
	UMultiUserReplicationClientContent* GetClientContent(const FConcertClientInfo& ClientInfo) const;
	/** @return The client preset that matches both the display and device name. */
	UMultiUserReplicationClientContent* GetExactClientContent(const FConcertClientInfo& ClientInfo) const;
	
	/** @return Whether a client that matches ClientInfo.DisplayName. */
	bool ContainsClient(const FConcertClientInfo& ClientInfo) const { return GetClientContent(ClientInfo) != nullptr; }
	/** @return Whether a client that matches both the display and device name is saved in this preset. */
	bool ContainsExactClient(const FConcertClientInfo& ClientInfo) const { return GetExactClientContent(ClientInfo) != nullptr; }
	
	/** Adds a client to the preset if it's not already present. */
	UMultiUserReplicationClientContent* AddClientIfUnique(const FConcertClientInfo& ClientInfo, const FGuid& StreamId);
	
	const TArray<TObjectPtr<UMultiUserReplicationClientContent>>& GetClientPresets() const { return ClientPresets; }
	
	/********** Muting **********/
	
	const FMultiUserMuteSessionContent& GetMuteContent() const { return MuteContent; }
	void SetMuteContent(FMultiUserMuteSessionContent Content) { MuteContent = MoveTemp(Content); }
	
private:
	
	UPROPERTY(Instanced)
	TArray<TObjectPtr<UMultiUserReplicationClientContent>> ClientPresets;

	UPROPERTY()
	FMultiUserMuteSessionContent MuteContent;
};
