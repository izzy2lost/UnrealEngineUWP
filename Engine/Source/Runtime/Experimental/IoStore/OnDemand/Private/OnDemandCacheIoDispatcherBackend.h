// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IO/IoDispatcherBackend.h"
#include "Templates/SharedPointer.h"

namespace UE::IoStore
{

class IIoCache;
class FOnDemandIoStore;

class IOnDemandCacheIoDispatcherBackend
	: public IIoDispatcherBackend
{
public:
	virtual ~IOnDemandCacheIoDispatcherBackend() = default;
};

TSharedPtr<IOnDemandCacheIoDispatcherBackend> MakeOnDemandCacheIoDispatcherBackend(FOnDemandIoStore& IoStore, IIoCache& Cache);

} // namespace UE::IoStore
