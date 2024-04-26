// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMessageContext.h"
#include "ISessionManager.h"
#include "ITraceController.h"
#include "MessageEndpoint.h"
#include "TraceControlMessages.h"

struct FTraceControlSettings;
struct FTraceControlStatus;
struct FTraceControlDiscovery;
class ISessionManager;
class IMessageBus;
class FMessageEndpoint;

/**
 * Interface to control other sessions tracing.
 */
class FTraceController : public ITraceController 
{
public:
	FTraceController(const TSharedRef<IMessageBus>& InMessageBus);
	virtual ~FTraceController() override;

private:
	/* ITraceController interface */
	virtual void SetChannels(TConstArrayView<FStringView> ChannelsToEnable, TConstArrayView<FStringView> ChannelsToDisable) override;
	virtual void SetChannels(TConstArrayView<FString> ChannelsToEnable, TConstArrayView<FString> ChannelsToDisable) override;
	virtual void Send(FStringView Host, FStringView Channels, bool bExcludeTail) override;
	virtual void File(FStringView File, FStringView Channels, bool bExcludeTail, bool bTruncateFile) override;
	virtual void Stop() override;
	virtual void SnapshotSend(FStringView Host) override;
	virtual void SnapshotFile(FStringView File) override;
	virtual void Pause() override;
	virtual void Resume() override;
	virtual void Bookmark(FStringView Label) override;
	virtual void Screenshot(FStringView Name, bool bShowUI) override;

	virtual void SendStatusUpdateRequest() override;
	virtual void SendChannelUpdateRequest() override;
	virtual void SendSettingsUpdateRequest() override;
	
	DECLARE_DERIVED_EVENT(FTraceController, ITraceController::FStatusRecievedEvent, FStatusRecievedEvent);
	virtual FStatusRecievedEvent& OnStatusReceived() override
	{
		return StatusReceivedEvent;
	}

	/* Message handlers */
	void OnNotification(const FMessageBusNotification& MessageBusNotification);
	void OnDiscoveryResponse(const FTraceControlDiscovery& Message, const TSharedRef<IMessageContext>& Context);
	void OnStatus(const FTraceControlStatus& Message, const TSharedRef<IMessageContext>& Context);
	void OnChannelsDesc(const FTraceControlChannelsDesc& Message, const TSharedRef<IMessageContext>& Context);
	void OnChannelsStatus(const FTraceControlChannelsStatus& Message, const TSharedRef<IMessageContext>& Context);
	void OnSettings(const FTraceControlSettings& Message, const TSharedRef<IMessageContext>& Context);
	static void UpdateStatus(const FTraceControlStatus& Message, FTraceStatus& Status);

	/* Events from SessionManager handlers */
	void OnInstanceSelectionChanged(const TSharedPtr<class ISessionInstanceInfo>&, bool);

	/** Utility for sending a message to all selected sessions */
	template <class MessageType>
	void SendToSelectedSessions(MessageType* Message);

	TArray<FMessageAddress> GetSelectedSessionAddresses();

public:

private:
	TWeakPtr<IMessageBus> MessageBus;

	/** Our own endpoint for messages */
	TSharedPtr<FMessageEndpoint> MessageEndpoint;

	/** Session manager used for selecting sessions */
	TSharedPtr<ISessionManager> SessionManager;

	/** Address of the runtime endpoint for trace controls */
	FMessageAddress TraceControlAddress;

	FStatusRecievedEvent StatusReceivedEvent;

	/** Lock to protect access to Instances list */
	FRWLock InstancesLock;

	/** Map of instances and their respective last reported status */
	TMap<FMessageAddress, FTraceStatus> Instances;

	/** Secondary lookup from instance -> address */
	TMap<FGuid, FMessageAddress> InstanceToAddress;

	/** Currently selected instance */
	FGuid SelectedInstanceId;
};

template <typename MessageType>
void FTraceController::SendToSelectedSessions(MessageType* Message)
{
	TArray<FMessageAddress> Recipients;
	auto SelectedInstances = SessionManager->GetSelectedInstances();
	for (const auto& Instance : SelectedInstances)
	{
		if (const auto Address = InstanceToAddress.Find(Instance->GetInstanceId()))
		{
			Recipients.Add(*Address);
		}
	}

	if (!Recipients.IsEmpty())
	{
		MessageEndpoint->Send(Message, Recipients);
	}
}
