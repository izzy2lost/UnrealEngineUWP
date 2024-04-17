// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IO/IoStatus.h"
#include "IO/PackageStore.h"
#include "Templates/SharedPointer.h"

struct FIoContainerHeader;

namespace UE::IoStore
{

class IOnDemandPackageStoreBackend
	: public IPackageStoreBackend
{
public:
	virtual ~IOnDemandPackageStoreBackend() = default;

	virtual FIoStatus Mount(FString ContainerName, FIoContainerHeader&& ContainerHeader) = 0;
	virtual FIoStatus Unmount(const FString& ContainerName) = 0;
};

TSharedPtr<IOnDemandPackageStoreBackend> MakeOnDemandPackageStoreBackend();

} // namespace UE
