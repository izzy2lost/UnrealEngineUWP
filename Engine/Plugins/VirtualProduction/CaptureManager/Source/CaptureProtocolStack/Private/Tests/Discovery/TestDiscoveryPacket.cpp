// Copyright Epic Games, Inc. All Rights Reserved.

#include "Discovery/Communication/DiscoveryPacket.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TDiscoveryPacketDeserializeTest, "Plugin.CaptureProtocolStack.Discovery.DiscoveryPacket.Deserialize.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TDiscoveryPacketDeserializeTest::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'D', 'I', 'S', 'C', 'O', 'V', 'E', 'R', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Request;
	const TArray<uint8> Payload = { 0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f, 0x00 };

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType) + Payload.Num());
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));
	Packet.Append(Payload);

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryPacket::Deserialize(Packet);
	TestTrue(TEXT("Packet Deserialize"), PacketResult.HasValue());

	FDiscoveryPacket DiscoveryPacket = PacketResult.StealValue();
	TestEqual(TEXT("Packet Deserialize"), DiscoveryPacket.GetMessageType(), MessageType);
	TestEqual(TEXT("Packet Deserialize"), DiscoveryPacket.GetPayload().Num(), Payload.Num());
	TestEqual(TEXT("Packet Deserialize"), DiscoveryPacket.GetPayload(), Payload);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TDiscoveryPacketDeserializeTestInvalidHeaderSize, "Plugin.CaptureProtocolStack.Discovery.DiscoveryPacket.Deserialize.InvalidHeaderSize", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TDiscoveryPacketDeserializeTestInvalidHeaderSize::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'A', 'A', 'A', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Request;

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType));
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryPacket::Deserialize(Packet);
	TestTrue(TEXT("Packet Deserialize"), PacketResult.HasError());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TDiscoveryPacketDeserializeTestInvalidHeader, "Plugin.CaptureProtocolStack.Discovery.DiscoveryPacket.Deserialize.InvalidHeader", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TDiscoveryPacketDeserializeTestInvalidHeader::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Request;

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType));
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryPacket::Deserialize(Packet);
	TestTrue(TEXT("Packet Deserialize"), PacketResult.HasError());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TDiscoveryPacketDeserializeTestInvalidMessageType, "Plugin.CaptureProtocolStack.Discovery.DiscoveryPacket.Deserialize.InvalidMessageType", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TDiscoveryPacketDeserializeTestInvalidMessageType::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'D', 'I', 'S', 'C', 'O', 'V', 'E', 'R', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Invalid;

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType));
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));

	// Deserialization
	TProtocolResult<FDiscoveryPacket> PacketResult = FDiscoveryPacket::Deserialize(Packet);
	TestTrue(TEXT("Packet Deserialize"), PacketResult.HasError());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(TDiscoveryPacketSerializeTest, "Plugin.CaptureProtocolStack.Discovery.DiscoveryPacket.Serialize.Success", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool TDiscoveryPacketSerializeTest::RunTest(const FString& InParameters)
{
	// Prepare test
	const TArray<uint8> Header = { 'C', 'P', 'S', 'D', 'I', 'S', 'C', 'O', 'V', 'E', 'R', '\0' };
	const FDiscoveryPacket::EMessageType MessageType = FDiscoveryPacket::EMessageType::Request;

	TArray<uint8> Packet;
	Packet.Reserve(Header.Num() + sizeof(MessageType));
	Packet.Append(Header);
	Packet.Add(static_cast<uint8>(MessageType));

	FDiscoveryPacket DiscoveryPacket(MessageType, TArray<uint8>());

	// Deserialization
	TProtocolResult<TArray<uint8>> DataResult = FDiscoveryPacket::Serialize(DiscoveryPacket);
	TestTrue(TEXT("Packet Serialize"), DataResult.HasValue());

	TArray<uint8> Data = DataResult.StealValue();
	TestEqual(TEXT("Packet Serialize"), Packet, Data);

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS