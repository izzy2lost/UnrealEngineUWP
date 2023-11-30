// Copyright Epic Games, Inc. All Rights Reserved.

#include "Capabilities/MessagingCapability.h"

const FString FMessagingEvent::Name = TEXT("MessagingEvent");

FMessagingEvent::FMessagingEvent(const FString& InEventName, const FString& InCapability)
	: FCaptureEvent(Name)
	, EventName(InEventName)
	, Capability(InCapability)
{
}

const FString FMessagingCapability::Name = TEXT("Messaging");
const float FMessagingCapability::KeepAliveInterval = 5.0f; // seconds
const float FMessagingCapability::KeepAliveTimeout = 3.0f; // seconds

FMessagingCapability::FMessagingCapability()
	: FCaptureSourceCapability(Name)
	, Builder(*Name)
	, bIsConnected(false)
{
	Builder.Handling<FEventMessage>(this, &FMessagingCapability::EventHandler)
		.Handling<FConnectResponse>(this, &FMessagingCapability::ConnectHandler)
		.Handling<FPingResponse>(this, &FMessagingCapability::ResponseHandler<FPingResponse>)
		.Handling<FGetCapabilitiesResponse>(this, &FMessagingCapability::ResponseHandler<FGetCapabilitiesResponse>)
		.Handling<FGetCapabilityResponse>(this, &FMessagingCapability::ResponseHandler<FGetCapabilityResponse>)
		.Handling<FGetPropertyValueResponse>(this, &FMessagingCapability::ResponseHandler<FGetPropertyValueResponse>)
		.Handling<FSetPropertyValueResponse>(this, &FMessagingCapability::ResponseHandler<FSetPropertyValueResponse>)
		.Handling<FExecuteCommandResponse>(this, &FMessagingCapability::ResponseHandler<FExecuteCommandResponse>);

	RegisterEvent(FMessagingEvent::Name);
}

void FMessagingCapability::Initialize()
{
	Endpoint = Builder.Build();

	Endpoint->Subscribe<FEventMessage>();
}

FMessageEndpointBuilder& FMessagingCapability::GetBuilder()
{
	return Builder;
}

void FMessagingCapability::SetAddress(FMessageAddress InAddress)
{
	ClientAddress = MoveTemp(InAddress);
}

void FMessagingCapability::SetDisconnectHandler(FDisconnectHandler InHandler)
{
	Handler = MoveTemp(InHandler);
}

void FMessagingCapability::Connect(FMessageCallback InCallback)
{
	if (bIsConnected.load())
	{
		return;
	}

	check(ClientAddress.IsValid());

	FConnectRequest* Request = FMessageEndpoint::MakeMessage<FConnectRequest>();
	Request->Guid = FGuid::NewGuid();

	AddContext(Request->Guid, MoveTemp(InCallback));
	Endpoint->Send(Request, ClientAddress);
}

void FMessagingCapability::SendGetCapabilities(FMessageCallback InCallback)
{
	if (!bIsConnected.load())
	{
		return;
	}

	FGetCapabilitiesRequest* Request = FMessageEndpoint::MakeMessage<FGetCapabilitiesRequest>();
	SendRequest(Request, MoveTemp(InCallback));
}

void FMessagingCapability::SendGetCapability(FString InCapability, FMessageCallback InCallback)
{
	if (!bIsConnected.load())
	{
		return;
	}

	FGetCapabilityRequest* Request = FMessageEndpoint::MakeMessage<FGetCapabilityRequest>();
	Request->Capability = MoveTemp(InCapability);

	SendRequest(Request, MoveTemp(InCallback));
}

void FMessagingCapability::SendGetPropertyValue(FString InCapability, FString InProperty, FMessageCallback InCallback)
{
	if (!bIsConnected.load())
	{
		return;
	}

	FGetPropertyValueRequest* Request = FMessageEndpoint::MakeMessage<FGetPropertyValueRequest>();
	Request->Capability = MoveTemp(InCapability);
	Request->Property = MoveTemp(InProperty);

	SendRequest(Request, MoveTemp(InCallback));
}

void FMessagingCapability::SendSetPropertyValue(FString InCapability, FString InProperty, FPropertyValue InValue, FMessageCallback InCallback)
{
	if (!bIsConnected.load())
	{
		return;
	}

	FSetPropertyValueRequest* Request = FMessageEndpoint::MakeMessage<FSetPropertyValueRequest>();
	Request->Capability = MoveTemp(InCapability);
	Request->Property = MoveTemp(InProperty);

	Request->Value = ParsePropertyValue(MoveTemp(InValue));

	SendRequest(Request, MoveTemp(InCallback));
}

