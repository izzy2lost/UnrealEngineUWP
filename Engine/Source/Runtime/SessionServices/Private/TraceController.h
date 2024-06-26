// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMessageContext.h"
#include "ISessionManager.h"
#include "ITraceController.h"
#include "MessageEndpoint.h"
#include "TraceControllerCommands.h"
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
	virtual void SendDiscoveryRequest(const FGuid& SessionId, const FGuid& InstanceId) const override;
	virtual void SendDiscoveryRequest() override;
	virtual void SendStatusUpdateRequest() override;
	virtual void SendChannelUpdateRequest() override;
	virtual void SendSettingsUpdateRequest() override;
	virtual bool HasAvailableSelectedInstance() override;
	virtual void WithSelectedInstances(FCallback Func) override;
	virtual void WithInstance(FGuid InstanceId, FCallback Func) override;
	
	DECLARE_DERIVED_EVENT(FTraceController, ITraceController::FStatusRecievedEvent, FStatusRecievedEvent);
	virtual FStatusRecievedEvent& OnStatusReceived() override
	{
		return StatusReceivedEvent;
	}

	virtual FStatusRecievedEvent& OnSelectedSessionStatusReceived() override
	{
		return SelectedSessionStatusReceivedEvent;
	}

	DECLARE_DERIVED_EVENT(FTraceController, ITraceController::FSessionSelectionChanged, FSessionSelectionChanged);
	virtual FSessionSelectionChanged& OnSessionSelectionChanged() override
	{
		return SessionSelectionChangedEvent;
	}

	virtual uint32 GetNumSelectedInstances() override { return SelectedInstanceIds.Num(); }

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

	/* A selected instance can end up not discovered, either because the FTraceControlDiscoveryPong was lost
	*  or because the selected session has been unregistered. Attempt to discover it again.
	*  Returns true if a discovery ping was sent. 
	*/
	bool RediscoverSelectedSession() const;

private:
	
	struct FTracingInstance
	{
		FTraceStatus Status;
		FTraceControllerCommands Commands;

		FTracingInstance(const TSharedRef<IMessageBus>& InMessageBus, FMessageAddress Service);
		FTracingInstance() = delete;
	};

	/**
	 * Needed to create command instances when new sessions are discovered. We don't need a ref counted
	 * pointer to the message bus.
	 */
	TWeakPtr<IMessageBus> MessageBus;

	/** Our own endpoint for messages */
	TSharedPtr<FMessageEndpoint> MessageEndpoint;

	/** Session manager used for selecting sessions */
	TSharedPtr<ISessionManager> SessionManager;

	/** Address of the runtime endpoint for trace controls */
	FMessageAddress TraceControlAddress;

	/** Event for status updates on any session */
	FStatusRecievedEvent StatusReceivedEvent;

	/** Event for status updates on a selected session */
	FStatusRecievedEvent SelectedSessionStatusReceivedEvent;

	/** Event that triggers when the session selection changes */
	FSessionSelectionChanged SessionSelectionChangedEvent;

	/** Lock to protect access to Instances list */
	FRWLock InstancesLock;

	/** Known instances with an active trace service */
	TMap<FMessageAddress, FTracingInstance> Instances;

	/** Secondary lookup from instance -> address */
	TMap<FGuid, FMessageAddress> InstanceToAddress;

	/** Currently selected instance */
	TSet<FGuid> SelectedInstanceIds;
};
