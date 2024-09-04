// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPixelStreaming2DataProtocol.h"
#include "Logging.h"
#include "Templates/SharedPointer.h"

#include "epic_rtc/core/data_track.h"

namespace UE::PixelStreaming2
{

	template <typename T>
	inline size_t ValueSize(T&& Value)
	{
		return sizeof(Value);
	}

	template <typename T>
	inline const void* ValueLoc(T&& Value)
	{
		return &Value;
	}

	inline size_t ValueSize(FString&& Value)
	{
		return Value.Len() * sizeof(TCHAR);
	}

	inline const void* ValueLoc(FString&& Value)
	{
		return *Value;
	}

	struct BufferBuilder
	{
		TArray<uint8> Buffer;
		size_t		  Pos;

		BufferBuilder(size_t size)
			: Pos(0)
		{
			Buffer.SetNum(size);
		}

		template <typename T>
		void Insert(T&& Value)
		{
			const size_t VSize = ValueSize(Forward<T>(Value));
			const void*	 VLoc = ValueLoc(Forward<T>(Value));
			check(Pos + VSize <= Buffer.Num());
			FMemory::Memcpy(Buffer.GetData() + Pos, VLoc, VSize);
			Pos += VSize;
		}
	};

	class FEpicRtcDataTrack : public TSharedFromThis<FEpicRtcDataTrack>
	{
	public:
		static TSharedPtr<FEpicRtcDataTrack> Create(TRefCountPtr<EpicRtcDataTrackInterface> InTrack, TWeakPtr<IPixelStreaming2DataProtocol> FromStreamerProtocol);

		/**
		 * Sends a series of arguments to the data channel with the given type.
		 * @param MessageType The name of the message you want to send. This message name must be registered IPixelStreaming2InputHandler::GetFromStreamerProtocol().
		 * @returns True of the message was successfully sent.
		 */
		template <typename... Args>
		bool SendMessage(FString MessageType, Args... VarArgs) const
		{
			if (!Track)
			{
				UE_LOG(LogPixelStreaming2, Error, TEXT("Cannot send message when datatrack is null."));
				return false;
			}
			if (Track->GetState() != EpicRtcTrackState::Active)
			{
				UE_LOG(LogPixelStreaming2, Error, TEXT("Cannot send message when is not active."));
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

			// Get the unique identifier of the message type we are about to send.
			uint8 Type = Message->GetID();

			BufferBuilder Builder(sizeof(Type) + (0 + ... + ValueSize(Forward<Args>(VarArgs))));
			Builder.Insert(Type);
			(Builder.Insert(Forward<Args>(VarArgs)), ...);

			EpicRtcDataFrameInput InputFrame = {
				._data = Builder.Buffer.GetData(),
				._size = static_cast<uint32>(Builder.Buffer.Num()),
				._binary = true
			};

			Track->PushFrame(InputFrame);

			return true;
		}

		/**
		 * Sends a large buffer of data to the data track, will chunk into multiple data frames if frames greater than 16KB.
		 * @param MessageType The name of the message, it must be registered in IPixelStreaming2InputHandler::GetTo/FromStreamerProtocol()
		 * @param DataBytes The raw byte buffer to send.
		 * @returns True of the message was successfully sent.
		 */
		bool SendArbitraryData(FString MessageType, const TArray64<uint8>& DataBytes) const;

		/**
		 * @return The id of the underlying EpicRtc data track.
		 */
		EpicRtcStringView GetId() const;

		/**
		 * @return The state of the underlying EpicRtc data track.
		 */
		EpicRtcTrackState GetState() const { return Track->GetState(); }

	protected:
		FEpicRtcDataTrack(TRefCountPtr<EpicRtcDataTrackInterface> InTrack, TWeakPtr<IPixelStreaming2DataProtocol> InDataProtocol);

	private:
		TRefCountPtr<EpicRtcDataTrackInterface>		 Track;
		TWeakPtr<IPixelStreaming2DataProtocol> WeakDataProtocol;
	};
} // namespace UE::PixelStreaming2