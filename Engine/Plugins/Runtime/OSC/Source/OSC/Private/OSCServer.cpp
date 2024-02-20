// Copyright Epic Games, Inc. All Rights Reserved.
#include "OSCServer.h"


#include "OSCMessage.h"
#include "OSCBundle.h"
#include "OSCBundlePacket.h"
#include "OSCLog.h"
#include "OSCServerProxy.h"


UOSCServer::UOSCServer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UOSCServer::GetMulticastLoopback() const
{
	check(ServerProxy.IsValid());
	return ServerProxy->GetMulticastLoopback();
}

bool UOSCServer::IsActive() const
{
	check(ServerProxy.IsValid());
	return ServerProxy->IsActive();
}

void UOSCServer::Listen()
{
	check(ServerProxy.IsValid());
	ServerProxy->Listen(GetName());

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float /*Time*/)
	{
		PumpPacketQueue();
		return true;
	}));
}

bool UOSCServer::SetAddress(const FString& InReceiveIPAddress, int32 InPort)
{
	check(ServerProxy.IsValid());

	FIPv4Address Address;
	FIPv4Address::Parse(InReceiveIPAddress, Address);
	return ServerProxy->SetIPEndpoint(FIPv4Endpoint(Address, InPort));
}

void UOSCServer::SetMulticastLoopback(bool bInMulticastLoopback)
{
	check(ServerProxy.IsValid());
	ServerProxy->SetMulticastLoopback(bInMulticastLoopback);
}

#if WITH_EDITOR
void UOSCServer::SetTickInEditor(bool bInTickInEditor)
{
}
#endif // WITH_EDITOR

void UOSCServer::Stop()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	TickHandle.Reset();

	// CDO May have this not set by initializer ctor, so have to check if valid
	if (ServerProxy.IsValid())
	{
		ServerProxy->Stop();
	}
}

void UOSCServer::BeginDestroy()
{
	Stop();
	Super::BeginDestroy();
}

void UOSCServer::PostInitProperties()
{
	using namespace UE::OSC;

	Super::PostInitProperties();

	const UClass* ThisClass = UOSCServer::StaticClass();
	check(ThisClass);

	const UObject* DefaultObj = ThisClass->GetDefaultObject();
	check(DefaultObj);

	if (DefaultObj != this)
	{
		OSCPackets = MakeShared<FPacketQueue>();
		ServerProxy = MakeUnique<UE::OSC::FServerProxy>(*this);
	}
}

void UOSCServer::SetAllowlistClientsEnabled(bool bEnabled)
{
	check(ServerProxy.IsValid());
	ServerProxy->SetFilterClientsByAllowList(bEnabled);
}

void UOSCServer::AddAllowlistedClient(const FString& InIPAddress, int32 IPPort)
{
	check(ServerProxy.IsValid());

	FIPv4Address NewAddress;
	if (FIPv4Address::Parse(InIPAddress, NewAddress))
	{
		ServerProxy->AddClientEndpointToAllowList(FIPv4Endpoint(NewAddress, IPPort));
	}
}

void UOSCServer::RemoveAllowlistedClient(const FString& InIPAddress, int32 IPPort)
{
	check(ServerProxy.IsValid());

	FIPv4Address NewAddress;
	if (FIPv4Address::Parse(InIPAddress, NewAddress))
	{
		ServerProxy->RemoveClientEndpointFromAllowList(FIPv4Endpoint(NewAddress, IPPort));
	}
}

void UOSCServer::ClearAllowlistedClients()
{
	check(ServerProxy.IsValid());
	ServerProxy->ClearClientEndpointAllowList();
}

FString UOSCServer::GetIpAddress(bool bIncludePort) const
{
	check(ServerProxy.IsValid());

	if (bIncludePort)
	{
		return ServerProxy->GetIPEndpoint().ToString();
	}

	return ServerProxy->GetIPEndpoint().Address.ToString();
}

int32 UOSCServer::GetPort() const
{
	check(ServerProxy.IsValid());
	return ServerProxy->GetIPEndpoint().Port;
}

TSet<FString> UOSCServer::GetAllowlistedClients(bool bIncludePort) const
{
	check(ServerProxy.IsValid());
	const TSet<FIPv4Endpoint>& Endpoints = ServerProxy->GetClientEndpointAllowList();

	TSet<FString> Result;
	Algo::Transform(Endpoints, Result, [&bIncludePort](const FIPv4Endpoint& ClientEndpoint)
	{
		return bIncludePort
			? ClientEndpoint.ToString()
			: ClientEndpoint.Address.ToString();
	});

	return Result;
}

void UOSCServer::BindEventToOnOSCAddressPatternMatchesPath(const FOSCAddress& InOSCAddressPattern, const FOSCDispatchMessageEventBP& InEvent)
{
	if (InOSCAddressPattern.IsValidPattern())
	{
		FOSCDispatchMessageEvent& MessageEvent = AddressPatterns.FindOrAdd(InOSCAddressPattern);
		MessageEvent.AddUnique(InEvent);
	}
}

