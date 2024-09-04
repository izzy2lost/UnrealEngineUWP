// Copyright Epic Games, Inc. All Rights Reserved.

#include "EpicRtcDataTrack.h"
#include "Utils.h"

namespace UE::PixelStreaming2
{

	TSharedPtr<FEpicRtcDataTrack> FEpicRtcDataTrack::Create(TRefCountPtr<EpicRtcDataTrackInterface> InTrack, TWeakPtr<IPixelStreaming2DataProtocol> InDataProtocol)
	{
		TSharedPtr<FEpicRtcDataTrack> DataTrack = TSharedPtr<FEpicRtcDataTrack>(new FEpicRtcDataTrack(InTrack, InDataProtocol));
		return DataTrack;
	}

	FEpicRtcDataTrack::FEpicRtcDataTrack(TRefCountPtr<EpicRtcDataTrackInterface> InTrack, TWeakPtr<IPixelStreaming2DataProtocol> InDataProtocol)
		: Track(InTrack)
		, WeakDataProtocol(InDataProtocol)
	{
	}

	EpicRtcStringView FEpicRtcDataTrack::GetId() const
	{
		return Track->GetId();
	}

	bool FEpicRtcDataTrack::SendArbitraryData(FString MessageType, const TArray64<uint8>& DataBytes) const
	{
		if (!Track || Track->GetState() != EpicRtcTrackState::Active)
		{
			UE_LOG(LogPixelStreaming2, Error, TEXT("Cannot send arbitrary data when datatrack is null."));
			return false;
		}

		TSharedPtr<IPixelStreaming2DataProtocol> DataProtocol = WeakDataProtocol.Pin();

		if (!DataProtocol)
		{
			UE_LOG(LogPixelStreaming2, Error, TEXT("Cannot send message, data protocol was null."));
			return false;
		}

		TSharedPtr<IPixelStreaming2InputMessage> Message = DataProtocol->Find(MessageType);
		if (!Message)
		{
			UE_LOG(LogPixelStreaming2, Error, TEXT("Cannot send message called '%s' as it is not in the data protocol. Try GetTo/FromStreamerProtocol()->Add()"), *MessageType);
			return false;
		}

		// The id of the message type we are about to send
		const uint8 Type = Message->GetID();

		// int32 results in a maximum 4GB file (4,294,967,296 bytes)
		const int32 DataSize = DataBytes.Num();

		// Maximum size of a single buffer should be 16KB as this is spec compliant message length for a single data channel transmission
		const int32 MaxBufferBytes = 16 * 1024;
		const int32 MessageHeader = sizeof(Type) + sizeof(DataSize);
		const int32 MaxDataBytesPerMsg = MaxBufferBytes - MessageHeader;

		int32 BytesTransmitted = 0;

		while (BytesTransmitted < DataSize)
		{
			int32 RemainingBytes = DataSize - BytesTransmitted;
			int32 BytesToTransmit = FGenericPlatformMath::Min(MaxDataBytesPerMsg, RemainingBytes);

			TArray<uint8> Buffer;
			Buffer.SetNum(MessageHeader + BytesToTransmit);

			size_t Pos = 0;

			// Write message type
			Pos = SerializeToBuffer(Buffer, Pos, &Type, sizeof(Type));

			// Write size of payload
			Pos = SerializeToBuffer(Buffer, Pos, &DataSize, sizeof(DataSize));

			// Write the data bytes payload
			Pos = SerializeToBuffer(Buffer, Pos, DataBytes.GetData() + BytesTransmitted, BytesToTransmit);

			// TODO (Migration): RTCP-6489 We may need EpicRtc API surface to query the buffered amount in the datachannel so we don't flood it.
			// uint64_t BufferBefore = SendChannel->buffered_amount();
			// while (BufferBefore + BytesToTransmit >= 16 * 1024 * 1024) // 16MB (WebRTC Data Channel buffer size)
			// {
			// 	// As per UE docs a Sleep of 0.0 simply lets other threads take CPU cycles while this is happening.
			// 	FPlatformProcess::Sleep(0.0);
			// 	BufferBefore = SendChannel->buffered_amount();
			// }

			EpicRtcDataFrameInput DataFrame{
				._data = Buffer.GetData(),
				._size = (uint32_t)Buffer.Num(),
				._binary = true
			};

			EpicRtcBool SendResult = Track->PushFrame(DataFrame);
			if (!SendResult)
			{
				UE_LOG(LogPixelStreaming2, Error, TEXT("DataTrack PushFrame return false"));
			}

			// Increment the number of bytes transmitted
			BytesTransmitted += BytesToTransmit;
		}

		return true;
	}

} // namespace UE::PixelStreaming2
