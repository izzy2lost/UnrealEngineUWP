// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveUpdateSessionMember.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineSessionInterfaceLive.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"


FOnlineAsyncTaskLiveUpdateSessionMember::FOnlineAsyncTaskLiveUpdateSessionMember(
	FName InSessionName,
	Microsoft::Xbox::Services::XboxLiveContext^ InContext,
	FOnlineSubsystemLive* InSubsystem,
	int RetryCount)
	: FOnlineAsyncTaskLiveSafeWriteSession(InSessionName, InContext, InSubsystem, RetryCount)
	, SessionIdentifier(InSessionName)
{
	
}

FOnlineAsyncTaskLiveUpdateSessionMember::FOnlineAsyncTaskLiveUpdateSessionMember(
	FName InSessionName,
	Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference^ InSessionReference,
	Microsoft::Xbox::Services::XboxLiveContext^ InContext,
	FOnlineSubsystemLive* InSubsystem,
	int RetryCount)
	: FOnlineAsyncTaskLiveSafeWriteSession(InSessionName, InSessionReference, InContext, InSubsystem, RetryCount)
    , SessionIdentifier(InSessionName)
{
}

bool FOnlineAsyncTaskLiveUpdateSessionMember::UpdateSession(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession^ Session)
{
	auto SessionInterface = Subsystem->GetSessionInterface();
	if (SessionInterface.IsValid() && Session->CurrentUser)
	{
		if (auto NamedSession = SessionInterface->GetNamedSession(GetSessionName()))
		{
			FString Key = FString::Printf(TEXT("%s%s"), SETTING_GROUP_NAME_PREFIX, Session->CurrentUser->XboxUserId->Data());
			FString Setting;
			if (NamedSession->SessionSettings.Get(FName(*Key), Setting))
			{
				Session->CurrentUser->Groups = ref new Platform::Collections::Vector<Platform::String^>;
				Session->CurrentUser->Groups->Append(ref new Platform::String(*Setting));
			}

			Key = FString::Printf(TEXT("%s%s"), SETTING_SESSION_MEMBER_CONSTANT_CUSTOM_JSON_XUID_PREFIX, Session->CurrentUser->XboxUserId->Data());
			if (NamedSession->SessionSettings.Get(FName(*Key), Setting))
			{
				TSharedPtr< FJsonObject > JObj;
				TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create(Setting);

				if (FJsonSerializer::Deserialize(Reader, JObj) && JObj.IsValid())
				{
					for (auto it = JObj->Values.CreateConstIterator(); it; ++it)
					{
						const FString					JSettingName = it.Key();
						const TSharedPtr<FJsonValue>	JSettingValue = it.Value();

						Session->SetCurrentUserMemberCustomPropertyJson(ref new Platform::String(*JSettingName), ref new Platform::String(*JSettingValue->AsString()));
					}
				}
			}
		}

		return true;
	}

	return false;
}


void FOnlineAsyncTaskLiveUpdateSessionMember::TriggerDelegates()
{
	auto SessionInterface = Subsystem->GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->TriggerOnUpdateSessionCompleteDelegates(GetSessionName(), WasSuccessful());
	}
}