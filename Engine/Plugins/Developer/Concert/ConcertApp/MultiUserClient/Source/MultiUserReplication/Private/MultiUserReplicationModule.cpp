// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiUserReplicationModule.h"

#include "Modules/ModuleManager.h"

namespace UE::MultiUserReplication::Private
{
	void FMultiUserReplicationModule::StartupModule()
	{
		
	}

	void FMultiUserReplicationModule::ShutdownModule()
	{
		
	}
};

IMPLEMENT_MODULE(UE::MultiUserReplication::Private::FMultiUserReplicationModule, MultiUserReplication);