void UOSCServer::UnbindEventFromOnOSCAddressPatternMatchesPath(const FOSCAddress& InOSCAddressPattern, const FOSCDispatchMessageEventBP& InEvent)
{
	if (InOSCAddressPattern.IsValidPattern())
	{
		if (FOSCDispatchMessageEvent* AddressPatternEvent = AddressPatterns.Find(InOSCAddressPattern))
		{
			AddressPatternEvent->Remove(InEvent);
			if (!AddressPatternEvent->IsBound())
			{
				AddressPatterns.Remove(InOSCAddressPattern);
			}
		}
	}
}

void UOSCServer::UnbindAllEventsFromOnOSCAddressPatternMatchesPath(const FOSCAddress& InOSCAddressPattern)
{
	if (InOSCAddressPattern.IsValidPattern())
	{
		AddressPatterns.Remove(InOSCAddressPattern);
	}
}

void UOSCServer::UnbindAllEventsFromOnOSCAddressPatternMatching()
{
	AddressPatterns.Reset();
}

TArray<FOSCAddress> UOSCServer::GetBoundOSCAddressPatterns() const
{
	TArray<FOSCAddress> OutAddressPatterns;
	AddressPatterns.GetKeys(OutAddressPatterns);
	return OutAddressPatterns;
}

void UOSCServer::ClearPackets()
{
	OSCPackets->Empty();
}

void UOSCServer::EnqueuePacket(TSharedPtr<UE::OSC::IPacket> InPacket)
{
	OSCPackets->Enqueue(InPacket);
}

void UOSCServer::DispatchBundle(const FString& InIPAddress, uint16 InPort, const FOSCBundle& InBundle)
{
	using namespace UE::OSC;

	OnOscBundleReceived.Broadcast(InBundle, InIPAddress, InPort);
	OnOscBundleReceivedNative.Broadcast(InBundle, InIPAddress, InPort);

	TSharedPtr<FBundlePacket> BundlePacket = StaticCastSharedRef<FBundlePacket>(InBundle.GetPacketRef());
	TArray<TSharedRef<IPacket>>& Packets = BundlePacket->GetPackets();
	for (TSharedRef<IPacket>& Packet : Packets)
	{
		if (Packet->IsMessage())
		{
			DispatchMessage(InIPAddress, InPort, FOSCMessage(Packet));
		}
		else if (Packet->IsBundle())
		{
			DispatchBundle(InIPAddress, InPort, FOSCBundle(Packet));
		}
		else
		{
			UE_LOG(LogOSC, Warning, TEXT("Failed to parse invalid received message. Invalid OSC type (packet is neither identified as message nor bundle)."));
		}
	}
}

void UOSCServer::DispatchMessage(const FString& InIPAddress, uint16 InPort, const FOSCMessage& InMessage)
{
	OnOscMessageReceived.Broadcast(InMessage, InIPAddress, InPort);
	OnOscMessageReceivedNative.Broadcast(InMessage, InIPAddress, InPort);

	UE_LOG(LogOSC, Verbose, TEXT("Message received from endpoint '%s', OSCAddress of '%s'."), *InIPAddress, *InMessage.GetAddress().GetFullPath());

	for (const TPair<FOSCAddress, FOSCDispatchMessageEvent>& Pair : AddressPatterns)
	{
		const FOSCDispatchMessageEvent& DispatchEvent = Pair.Value;
		if (Pair.Key.Matches(InMessage.GetAddress()))
		{
			DispatchEvent.Broadcast(Pair.Key, InMessage, InIPAddress, InPort);
			UE_LOG(LogOSC, Verbose, TEXT("Message dispatched from endpoint '%s', OSCAddress path of '%s' matched OSCAddress pattern '%s'."),
				*InIPAddress,
				*InMessage.GetAddress().GetFullPath(),
				*Pair.Key.GetFullPath());
		}
	}
}

void UOSCServer::PumpPacketQueue()
{
	using namespace UE::OSC;

	check(IsInGameThread());

	TSharedPtr<UE::OSC::IPacket> Packet;
	while (OSCPackets->Dequeue(Packet))
	{
		FIPv4Address IPAddr;
		const FIPv4Endpoint& Endpoint = Packet->GetIPEndpoint();
		if (ServerProxy->CanProcessPacket(Packet.ToSharedRef()))
		{
			const FString StringAdr = Endpoint.Address.ToString();
			if (Packet->IsMessage())
			{
				DispatchMessage(StringAdr, Endpoint.Port, FOSCMessage(Packet.ToSharedRef()));
			}
			else if (Packet->IsBundle())
			{
				DispatchBundle(StringAdr, Endpoint.Port, FOSCBundle(Packet.ToSharedRef()));
			}
			else
			{
				UE_LOG(LogOSC, Warning, TEXT("Failed to parse invalid received message. Invalid OSC type (packet is neither identified as message nor bundle)."));
			}
		}
	}
}
