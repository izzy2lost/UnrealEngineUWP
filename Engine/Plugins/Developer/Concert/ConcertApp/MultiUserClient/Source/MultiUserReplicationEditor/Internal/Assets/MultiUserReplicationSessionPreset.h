// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MultiUserReplicationClientContent.h"
#include "UObject/Object.h"
#include "MultiUserReplicationSessionPreset.generated.h"

/**
 * 
 */
UCLASS()
class MULTIUSERREPLICATIONEDITOR_API UMultiUserReplicationSessionPreset : public UObject
{
	GENERATED_BODY()
public:

	UMultiUserReplicationClientContent* AddClient();
	void RemoveClient(UMultiUserReplicationClientContent& Client);

	const TArray<TObjectPtr<UMultiUserReplicationClientContent>>& GetClientPresets() const { return ClientPresets; }

private:
	
	UPROPERTY(Instanced)
	TArray<TObjectPtr<UMultiUserReplicationClientContent>> ClientPresets;
};
