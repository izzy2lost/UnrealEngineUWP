// Copyright Epic Games, Inc. All Rights Reserved.


#include "StormSyncAvaBridgeUtils.h"

#include "AvalancheBroadcast.h"

TArray<FString> FStormSyncAvaBridgeUtils::GetServerNamesForChannel(const FString& InChannelName)
{
	TArray<FString> ServerNames;
	
	if (UAvalancheBroadcast* Broadcast = UAvalancheBroadcast::GetAvalancheBroadcast())
	{
		const FAvaOutputChannel Channel = Broadcast->GetCurrentProfile().GetChannel(FName(*InChannelName));
		if (Channel.IsValidChannel())
		{
			TArray<UMediaOutput*> Outputs = Channel.GetMediaOutputs();
			for (const UMediaOutput* Output : Outputs)
			{
				FAvaMediaOutputInfo OutputInfo = Channel.GetMediaOutputInfo(Output);
				if (OutputInfo.IsValid() && OutputInfo.IsRemote())
				{
					ServerNames.Add(OutputInfo.ServerName);
				}
			}
		}
	}

	return ServerNames;
}
