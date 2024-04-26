// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraceController.h"

#include "IMessageBus.h"
#include "ISessionServicesModule.h"
#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"
#include "TraceControlMessages.h"

FTraceController::FTraceController(const TSharedRef<IMessageBus>& InMessageBus)
	: MessageBus(InMessageBus)
{
	MessageEndpoint = FMessageEndpoint::Builder("FTraceController", InMessageBus)
		.Handling<FTraceControlDiscovery>(this, &FTraceController::OnDiscoveryResponse)
		.Handling<FTraceControlStatus>(this, &FTraceController::OnStatus)
		.Handling<FTraceControlSettings>(this, &FTraceController::OnSettings)
		.Handling<FTraceControlChannelsDesc>(this, &FTraceController::OnChannelsDesc)
		.Handling<FTraceControlChannelsStatus>(this, &FTraceController::OnChannelsStatus)
		.NotificationHandling(FOnBusNotification::CreateRaw(this, &FTraceController::OnNotification));
	
	ISessionServicesModule& SessionServicesModule = FModuleManager::LoadModuleChecked<ISessionServicesModule>("SessionServices");
	SessionManager = SessionServicesModule.GetSessionManager();
	
	if (SessionManager.IsValid())
	{
		//Note that we should not add ourselves as shared pointer to session manager,
		//that will create a circular dependency
		SessionManager->OnInstanceSelectionChanged().AddRaw(this, &FTraceController::OnInstanceSelectionChanged);
	}
}

FTraceController::~FTraceController()
{
	if (SessionManager.IsValid())
	{
		SessionManager->OnInstanceSelectionChanged().RemoveAll(this);
	}
}

template<typename StringType>
TArray<uint32> ResolveChannelNames(TConstArrayView<StringType> ChannelNames, const TMap<uint32, FTraceStatus::FChannel> Channels)
{
	TArray<uint32> Ids;
	if (ChannelNames.Num() > 0)
	{
		for (const auto& [Id, Channel] : Channels)
		{
			if (ChannelNames.Contains(Channel.Name))
			{
				Ids.Add(Channel.Id);
			}
		}
	}
	return MoveTemp(Ids);
}

void FTraceController::SetChannels(TConstArrayView<FStringView> ChannelsToEnable, TConstArrayView<FStringView> ChannelsToDisable)
{
	FReadScopeLock _(InstancesLock);

	// Since each session might have different notion about channel ids
	// we need to send a specific message to each instance using their unique
	// channel mappings.
	for (const auto& Instance : SessionManager->GetSelectedInstances())
	{
		if (const auto Address = InstanceToAddress.Find(Instance->GetInstanceId()))
		{
			const auto& InstanceStatus = Instances[*Address];
			const auto Message = FMessageEndpoint::MakeMessage<FTraceControlChannelsSet>();
			Message->ChannelIdsToEnable = ResolveChannelNames(ChannelsToEnable, InstanceStatus.Channels);
			Message->ChannelIdsToDisable = ResolveChannelNames(ChannelsToDisable, InstanceStatus.Channels);
			MessageEndpoint->Send(Message, *Address);
		}
	}
}

void FTraceController::SetChannels(TConstArrayView<FString> ChannelsToEnable, TConstArrayView<FString> ChannelsToDisable)
{
	FReadScopeLock _(InstancesLock);
	
	for (const auto& Instance : SessionManager->GetSelectedInstances())
	{
		if (const auto Address = InstanceToAddress.Find(Instance->GetInstanceId()))
		{
			const auto& InstanceStatus = Instances[*Address];
			const auto Message = FMessageEndpoint::MakeMessage<FTraceControlChannelsSet>();
			Message->ChannelIdsToEnable = ResolveChannelNames(ChannelsToEnable, InstanceStatus.Channels);
			Message->ChannelIdsToDisable = ResolveChannelNames(ChannelsToDisable, InstanceStatus.Channels);
			MessageEndpoint->Send(Message, *Address);
		}
	}
}

void FTraceController::Send(FStringView Host, FStringView Channels, bool bExcludeTail)
{
	const auto Message = FMessageEndpoint::MakeMessage<FTraceControlSend>();
	Message->Host = Host;
	Message->Channels = Channels;
	Message->bExcludeTail = bExcludeTail;

	SendToSelectedSessions(Message);
}

