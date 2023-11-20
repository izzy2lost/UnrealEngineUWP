// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureSourceCapability.h"

struct CAPTURESOURCEFRAMEWORK_API FStateUpdatedEvent : public FCaptureEvent
{
	static const FString Name;

	FStateUpdatedEvent(uint64 InTotalSpace,
					   uint64 InRemainingSpace)
		: FCaptureEvent(Name)
		, TotalSpace(InTotalSpace)
		, RemainingSpace(InRemainingSpace)
		, UsedSpace(InTotalSpace - InRemainingSpace)
	{
	}

	uint64 TotalSpace;
	uint64 RemainingSpace;
	uint64 UsedSpace;
};

class CAPTURESOURCEFRAMEWORK_API FUpdateCmd : public FCommandBase
{
public:

	static const FString Name;

	FUpdateCmd();
};

class CAPTURESOURCEFRAMEWORK_API FDiskCapacityCapabilityBase : public FCaptureSourceCapability
{
public:
	static const FString Name;
	static const FString TotalSpace;
	static const FString RemainingSpace;
	static const FString UsedSpace;

	FDiskCapacityCapabilityBase();
	virtual ~FDiskCapacityCapabilityBase() = default;

	virtual uint64 GetTotalSpace() = 0;
	virtual uint64 GetRemainingSpace() = 0;
	virtual uint64 GetUsedSpace() = 0;

	virtual void Update() = 0;
};

class CAPTURESOURCEFRAMEWORK_API FDiskCapacityCapability : public FDiskCapacityCapabilityBase
{
public:

	FDiskCapacityCapability();
	virtual ~FDiskCapacityCapability() = default;

	virtual uint64 GetTotalSpace() override;
	virtual uint64 GetRemainingSpace() override;
	virtual uint64 GetUsedSpace() override;

	virtual void Update() override;

	void SetTotalSpacePropertyHandler(FPropertyGetter InGetter);
	void SetRemainingSpacePropertyHandler(FPropertyGetter InGetter);
	void SetUpdateCmdHandler(FCommandHandler InCommandHandler);

	void PublishStateUpdatedEvent(uint64 InTotalBytes, uint64 InRemainingBytes);
};