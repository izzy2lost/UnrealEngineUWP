// Copyright Epic Games, Inc. All Rights Reserved.

#include "Assets/MultiUserReplicationSessionPreset.h"

#include "ConcertLogGlobal.h"
#include "ConcertMessageData.h"

UMultiUserReplicationClientContent* UMultiUserReplicationSessionPreset::GetClientContent(const FConcertClientInfo& ClientInfo) const
{
	const auto IsMatch = [&ClientInfo](UMultiUserReplicationClientContent* Content)
	{
		return Content && Content->DisplayName == ClientInfo.DisplayName;
	};
	const auto IsPerfectMatch = [&ClientInfo, &IsMatch](UMultiUserReplicationClientContent* Content)
	{
		return IsMatch(Content) && Content->DeviceName == ClientInfo.DeviceName;
	};
	
	UMultiUserReplicationClientContent* BestMatch = nullptr;
	bool bIsBestMatchPerfect = false;
	for (UMultiUserReplicationClientContent* Content : ClientPresets)
	{
		if (IsPerfectMatch(Content))
		{
			UE_CLOG(bIsBestMatchPerfect, LogConcert, Warning, TEXT("Preset %s contained client (name: %s, device: %s) multiple times"), *GetPathName(), *ClientInfo.DisplayName, *ClientInfo.DeviceName);
			bIsBestMatchPerfect = true;
			BestMatch = Content;
		}
		else if (!bIsBestMatchPerfect && IsMatch(Content))
		{
			BestMatch = Content;
		}
	}
	
	return BestMatch;
}

UMultiUserReplicationClientContent* UMultiUserReplicationSessionPreset::GetExactClientContent(const FConcertClientInfo& ClientInfo) const
{
	const TObjectPtr<UMultiUserReplicationClientContent>* Result = ClientPresets.FindByPredicate([&ClientInfo](const TObjectPtr<UMultiUserReplicationClientContent>& Content)
	{
		return Content && Content->DisplayName == ClientInfo.DisplayName && Content->DeviceName == ClientInfo.DeviceName;
	});
	return Result ? *Result : nullptr;
}

UMultiUserReplicationClientContent* UMultiUserReplicationSessionPreset::AddClientIfUnique(const FConcertClientInfo& ClientInfo, const FGuid& StreamId)
{
	if (ContainsExactClient(ClientInfo))
	{
		return nullptr;
	}
	
	UMultiUserReplicationClientContent* Result = NewObject<UMultiUserReplicationClientContent>(this);
	Result->DisplayName = ClientInfo.DisplayName;
	Result->DeviceName = ClientInfo.DeviceName;
	Result->Stream->StreamId = StreamId;
	ClientPresets.Add(Result);
	return Result;
}
