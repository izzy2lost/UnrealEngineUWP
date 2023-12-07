// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Foundation.h"

////////////////////////////////////////////////////////////////////////////////

class FLifetime
{
public:
						FLifetime(class FStoreService* Service);
	bool				ShouldKeepAlive();
	void				CheckNewSponsors(struct FInstanceInfo* InstanceInfo);
	void				AddPid(uint32);

private:
	bool				ShutdownStoreIfNoConnections();
	bool				IsAnySponsorActive();

	using FProcHandle = void*;

	TArray<FProcHandle> SponsorHandles;
	class FStoreService* StoreService;
};

