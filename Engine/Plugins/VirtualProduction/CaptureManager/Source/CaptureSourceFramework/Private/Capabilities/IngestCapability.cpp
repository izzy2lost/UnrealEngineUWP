// Copyright Epic Games, Inc. All Rights Reserved.

#include "Capabilities/IngestCapability.h"

FIngestCapabilityError::FIngestCapabilityError(ECode InCode, FString InMessage)
	: Code(InCode)
	, Message(MoveTemp(InMessage))
{
}

FIngestCapabilityError::ECode FIngestCapabilityError::GetCode() const
{
	return Code;
}

const FString& FIngestCapabilityError::GetMessage() const
{
	return Message;
}

const FString FIngestCapabilityBase::Name = TEXT("Ingest");
const FString FIngestCapabilityBase::CancelCmd = TEXT("Cancel");

const FString FIngestCapabilityBase::Id = TEXT("Id");
const FString FIngestCapabilityBase::Location = TEXT("Location");

FIngestCapabilityBase::FIngestCapabilityBase()
	: FCaptureSourceCapability(Name)
{
	AddCommand(FCommandDesc(FImportTakeCmd::Name,
							{
								FPropertyDesc(Id, FPropertyDesc::EType::Number),
								FPropertyDesc(Location, FPropertyDesc::EType::String)
							}));
	AddCommand(FCommandDesc(FUpdateTakeListCmd::Name, {}));

	AddCommand(FCommandDesc(CancelCmd,
							{
								FPropertyDesc(Id, FPropertyDesc::EType::Number)
							}));
}

FIngestCapabilityBase::FTakeProcessHandle::FTakeProcessHandle(FIngestCapabilityBase* InOwner, FTakeId InId)
	: Owner(InOwner)
	, Id(InId)
{
}

FTakeId FIngestCapabilityBase::AddTakeInfo(FTakeInfo InTakeInfo)
{
	FTakeId TakeId = CurrentTakeId++;
	InTakeInfo.Id = TakeId;
	Takes.Emplace(TakeId, MoveTemp(InTakeInfo));

	return TakeId;
}

FTakeInfo FIngestCapabilityBase::GetTakeInfo(FTakeId InTakeId) const
{
	return Takes[InTakeId];
}

TArray<FTakeId> FIngestCapabilityBase::GetTakeIds() const
{
	TArray<FTakeId> TakeIds;
	Takes.GetKeys(TakeIds);

	return TakeIds;
}

void FIngestCapabilityBase::FTakeProcessHandle::Cancel()
{
	Owner->Cancel(Id);
}

const FString FImportTakeCmd::Name = TEXT("ImportTake");
FImportTakeCmd::FImportTakeCmd(FProcessProgressReporter InProgressReporter,
							   FProcessFinished InProcessFinished)
	: FCommandBase(Name)
	, ProgressReporter(MoveTemp(InProgressReporter))
	, ProcessFinished(MoveTemp(InProcessFinished))
{
}

const FString FUpdateTakeListCmd::Name = TEXT("UpdateTakeList");
FUpdateTakeListCmd::FUpdateTakeListCmd(FCallback InCallback)
	: FCommandBase(Name)
	, Callback(MoveTemp(InCallback))
{
}

FIngestCapability::FIngestCapability()
{
}

FIngestCapability::FTakeProcessHandle FIngestCapability::ImportTake(FTakeId InTakeId,
																	FString InLocation,
																	FImportTakeCmd::FProcessFinished InProcessFinished,
																	FImportTakeCmd::FProcessProgressReporter InProcessProgressReported)
{
	FTakeInfo& TakeInfo = Takes[InTakeId];

	TSharedPtr<FImportTakeCmd> InCommand = MakeShared<FImportTakeCmd>(MoveTemp(InProcessProgressReported),
																	  MoveTemp(InProcessFinished));

	InCommand->AddParamValue(Id, MakePropertyValue<int64>(TakeInfo.Id));
	InCommand->AddParamValue(Location, MakePropertyValue(MoveTemp(InLocation)));

	ExecuteCommand(MoveTemp(InCommand));

	return FTakeProcessHandle(this, InTakeId);
}

void FIngestCapability::UpdateTakeList(FUpdateTakeListCmd::FCallback InUpdateTakeListCallback)
{
	Takes.Empty();

	ExecuteCommand(MakeShared<FUpdateTakeListCmd>(MoveTemp(InUpdateTakeListCallback)));
}

void FIngestCapability::Cancel(FTakeId InTakeId)
{
	TSharedPtr<FCommandBase> InCommand = MakeShared<FCommandBase>(CancelCmd);

	InCommand->AddParamValue(Id, MakePropertyValue<int64>(InTakeId));

	ExecuteCommand(MoveTemp(InCommand));
}

void FIngestCapability::SetImportTakeHandler(FCommandHandler InCommandHandler)
{
	SetCommandHandler(FImportTakeCmd::Name, MoveTemp(InCommandHandler));
}

void FIngestCapability::SetUpdateTakeListHandler(FCommandHandler InCommandHandler)
{
	SetCommandHandler(FUpdateTakeListCmd::Name, MoveTemp(InCommandHandler));
}

void FIngestCapability::SetCancelHandler(FCommandHandler InCommandHandler)
{
	SetCommandHandler(CancelCmd, MoveTemp(InCommandHandler));
}