void FTraceController::File(FStringView File, FStringView Channels, bool bExcludeTail, bool bTruncateFile)
{
	const auto Message = FMessageEndpoint::MakeMessage<FTraceControlFile>();
	Message->File = File;
	Message->Channels = Channels;
	Message->bExcludeTail = bExcludeTail;
	Message->bTruncateFile = bTruncateFile;

	SendToSelectedSessions(Message);
}

void FTraceController::Stop()
{
	SendToSelectedSessions(FMessageEndpoint::MakeMessage<FTraceControlStop>());
}

void FTraceController::SnapshotSend(FStringView Host)
{
	const auto Message = FMessageEndpoint::MakeMessage<FTraceControlSnapshotSend>();
	Message->Host = Host;

	SendToSelectedSessions(Message);
}

void FTraceController::SnapshotFile(FStringView File)
{
	const auto Message = FMessageEndpoint::MakeMessage<FTraceControlSnapshotFile>();
	Message->File = File;

	SendToSelectedSessions(Message);
}

void FTraceController::Pause()
{
	SendToSelectedSessions(FMessageEndpoint::MakeMessage<FTraceControlPause>());
}

void FTraceController::Resume()
{
	SendToSelectedSessions(FMessageEndpoint::MakeMessage<FTraceControlResume>());
}

void FTraceController::Bookmark(FStringView Label)
{
	const auto Message = FMessageEndpoint::MakeMessage<FTraceControlBookmark>();
	Message->Label = Label;
	
	SendToSelectedSessions(Message);
}

void FTraceController::Screenshot(FStringView Name, bool bShowUI)
{
	const auto Message = FMessageEndpoint::MakeMessage<FTraceControlScreenshot>();
	Message->Name = Name;
	Message->bShowUI = bShowUI;
	
	SendToSelectedSessions(Message);
}

void FTraceController::SendStatusUpdateRequest()
{
	MessageEndpoint->Publish(FMessageEndpoint::MakeMessage<FTraceControlStatusPing>());
}

void FTraceController::SendChannelUpdateRequest()
{
	FReadScopeLock _(InstancesLock);
	
	for (const auto& Instance : Instances)
	{
		const auto Message = FMessageEndpoint::MakeMessage<FTraceControlChannelsPing>();
		Message->KnownChannelCount = uint32(Instance.Value.Channels.Num());
		MessageEndpoint->Send(Message, Instance.Key);
	}
}

void FTraceController::SendSettingsUpdateRequest()
{
	MessageEndpoint->Publish(FMessageEndpoint::MakeMessage<FTraceControlSettingsPing>());
}

void FTraceController::OnNotification(const FMessageBusNotification& Event)
{
	FWriteScopeLock _(InstancesLock);
	
	if (Event.NotificationType == EMessageBusNotification::Unregistered)
	{
		// Many endpoints may be removed from one instance, look for the
		// one we have registered.
		if(Instances.Remove(Event.RegistrationAddress) > 0)
		{
			InstanceToAddress = InstanceToAddress.FilterByPredicate([&](auto It) { return It.Value == Event.RegistrationAddress; } );
		}
	}
}

void FTraceController::OnDiscoveryResponse(const FTraceControlDiscovery& Message, const TSharedRef<IMessageContext>& Context)
{
	FWriteScopeLock _(InstancesLock);
	
	FTraceStatus& Status = Instances.FindOrAdd(Context->GetSender());
	InstanceToAddress.FindOrAdd(Message.InstanceId, Context->GetSender());
	
	UpdateStatus(Message, Status);
	// This is the application session id, which is not necessarily the
	// same as trace session (may be overridden by commandline)
	Status.SessionId = Message.SessionId;
	Status.InstanceId = Message.InstanceId;

	if (SelectedInstanceId == Message.InstanceId)
	{
		StatusReceivedEvent.Broadcast(Status, FTraceStatus::EUpdateType::All);
	}
}

void FTraceController::OnStatus(const FTraceControlStatus& Message, const TSharedRef<IMessageContext>& Context)
{
	FWriteScopeLock _(InstancesLock);
	
	if (FTraceStatus* Status = Instances.Find(Context->GetSender()))
	{
		UpdateStatus(Message, *Status);
		
		if (SelectedInstanceId == Status->InstanceId)
		{
			StatusReceivedEvent.Broadcast(*Status, FTraceStatus::EUpdateType::Status);
		}
	}
	
}

