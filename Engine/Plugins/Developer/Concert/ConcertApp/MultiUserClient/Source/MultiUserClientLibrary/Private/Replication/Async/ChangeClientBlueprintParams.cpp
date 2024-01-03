// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Async/ChangeClientBlueprintParams.h"

namespace UE::MultiUserClientLibrary
{
#if WITH_CONCERT
	EMultiUserChangeStreamOperationResult Transform(MultiUserClient::EChangeStreamOperationResult Data)
	{
		static_assert(static_cast<int32>(MultiUserClient::EChangeStreamOperationResult::Count) == 9, "Update EMultiUserChangeStreamOperationResult to have the equivalent enum entry you added to EChangeStreamOperationResult");
		return static_cast<EMultiUserChangeStreamOperationResult>(Data);
	}
	EMultiUserChangeAuthorityOperationResult Transform(MultiUserClient::EChangeAuthorityOperationResult Data)
	{
		static_assert(static_cast<int32>(MultiUserClient::EChangeAuthorityOperationResult::Count) == 10, "Update EMultiUserChangeStreamOperationResult to have the equivalent enum entry you added to EChangeStreamOperationResult");
		return static_cast<EMultiUserChangeAuthorityOperationResult>(Data);
	}
	FMultiUserChangeClientReplicationResult Transform(MultiUserClient::FChangeClientReplicationResult Data)
	{
		return { Transform(Data.StreamChangeResult), Transform(Data.AuthorityChangeResult) };
	}
	
	MultiUserClient::EPropertyChangeType Transform(EMultiUserPropertyChangeType Data)
	{
		static_assert(static_cast<int32>(MultiUserClient::EPropertyChangeType::Count) == 3, "Update this location if you changed the enum");
		return static_cast<MultiUserClient::EPropertyChangeType>(Data);
	}
	MultiUserClient::FPropertyChange Transform(FMultiUserPropertyChange Data)
	{
		MultiUserClient::FPropertyChange Result { .ChangeType =  Transform(MoveTemp(Data.ChangeType)) };
		
		if (!Data.Properties.IsEmpty())
		{
			Result.Properties.Reserve(Data.Properties.Num());
			for (FConcertPropertyChainWrapper& Wrapper : Data.Properties)
			{
				Result.Properties.Emplace(MoveTemp(Wrapper.PropertyChain));
			}
		}
		
		return Result;
	}
	
	MultiUserClient::FChangeStreamRequest Transform(FMultiUserChangeStreamRequest Data)
	{
		MultiUserClient::FChangeStreamRequest Result { .ObjectsToRemove = MoveTemp(Data.ObjectsToRemove) };
		
		if (!Data.PropertyChanges.IsEmpty())
		{
			Result.PropertyChanges.Reserve(Data.PropertyChanges.Num());
			for (TPair<UObject*, FMultiUserPropertyChange>& Change : Data.PropertyChanges)
			{
				Result.PropertyChanges.Emplace(Change.Key, Transform(MoveTemp(Change.Value)));
			}
		}
		
		return Result;
	}
	MultiUserClient::FChangeAuthorityRequest Transform(FMultiUserChangeAuthorityRequest Data)
	{
		return { MoveTemp(Data.ObjectsToStartReplicating), MoveTemp(Data.ObjectToStopReplicating) };
	}
	MultiUserClient::FChangeClientReplicationRequest Transform(FMultiUserChangeClientReplicationRequest Data)
	{
		return {
			Data.StreamChangeRequest.IsEmpty() ? TOptional<MultiUserClient::FChangeStreamRequest>{} : Transform(MoveTemp(Data.StreamChangeRequest)),
			Data.AuthorityChangeRequest.IsEmpty() ? TOptional<MultiUserClient::FChangeAuthorityRequest>{} : Transform(MoveTemp(Data.AuthorityChangeRequest)),
		};
	}
#endif
}