// Copyright Epic Games, Inc. All Rights Reserved.

#include "SessionTraceControllerFilterService.h"

#include "Models/ITraceFilterPreset.h"
#include "Misc/CoreDelegates.h"

namespace UE::TraceTools
{

FSessionTraceControllerFilterService::FSessionTraceControllerFilterService(TSharedPtr<ITraceController> InTraceController)
{
	FCoreDelegates::OnEndFrame.AddRaw(this, &FSessionTraceControllerFilterService::OnApplyChannelChanges);

	TraceController = InTraceController;
	TraceController->OnStatusReceived().AddRaw(this, &FSessionTraceControllerFilterService::OnTraceStatusUpdated);
}

FSessionTraceControllerFilterService::~FSessionTraceControllerFilterService()
{
	FCoreDelegates::OnEndFrame.RemoveAll(this);

	TraceController->OnStatusReceived().RemoveAll(this);
}

void FSessionTraceControllerFilterService::GetRootObjects(TArray<FTraceObjectInfo>& OutObjects) const
{
	OutObjects.Append(Objects);
}

void FSessionTraceControllerFilterService::GetChildObjects(uint32 InObjectHash, TArray<FTraceObjectInfo>& OutChildObjects) const
{
	/** TODO, parent/child relationship for Channels */
}

const FDateTime& FSessionTraceControllerFilterService::GetTimestamp()
{
	return TimeStamp;
}

void FSessionTraceControllerFilterService::SetObjectFilterState(const FString& InObjectName, const bool bFilterState)
{
	if (bFilterState)
	{
		FrameDisabledChannels.Remove(InObjectName);
		FrameEnabledChannels.Add(InObjectName);
	}
	else
	{
		FrameEnabledChannels.Remove(InObjectName);
		FrameDisabledChannels.Add(InObjectName);
	}
}

void FSessionTraceControllerFilterService::UpdateFilterPreset(const TSharedPtr<ITraceFilterPreset> Preset, bool IsEnabled)
{
	TArray<FString> Names;
	Preset->GetAllowlistedNames(Names);
	if (IsEnabled)
	{
		FrameEnabledChannels.Append(Names);
	}
	else
	{
		FrameDisabledChannels.Append(Names);
	}
}

void FSessionTraceControllerFilterService::DisableAllChannels()
{
	for (FTraceObjectInfo& ObjectInfo : Objects)
	{
		ObjectInfo.bEnabled = false;
	}
}

void FSessionTraceControllerFilterService::OnTraceStatusUpdated(const FTraceStatus& InStatus, FTraceStatus::EUpdateType InUpdateType)
{
	if (!TraceController->HasAvailableSelectedInstance())
	{
		TimeStamp = FDateTime::Now();
		Objects.Empty();
		return;
	}
	if (InUpdateType != FTraceStatus::EUpdateType::ChannelsDesc && 
		InUpdateType != FTraceStatus::EUpdateType::ChannelsStatus && 
		InUpdateType != FTraceStatus::EUpdateType::All)
	{
		return;
	}

	TimeStamp = FDateTime::Now();

	const TMap<uint32, FTraceStatus::FChannel> Channels = InStatus.Channels;
	Objects.Empty(Channels.Num());

	for (auto& Entry : Channels)
	{
		FTraceObjectInfo& EventInfo = Objects.AddDefaulted_GetRef();
		EventInfo.Name = Entry.Value.Name;
		EventInfo.bEnabled = Entry.Value.bEnabled;
		EventInfo.bReadOnly = Entry.Value.bReadOnly;
		EventInfo.Hash = Entry.Value.Id;
		EventInfo.OwnerHash = 0;
	}
}

void FSessionTraceControllerFilterService::OnApplyChannelChanges()
{
	if (FrameEnabledChannels.Num() || FrameDisabledChannels.Num())
	{
		TraceController->SetChannels(FrameEnabledChannels, FrameDisabledChannels);
		FrameEnabledChannels.Empty();
		FrameDisabledChannels.Empty();
	}
}

} // namespace UE::TraceTools