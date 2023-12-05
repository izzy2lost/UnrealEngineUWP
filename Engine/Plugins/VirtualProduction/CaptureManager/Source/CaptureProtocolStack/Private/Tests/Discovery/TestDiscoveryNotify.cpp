// Copyright Epic Games, Inc. All Rights Reserved.

#include "Discovery/Messages/DiscoveryNotify.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TDiscoveryNotifyDeserializeTest, "Plugin.CaptureProtocolStack.Discovery.DiscoveryNotify.Deserialize.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TDiscoveryNotifyDeserializeTest::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'D', 'I', 'S', 'C', 'O', 'V', 'E', 'R', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Notify;
	const TArray<uint8> ServerId = { 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f, 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f };
	const uint16 ControlPort = 8000;
	const FDiscoveryNotify::EConnectionState ConnectionState = FDiscoveryNotify::EConnectionState::Online;
	const TArray<uint16> SupportedVersions = { 1, 2, 3 };

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType) + ServerId.Num() + sizeof(ControlPort) + SupportedVersions.Num() * sizeof(uint16));
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));
	Packet.Append(ServerId);
	Packet.Append((uint8*) &ControlPort, sizeof(ControlPort));
	Packet.Add(static_cast<uint8>(ConnectionState));
	Packet.Append((uint8*)SupportedVersions.GetData(), SupportedVersions.Num() * sizeof(uint16));

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryPacket::Deserialize(Packet);
	TestTrue(TEXT("Packet Deserialize"), PacketResult.HasValue());

	FDiscoveryPacket DiscoveryPacket = PacketResult.StealValue();
	TProtocolResult<FDiscoveryNotify> NotifyResult = FDiscoveryNotify::Deserialize(DiscoveryPacket);
	TestTrue(TEXT("Notify Deserialize"), NotifyResult.HasValue());

	FDiscoveryNotify Notify = NotifyResult.StealValue();
	int32 ServerIdCompareResult = FMemory::Memcmp(Notify.GetServerId().GetData(), ServerId.GetData(), ServerId.Num());
	TestTrue(TEXT("Notify Deserialize"), (ServerIdCompareResult == 0));
	TestEqual(TEXT("Notify Deserialize"), Notify.GetControlPort(), ControlPort);
	TestEqual(TEXT("Notify Deserialize"), Notify.GetConnectionState(), ConnectionState);
	TestEqual(TEXT("Notify Deserialize"), Notify.GetSupportedVersions(), SupportedVersions);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TDiscoveryNotifyDeserializeTestInvalidMessageType, "Plugin.CaptureProtocolStack.Discovery.DiscoveryNotify.Deserialize.InvalidMessageType", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TDiscoveryNotifyDeserializeTestInvalidMessageType::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'D', 'I', 'S', 'C', 'O', 'V', 'E', 'R', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Request;

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType));
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryPacket::Deserialize(Packet);
	TestTrue(TEXT("Packet Deserialize"), PacketResult.HasValue());

	FDiscoveryPacket DiscoveryPacket = PacketResult.StealValue();
	TProtocolResult<FDiscoveryNotify> NotifyResult = FDiscoveryNotify::Deserialize(DiscoveryPacket);
	TestTrue(TEXT("Notify Deserialize"), NotifyResult.HasError());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TDiscoveryNotifyDeserializeTestInvalidSize, "Plugin.CaptureProtocolStack.Discovery.DiscoveryNotify.Deserialize.InvalidSize", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TDiscoveryNotifyDeserializeTestInvalidSize::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'D', 'I', 'S', 'C', 'O', 'V', 'E', 'R', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Notify;
	const TArray<uint8> ServerId = { 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f };

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType));
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));
	Packet.Append(ServerId);

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryPacket::Deserialize(Packet);
	TestTrue(TEXT("Packet Deserialize"), PacketResult.HasValue());

	FDiscoveryPacket DiscoveryPacket = PacketResult.StealValue();
	TProtocolResult<FDiscoveryNotify> NotifyResult = FDiscoveryNotify::Deserialize(DiscoveryPacket);
	TestTrue(TEXT("Notify Deserialize"), NotifyResult.HasError());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TDiscoveryNotifyDeserializeTestInvalidConnectionState, "Plugin.CaptureProtocolStack.Discovery.DiscoveryNotify.Deserialize.InvalidConnectionState", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TDiscoveryNotifyDeserializeTestInvalidConnectionState::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'D', 'I', 'S', 'C', 'O', 'V', 'E', 'R', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Notify;
	const TArray<uint8> ServerId = { 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f, 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f };
	const uint16 ControlPort = 8000;
	const FDiscoveryNotify::EConnectionState ConnectionState = FDiscoveryNotify::EConnectionState::Invalid;
	const TArray<uint16> SupportedVersions = { 1, 2, 3 };

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType) + ServerId.Num() + sizeof(ControlPort) + SupportedVersions.Num() * sizeof(uint16));
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));
	Packet.Append(ServerId);
	Packet.Append((uint8*) &ControlPort, sizeof(ControlPort));
	Packet.Add(static_cast<uint8>(ConnectionState));
	Packet.Append((uint8*)SupportedVersions.GetData(), SupportedVersions.Num() * sizeof(uint16));

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryPacket::Deserialize(Packet);
	TestTrue(TEXT("Packet Deserialize"), PacketResult.HasValue());

	FDiscoveryPacket DiscoveryPacket = PacketResult.StealValue();
	TProtocolResult<FDiscoveryNotify> NotifyResult = FDiscoveryNotify::Deserialize(DiscoveryPacket);
	TestTrue(TEXT("Notify Deserialize"), NotifyResult.HasError());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TDiscoveryNotifySerializeTest, "Plugin.CaptureProtocolStack.Discovery.DiscoveryNotify.Serialize.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TDiscoveryNotifySerializeTest::RunTest(const FString& InParameters)
{
	// Prepare test
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Notify;

	const TArray<uint8> ServerId = { 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f, 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f };
	const uint16 ControlPort = 8000;
	const FDiscoveryNotify::EConnectionState ConnectionState = FDiscoveryNotify::EConnectionState::Online;
	const TArray<uint16> SupportedVersions = { 1 };

	TArray<uint8> Payload;
	Payload.Reserve(ServerId.Num() + sizeof(ControlPort) + sizeof(ConnectionState) + SupportedVersions.Num() * sizeof(uint16));
	Payload.Append(ServerId);
	Payload.Append((uint8*) &ControlPort, sizeof(ControlPort));
	Payload.Add(static_cast<uint8>(ConnectionState));
	Payload.Append((uint8*) SupportedVersions.GetData(), SupportedVersions.Num() * sizeof(uint16));

	FDiscoveryNotify::FServerId ServerIdStatic;
	FMemory::Memcpy(ServerIdStatic.GetData(), ServerId.GetData(), ServerId.Num());

	FDiscoveryNotify Notify(MoveTemp(ServerIdStatic), ControlPort, ConnectionState, SupportedVersions);

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryNotify::Serialize(Notify);
	TestTrue(TEXT("Packet Serialize"), PacketResult.HasValue());

	FDiscoveryPacket DiscoveryPacket = PacketResult.StealValue();
	TestEqual(TEXT("Notify Serialize"), DiscoveryPacket.GetMessageType(), MessageType);
	TestEqual(TEXT("Notify Serialize"), DiscoveryPacket.GetPayload(), Payload);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS