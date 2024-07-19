// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MultiUserReplicationClientContent.h"
#include "UObject/Object.h"
#include "MultiUserReplicationSessionPreset.generated.h"

struct FConcertClientInfo;

/** Stores per-client replication settings so it can be loaded by a user to quickly set up a session. */
UCLASS()
class MULTIUSERREPLICATIONEDITOR_API UMultiUserReplicationSessionPreset : public UObject
{
	GENERATED_BODY()
public:

	/** @return The client preset that matches ClientInfo.DisplayName. If there are multiple, returns the one that matches ClientInfo.DeviceName, as well. */
	UMultiUserReplicationClientContent* GetClientContent(const FConcertClientInfo& ClientInfo) const;
	/** @return The client preset that matches both the display and device name. */
	UMultiUserReplicationClientContent* GetExactClientContent(const FConcertClientInfo& ClientInfo) const;
	
	/** @return Whether a client that matches ClientInfo.DisplayName. */
	bool ContainsClient(const FConcertClientInfo& ClientInfo) const { return GetClientContent(ClientInfo) != nullptr; }
	/** @return Whether a client that matches both the display and device name is saved in this preset. */
	bool ContainsExactClient(const FConcertClientInfo& ClientInfo) const { return GetExactClientContent(ClientInfo) != nullptr; }
	
	/** Adds a client to the preset if it's not already present. */
	UMultiUserReplicationClientContent* AddClientIfUnique(const FConcertClientInfo& ClientInfo);

	const TArray<TObjectPtr<UMultiUserReplicationClientContent>>& GetClientPresets() const { return ClientPresets; }

private:
	
	UPROPERTY(Instanced)
	TArray<TObjectPtr<UMultiUserReplicationClientContent>> ClientPresets;
};