void FTraceController::OnChannelsDesc(const FTraceControlChannelsDesc& Message, const TSharedRef<IMessageContext>& Context)
{
	FWriteScopeLock _(InstancesLock);
	
	const auto Status = Instances.Find(Context->GetSender());
	
	if (!Status)
	{
		return;
	}
	
	check(Message.Channels.Num() == Message.Ids.Num() && Message.Channels.Num() == Message.Descriptions.Num());
	const int32 Count = Message.Channels.Num();
	for(int32 Index = 0; Index < Count; ++Index)
	{
		const uint32 Id = Message.Ids[Index];
		if (Status->Channels.Contains(Id))
		{
			continue;
		}

		FTraceStatus::FChannel& NewChannel = Status->Channels.Add(Id);
		NewChannel.Name = Message.Channels[Index];
		NewChannel.Description = Message.Descriptions[Index];
		NewChannel.Id = Id;
		NewChannel.bEnabled = false;
		NewChannel.bReadOnly = Message.ReadOnlyIds.Contains(Id);
		
		if (SelectedInstanceId == Status->InstanceId)
		{
			StatusReceivedEvent.Broadcast(*Status, FTraceStatus::EUpdateType::ChannelsDesc);
		}
	}
}

void FTraceController::OnChannelsStatus(const FTraceControlChannelsStatus& Message, const TSharedRef<IMessageContext>& Context)
{
	FWriteScopeLock _(InstancesLock);
	
	if (const auto Status = Instances.Find(Context->GetSender()))
	{
		for (auto& [Id, Channel] : Status->Channels)
		{
			Channel.bEnabled = Message.EnabledIds.Contains(Id);
		}
		
		if (SelectedInstanceId == Status->InstanceId)
		{
			StatusReceivedEvent.Broadcast(*Status, FTraceStatus::EUpdateType::ChannelsStatus);
		}
	}
}

void FTraceController::OnSettings(const FTraceControlSettings& Message, const TSharedRef<IMessageContext>& Context)
{
	FWriteScopeLock _(InstancesLock);
	
	if (const auto Status = Instances.Find(Context->GetSender()))
	{
		FTraceStatus::FSettings& Settings = Status->Settings;
		Settings.bStatNamedEvents = Message.bStatNamedEvents;
		Settings.bUseImportantCache = Message.bUseImportantCache;
		Settings.bUseWorkerThread = Message.bUseWorkerThread;
		Settings.TailSizeBytes = Message.TailSizeBytes;
		
		if (SelectedInstanceId == Status->InstanceId)
		{
			StatusReceivedEvent.Broadcast(*Status, FTraceStatus::EUpdateType::Settings);
		}
	}
}

void FTraceController::OnInstanceSelectionChanged(const TSharedPtr<ISessionInstanceInfo>& Instance, bool bSelected)
{
	FReadScopeLock _(InstancesLock);
	
	SelectedInstanceId = Instance->GetInstanceId();
	if (const auto Address = InstanceToAddress.Find(SelectedInstanceId))
	{
		if (const auto Status = Instances.Find(*Address))
		{
			StatusReceivedEvent.Broadcast(*Status, FTraceStatus::EUpdateType::All);
		}
	}
	else
	{
		// We haven't discovered this instance yet, send a discovery message specifically
		// to that instance.
		const auto Message = FMessageEndpoint::MakeMessage<FTraceControlDiscoveryPing>();
		Message->SessionId = Instance->GetOwnerSession()->GetSessionId();
		Message->InstanceId = Instance->GetInstanceId();
		MessageEndpoint->Publish<FTraceControlDiscoveryPing>(Message);
	}
}

TArray<FMessageAddress> FTraceController::GetSelectedSessionAddresses()
{
	TArray<FMessageAddress> Selected;
	auto SelectedInstances = SessionManager->GetSelectedInstances();
	for (const auto& Instance : SelectedInstances)
	{
		if (const auto Address = InstanceToAddress.Find(Instance->GetInstanceId()))
		{
			Selected.Add(*Address);
		}
	}
	return MoveTemp(Selected);
}

void FTraceController::UpdateStatus(const FTraceControlStatus& Message, FTraceStatus& Status)
{
	Status.bIsTracing = !Message.Endpoint.IsEmpty();
	Status.Endpoint = Message.Endpoint;
	Status.SessionGuid = Message.SessionGuid;
	Status.TraceGuid = Message.TraceGuid;
	Status.Stats.BytesSent = Message.BytesSent;
	Status.Stats.BytesTraced = Message.BytesTraced;
	Status.Stats.CacheAllocated = Message.CacheAllocated;
	Status.Stats.CacheUsed = Message.CacheUsed;
	Status.Stats.CacheWaste = Message.CacheWaste;
}
