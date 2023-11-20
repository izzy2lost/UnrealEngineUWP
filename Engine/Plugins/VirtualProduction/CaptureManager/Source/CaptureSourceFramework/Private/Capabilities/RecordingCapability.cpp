// Copyright Epic Games, Inc. All Rights Reserved.

#include "Capabilities/RecordingCapability.h"

const FString FRecordingEvent::Name = TEXT("IsRecording");

const FString FRecordingCapabilityBase::Name = TEXT("Recording");
const FString FRecordingCapabilityBase::SlateName = TEXT("SlateName");
const FString FRecordingCapabilityBase::TakeNumber = TEXT("TakeNumber");
const FString FRecordingCapabilityBase::StartRecordingCmd = TEXT("StartRecording");
const FString FRecordingCapabilityBase::StopRecordingCmd = TEXT("StopRecording");

FRecordingCapabilityBase::FRecordingCapabilityBase()
	: FCaptureSourceCapability(Name)
{
	AddCommand(FCommandDesc(StartRecordingCmd, { FPropertyDesc(SlateName, FPropertyDesc::EType::String), FPropertyDesc(TakeNumber, FPropertyDesc::EType::Number) }));
	AddCommand(FCommandDesc(StopRecordingCmd, {}));

	RegisterEvent(FRecordingEvent::Name);
}

FRecordingCapability::FRecordingCapability()
{
}

void FRecordingCapability::StartRecording(FString InSlateName, int64 InTakeNumber)
{
	TSharedPtr<FCommandBase> InCommand = MakeShared<FCommandBase>(StartRecordingCmd);
	InCommand->AddParamValue(SlateName, MakePropertyValue<FString>(MoveTemp(InSlateName)));
	InCommand->AddParamValue(TakeNumber, MakePropertyValue<int64>(InTakeNumber));

	ExecuteCommand(MoveTemp(InCommand));
}

void FRecordingCapability::StopRecording()
{
	ExecuteCommand(MakeShared<FCommandBase>(StopRecordingCmd));
}

void FRecordingCapability::SetStartRecordingHandler(FCommandHandler InCommandHandler)
{
	SetCommandHandler(StartRecordingCmd, MoveTemp(InCommandHandler));
}

void FRecordingCapability::SetStopRecordingHandler(FCommandHandler InCommandHandler)
{
	SetCommandHandler(StopRecordingCmd, MoveTemp(InCommandHandler));
}

void FRecordingCapability::PublishRecordingEvent(bool bIsRecording)
{
	PublishEvent<FRecordingEvent>(bIsRecording);
}