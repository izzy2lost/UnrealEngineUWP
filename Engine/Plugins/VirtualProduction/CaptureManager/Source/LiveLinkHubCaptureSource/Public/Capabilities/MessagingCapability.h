// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureSourceCapability.h"

#include "MessageEndpointBuilder.h"
#include "MessageEndpoint.h"

#include "LiveLinkHubCaptureMessages.h"

#include "HAL/CriticalSection.h"

#include "Containers/Ticker.h"

struct LIVELINKHUBCAPTURESOURCE_API FMessagingEvent : public FCaptureEvent
{
	static const FString Name;

	FMessagingEvent(const FString& InEventName, const FString& InCapability);

	const FString EventName;
	const FString Capability;
	TMap<FString, FPropertyValue> Params;
};

class LIVELINKHUBCAPTURESOURCE_API FMessagingCapability : public FCaptureSourceCapability
{
public:

	DECLARE_DELEGATE_TwoParams(FMessageCallback, const FName&, const FBaseResponse&);
	DECLARE_DELEGATE(FDisconnectHandler);

	static const FString Name;
	static const float KeepAliveInterval;
	static const float KeepAliveTimeout;

	FMessagingCapability();

	FMessageEndpointBuilder& GetBuilder();

	void Initialize();

	void SendGetCapabilities(FMessageCallback InCallback);
	void SendGetCapability(FString InCapability, FMessageCallback InCallback);

	void SendGetPropertyValue(FString InCapability, FString InProperty, FMessageCallback InCallback);
	void SendSetPropertyValue(FString InCapability, FString InProperty, FPropertyValue InValue, FMessageCallback InCallback);

	void SendExecuteCommand(FString InCapability, TSharedPtr<FCommandBase> InCommand, FMessageCallback InCallback);

	void SetAddress(FMessageAddress InAddress);
	void SetDisconnectHandler(FDisconnectHandler InHandler);

	void Connect(FMessageCallback InCallback);

	static FValue ParsePropertyValue(FPropertyValue InValue);
	static FPropertyValue ParseValue(const FValue& InValue);

	template<typename T>
	void SendMessage(T* InMessage, bool bIsReliable)
	{
		if (!bIsConnected.load())
		{
			return;
		}

		if (!bIsReliable)
		{
			Endpoint->Send(InMessage, ClientAddress);
		}
		else
		{
			Endpoint->Send(InMessage, EMessageFlags::Reliable, nullptr, { ClientAddress }, FTimespan::Zero(), FDateTime::MaxValue());
		}
	}

private:

	template<typename T>
	void SendRequest(T* Request, FMessageCallback InCallback)
	{
		Request->Guid = FGuid::NewGuid();

		AddContext(Request->Guid, MoveTemp(InCallback));
		Endpoint->Send(Request, ClientAddress);
	}

	void AddContext(FGuid InGuid, FMessageCallback InCallback);

	void ConnectHandler(const FConnectResponse& InResponse, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext);

	template<typename Response>
	void ResponseHandler(const Response& InResponse, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>&)
	{
		ResponseHandler(Response::StaticStruct()->GetFName(), InResponse);
	}

	void ResponseHandler(const FName& InResponseName, const FBaseResponse& InResponse);

	void EventHandler(const FEventMessage& InEvent, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>&);

	bool OnKeepAliveInterval(float InDeltaTime);
	
	FMessageEndpointBuilder Builder;
	TSharedPtr<FMessageEndpoint> Endpoint;
	FMessageAddress ClientAddress;

	FCriticalSection Mutex;
	TMap<FGuid, FMessageCallback> Contexts;

	std::atomic_bool bIsConnected;
	FTSTicker::FDelegateHandle KeepAlive;

	FDisconnectHandler Handler;
};