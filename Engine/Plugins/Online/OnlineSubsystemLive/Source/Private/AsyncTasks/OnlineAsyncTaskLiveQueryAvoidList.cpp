// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveQueryAvoidList.h"
#include "OnlineSubsystemLive.h"

using Microsoft::Xbox::Services::XboxLiveContext;
using Windows::Foundation::Collections::IVectorView;

FOnlineAsyncTaskLiveQueryAvoidList::FOnlineAsyncTaskLiveQueryAvoidList(FOnlineSubsystemLive* const InLiveInterface, XboxLiveContext^ InLiveContext, const FUniqueNetIdLive& InUserIdLive)
	: FOnlineAsyncTaskConcurrencyLive(InLiveInterface, InLiveContext)
	, UserIdLive(InUserIdLive)
{
}

Windows::Foundation::IAsyncOperation<IVectorView<Platform::String^>^>^ FOnlineAsyncTaskLiveQueryAvoidList::CreateOperation()
{
	try
	{
		auto AsyncTask = LiveContext->PrivacyService->GetAvoidListAsync();
		return AsyncTask;
	}
	catch (Platform::COMException^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("Error querying block list, error: (0x%0.8X) %ls."), Ex->HResult, Ex->ToString()->Data());
	}

	return nullptr;
}

bool FOnlineAsyncTaskLiveQueryAvoidList::ProcessResult(const Concurrency::task<IVectorView<Platform::String^>^>& CompletedTask)
{
	try
	{
		IVectorView<Platform::String^>^ XUIDVector = CompletedTask.get();

		const int32 VectorSize = XUIDVector->Size;
		for (int32 Index = 0; Index < VectorSize; ++Index)
		{
			Platform::String^ XUID = XUIDVector->GetAt(Index);
			AvoidList.Emplace(MakeShared<FOnlineBlockedPlayerLive>(XUID));
		}
	}
	catch (Platform::Exception^ Ex)
	{
		UE_LOG_ONLINE(Error, TEXT("Error querying block list, error: (0x%0.8X) %ls."), Ex->HResult, Ex->ToString()->Data());
		return false;
	}

	return true;
}

void FOnlineAsyncTaskLiveQueryAvoidList::Finalize()
{
	if (bWasSuccessful)
	{
		Subsystem->GetFriendsLive()->AvoidListMap.Emplace(UserIdLive, MoveTemp(AvoidList));
	}
}

void FOnlineAsyncTaskLiveQueryAvoidList::TriggerDelegates()
{
	Subsystem->GetFriendsLive()->TriggerOnQueryBlockedPlayersCompleteDelegates(UserIdLive, bWasSuccessful, OutError);
}
