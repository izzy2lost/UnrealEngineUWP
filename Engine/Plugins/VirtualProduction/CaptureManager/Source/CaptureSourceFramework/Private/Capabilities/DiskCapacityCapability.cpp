// Copyright Epic Games, Inc. All Rights Reserved.

#include "Capabilities/DiskCapacityCapability.h"

const FString FStateUpdatedEvent::Name = TEXT("StateUpdated");

const FString FUpdateCmd::Name = TEXT("Update");

FUpdateCmd::FUpdateCmd()
	: FCommandBase(Name)
{
}

const FString FDiskCapacityCapabilityBase::Name = TEXT("DiskCapacity");
const FString FDiskCapacityCapabilityBase::TotalSpace = TEXT("TotalSpace");
const FString FDiskCapacityCapabilityBase::RemainingSpace = TEXT("RemainingSpace");
const FString FDiskCapacityCapabilityBase::UsedSpace = TEXT("UsedSpace");

FDiskCapacityCapabilityBase::FDiskCapacityCapabilityBase()
	: FCaptureSourceCapability(Name)
{
	AddProperty(FPropertyDesc(TotalSpace, FPropertyDesc::EType::Number, FPropertyDesc::EAccess::ReadOnly));
	AddProperty(FPropertyDesc(RemainingSpace, FPropertyDesc::EType::Number, FPropertyDesc::EAccess::ReadOnly));
	AddProperty(FPropertyDesc(UsedSpace, FPropertyDesc::EType::Number, FPropertyDesc::EAccess::ReadOnly));

	AddCommand(FCommandDesc(FUpdateCmd::Name, {}));

	RegisterEvent(FStateUpdatedEvent::Name);
}

FDiskCapacityCapability::FDiskCapacityCapability()
{
}

uint64 FDiskCapacityCapability::GetTotalSpace()
{
	return GetPropertyValue(TotalSpace).Get<int64>();
}

uint64 FDiskCapacityCapability::GetRemainingSpace()
{
	return GetPropertyValue(RemainingSpace).Get<int64>();
}

uint64 FDiskCapacityCapability::GetUsedSpace()
{
	return GetPropertyValue(TotalSpace).Get<int64>() - GetPropertyValue(RemainingSpace).Get<int64>();
}

void FDiskCapacityCapability::Update()
{
	ExecuteCommand(MakeShared<FUpdateCmd>());
}

void FDiskCapacityCapability::SetTotalSpacePropertyHandler(FPropertyGetter InGetter)
{
	SetPropertyGetter(TotalSpace, MoveTemp(InGetter));
}

void FDiskCapacityCapability::SetRemainingSpacePropertyHandler(FPropertyGetter InGetter)
{
	SetPropertyGetter(RemainingSpace, MoveTemp(InGetter));
}

void FDiskCapacityCapability::SetUpdateCmdHandler(FCommandHandler InCommandHandler)
{
	SetCommandHandler(FUpdateCmd::Name, MoveTemp(InCommandHandler));
}

void FDiskCapacityCapability::PublishStateUpdatedEvent(uint64 InTotalBytes, uint64 InRemainingBytes)
{
	PublishEvent<FStateUpdatedEvent>(InTotalBytes, InRemainingBytes);
}