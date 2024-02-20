// Copyright Epic Games, Inc. All Rights Reserved.
#include "OSCMessage.h"

#include "OSCLog.h"
#include "OSCMessagePacket.h"


FOSCMessage::FOSCMessage()
	: Packet(MakeShared<UE::OSC::FMessagePacket>())
{
}

FOSCMessage::FOSCMessage(const TSharedRef<UE::OSC::IPacket>& InPacket)
	: Packet(InPacket)
{
}

FOSCMessage::FOSCMessage(const TSharedPtr<UE::OSC::IPacket>& InPacket)
	: FOSCMessage::FOSCMessage()
{
	Packet = TSharedRef<UE::OSC::IPacket>(InPacket.Get());
}

void FOSCMessage::SetPacket(TSharedPtr<UE::OSC::IPacket>& InPacket)
{
	using namespace UE::OSC;
	Packet = TSharedRef<IPacket>(InPacket.Get());
}

void FOSCMessage::SetPacket(TSharedRef<UE::OSC::IPacket>& InPacket)
{
	Packet = InPacket;
}

const TSharedPtr<UE::OSC::IPacket>& FOSCMessage::GetPacket() const
{
	static TSharedPtr<UE::OSC::IPacket> RetPacketPtr;
	RetPacketPtr = TSharedPtr<UE::OSC::IPacket>(&Packet.Get());
	return RetPacketPtr;
}

const TSharedRef<UE::OSC::IPacket>& FOSCMessage::GetPacketRef() const
{
	return Packet;
}

const TArray<UE::OSC::FOSCData>& FOSCMessage::GetArgumentsChecked() const
{
	using namespace UE::OSC;
	return StaticCastSharedRef<FMessagePacket>(Packet)->GetArguments();
}

bool FOSCMessage::SetAddress(const FOSCAddress& InAddress)
{
	using namespace UE::OSC;

	if (!InAddress.IsValidPath())
	{
		UE_LOG(LogOSC, Warning, TEXT("Attempting to set invalid OSCAddress '%s'. OSC address must begin with '/'"), *InAddress.GetFullPath());
		return false;
	}

	StaticCastSharedRef<FMessagePacket>(Packet)->SetAddress(InAddress);
	return true;
}

const FOSCAddress& FOSCMessage::GetAddress() const
{
	using namespace UE::OSC;

	return StaticCastSharedRef<FMessagePacket>(Packet)->GetAddress();
}