void FMessagingCapability::SendExecuteCommand(FString InCapability, TSharedPtr<FCommandBase> InCommand, FMessageCallback InCallback)
{
	if (!bIsConnected.load())
	{
		return;
	}

	FExecuteCommandRequest* Request = FMessageEndpoint::MakeMessage<FExecuteCommandRequest>();
	Request->Capability = MoveTemp(InCapability);
	Request->Command = InCommand->GetName();

	for (const TPair<FString, FPropertyValue>& Value : InCommand->GetParamValues())
	{
		Request->Params.Add(Value.Key, ParsePropertyValue(Value.Value));
	}

	SendRequest(Request, MoveTemp(InCallback));
}

void FMessagingCapability::ConnectHandler(const FConnectResponse& InResponse, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& InContext)
{
	check(ClientAddress == InContext->GetSender());

	if (InResponse.Status == EStatus::Ok)
	{
		bIsConnected.store(true);

		KeepAlive = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FMessagingCapability::OnKeepAliveInterval), KeepAliveInterval);
	}

	ResponseHandler(InResponse, InContext);
}

void FMessagingCapability::AddContext(FGuid InGuid, FMessageCallback InCallback)
{
	FScopeLock Lock(&Mutex);
	Contexts.Add(MoveTemp(InGuid), MoveTemp(InCallback));
}

void FMessagingCapability::ResponseHandler(const FName& InResponseName, const FBaseResponse& InResponse)
{
	FScopeLock Lock(&Mutex);
	if (Contexts.Contains(InResponse.RequestGuid))
	{
		FMessageCallback Callback;
		Contexts.RemoveAndCopyValue(InResponse.RequestGuid, Callback);

		Callback.ExecuteIfBound(InResponseName, InResponse);
	}
}

void FMessagingCapability::EventHandler(const FEventMessage& InEvent, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>&)
{
	if (!bIsConnected.load())
	{
		return;
	}

	TSharedPtr<FMessagingEvent> Event = MakeShared<FMessagingEvent>(InEvent.Name, InEvent.Capability);

	for (const TPair<FString, FValue>& Param : InEvent.Params)
	{
		Event->Params.Add(Param.Key, ParseValue(Param.Value));
	}

	PublishEventPtr(MoveTemp(Event));
}

bool FMessagingCapability::OnKeepAliveInterval(float InDeltaTime)
{
	AsyncTask(ENamedThreads::Type::AnyThread, [this]()
	{
		if (!bIsConnected.load())
		{
			return;
		}

		TPromise<void> Promise;
		TFuture<void> Future = Promise.GetFuture();

		FPingRequest* Request = FMessageEndpoint::MakeMessage<FPingRequest>();

		SendRequest(Request, FMessageCallback::CreateLambda([&Promise](const FName& InName, const FBaseResponse& InResponse) mutable
		{
			check(InName == FPingResponse::StaticStruct()->GetFName());

			Promise.SetValue();
		}));

		bool Result = Future.WaitFor(FTimespan::FromSeconds(KeepAliveTimeout));

		if (!Result)
		{
			Future.Reset();
			Promise.SetValue(); // Setting a value to avoid error when destructing the promise

			Handler.ExecuteIfBound();

			bIsConnected.store(false);
			FTSTicker::GetCoreTicker().RemoveTicker(KeepAlive);
		}
	});

	return true;
}

FValue FMessagingCapability::ParsePropertyValue(FPropertyValue InValue)
{
	FValue Value;
	if (InValue.IsType<bool>())
	{
		Value.BoolValue = InValue.Get<bool>();
		Value.Type = EValueType::Bool;
	}
	else if (InValue.IsType<int64>())
	{
		Value.IntegerValue = InValue.Get<int64>();
		Value.Type = EValueType::Integer;
	}
	else if (InValue.IsType<double>())
	{
		Value.FloatingPointValue = InValue.Get<double>();
		Value.Type = EValueType::FloatingPoint;
	}
	else if (InValue.IsType<FString>())
	{
		Value.StringValue = InValue.Get<FString>();
		Value.Type = EValueType::String;
	}

	return Value;
}

FPropertyValue FMessagingCapability::ParseValue(const FValue& InValue)
{
	switch (InValue.Type)
	{
		case EValueType::Bool:
			return MakePropertyValue(InValue.BoolValue);
		case EValueType::Integer:
			return MakePropertyValue(InValue.IntegerValue);
		case EValueType::FloatingPoint:
			return MakePropertyValue(InValue.FloatingPointValue);
		case EValueType::String:
			return MakePropertyValue(InValue.StringValue);
		default:
			return MakePropertyValue(FEmptyVariantState());
	}
}