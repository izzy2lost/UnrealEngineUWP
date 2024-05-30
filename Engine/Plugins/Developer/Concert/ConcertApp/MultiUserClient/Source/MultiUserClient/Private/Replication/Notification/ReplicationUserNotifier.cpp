// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationUserNotifier.h"

namespace UE::MultiUserClient
{
	FReplicationUserNotifier::FReplicationUserNotifier(FReplicationClientManager& InReplicationClientManager, FMuteStateManager& InMuteManager)
		: SubmissionNotifier(InReplicationClientManager)
		, MutingNotifier(InMuteManager)
	{}
}
