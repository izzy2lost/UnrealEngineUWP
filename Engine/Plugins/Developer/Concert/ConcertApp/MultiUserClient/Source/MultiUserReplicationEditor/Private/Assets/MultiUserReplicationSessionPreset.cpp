// Copyright Epic Games, Inc. All Rights Reserved.

#include "Assets/MultiUserReplicationSessionPreset.h"

UMultiUserReplicationClientContent* UMultiUserReplicationSessionPreset::AddClient()
{
	UMultiUserReplicationClientContent* Result = NewObject<UMultiUserReplicationClientContent>(this);
	ClientPresets.Add(Result);
	return Result;
}

void UMultiUserReplicationSessionPreset::RemoveClient(UMultiUserReplicationClientContent& Client)
{
	ClientPresets.RemoveSingle(&Client);
}
