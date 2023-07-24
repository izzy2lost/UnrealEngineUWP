// Copyright Epic Games, Inc. All Rights Reserved.


#include "OpusAudioInfo.h"
#include "Interfaces/IAudioFormat.h"
#include <opus_defines.h>
#include <opus_types.h>

THIRD_PARTY_INCLUDES_START
#include "opus_multistream.h"
THIRD_PARTY_INCLUDES_END

#define USE_UE_MEM_ALLOC 1
#define OPUS_MAX_FRAME_SIZE_MS 120

///////////////////////////////////////////////////////////////////////////////////////
// Followed pattern used in opus_multistream_encoder.c - this will allow us to setup //
// a multistream decoder without having to save extra information for every asset.   //
///////////////////////////////////////////////////////////////////////////////////////
struct UnrealChannelLayout{
	int32 NumStreams;
	int32 NumCoupledStreams;
	uint8 Mapping[8];
};

/* Index is NumChannels-1*/
static const UnrealChannelLayout UnrealMappings[8] = {
	{ 1, 0, { 0 } },                      /* 1: mono */
	{ 1, 1, { 0, 1 } },                   /* 2: stereo */
	{ 2, 1, { 0, 1, 2 } },                /* 3: 1-d surround */
	{ 2, 2, { 0, 1, 2, 3 } },             /* 4: quadraphonic surround */
	{ 3, 2, { 0, 1, 4, 2, 3 } },          /* 5: 5-channel surround */
	{ 4, 2, { 0, 1, 4, 5, 2, 3 } },       /* 6: 5.1 surround */
	{ 4, 3, { 0, 1, 4, 6, 2, 3, 5 } },    /* 7: 6.1 surround */
	{ 5, 3, { 0, 1, 6, 7, 2, 3, 4, 5 } }, /* 8: 7.1 surround */
};

/*------------------------------------------------------------------------------------
FOpusDecoderWrapper
------------------------------------------------------------------------------------*/
struct FOpusDecoderWrapper
{
	FOpusDecoderWrapper(uint16 SampleRate, uint8 NumChannels)
	{
		check(NumChannels <= 8);
		const UnrealChannelLayout& Layout = UnrealMappings[NumChannels-1];
	#if USE_UE_MEM_ALLOC
		int32 DecSize = opus_multistream_decoder_get_size(Layout.NumStreams, Layout.NumCoupledStreams);
		Decoder = (OpusMSDecoder*)FMemory::Malloc(DecSize);
		DecError = opus_multistream_decoder_init(Decoder, SampleRate, NumChannels, Layout.NumStreams, Layout.NumCoupledStreams, Layout.Mapping);
	#else
		Decoder = opus_multistream_decoder_create(SampleRate, NumChannels, Layout.NumStreams, Layout.NumCoupledStreams, Layout.Mapping, &DecError);
	#endif
	}

	~FOpusDecoderWrapper()
	{
	#if USE_UE_MEM_ALLOC
		FMemory::Free(Decoder);
	#else
		opus_multistream_encoder_destroy(Decoder);
	#endif
	}

	int32 Decode(const uint8* FrameData, uint16 FrameSize, int16* OutPCMData, int32 SampleSize)
	{
		return opus_multistream_decode(Decoder, FrameData, FrameSize, OutPCMData, SampleSize, 0);
	}

	bool WasInitialisedSuccessfully() const
	{
		return DecError == OPUS_OK;
	}

private:
	OpusMSDecoder* Decoder;
	int32 DecError;
};

/*------------------------------------------------------------------------------------
FOpusAudioInfo.
------------------------------------------------------------------------------------*/
FOpusAudioInfo::FOpusAudioInfo()
	: OpusDecoderWrapper(nullptr)
{
}

FOpusAudioInfo::~FOpusAudioInfo()
{
	if (OpusDecoderWrapper != nullptr)
	{
		delete OpusDecoderWrapper;
		OpusDecoderWrapper = nullptr;
	}
}


bool FOpusAudioInfo::ParseHeader(FHeader& OutHeader, uint32& OutNumRead, const uint8* InSrcBufferData, uint32 InSrcBufferDataSize)
{
	OutNumRead = 0;

	if ((int32)InSrcBufferDataSize < FHeader::HeaderSize())
	{
		return false;
	}

	auto Read = [&InSrcBufferData, &OutNumRead](void* To, int32 NumBytes) -> void
	{
		FMemory::Memcpy(To, InSrcBufferData, NumBytes);
		InSrcBufferData += NumBytes;
		OutNumRead += NumBytes;
	};

	Read(OutHeader.Identifier, 8);
	if (FMemory::Memcmp(OutHeader.Identifier, FHeader::OPUS_ID, 8))
	{
		return false;
	}
	Read(&OutHeader.Version, sizeof(uint8));
	Read(&OutHeader.NumChannels, sizeof(uint8));
	Read(&OutHeader.SampleRate, sizeof(uint16));
	Read(&OutHeader.ActiveSampleCount, sizeof(uint64));
	Read(&OutHeader.NumEncodedFrames, sizeof(uint32));
	Read(&OutHeader.NumSilentSamplesAtBeginning, sizeof(int32));
	Read(&OutHeader.NumSilentSamplesAtEnd, sizeof(int32));
	return true;
}

bool FOpusAudioInfo::ParseHeader(const uint8* InSrcBufferData, uint32 InSrcBufferDataSize, struct FSoundQualityInfo* QualityInfo)
{
	SrcBufferData = InSrcBufferData;
	SrcBufferDataSize = InSrcBufferDataSize;
	SrcBufferOffset = 0;
	CurrentSampleCount = 0;

	Header.Reset();
	if (!ParseHeader(Header, SrcBufferOffset, InSrcBufferData, InSrcBufferDataSize))
	{
		return false;
	}

	// Store the offset to where the audio data begins
	AudioDataOffset = SrcBufferOffset;
	// Set members from header
	TrueSampleCount = Header.ActiveSampleCount;
	NumChannels = Header.NumChannels;

	// Write out the the header info
	if (QualityInfo)
	{
		QualityInfo->SampleRate = Header.SampleRate;
		QualityInfo->NumChannels = Header.NumChannels;
		QualityInfo->SampleDataSize = (uint32)Header.ActiveSampleCount * QualityInfo->NumChannels * sizeof(int16);
		QualityInfo->Duration = (float)Header.ActiveSampleCount / QualityInfo->SampleRate;
	}

	return true;
}

bool FOpusAudioInfo::CreateDecoder()
{
	check(OpusDecoderWrapper == nullptr);
	OpusDecoderWrapper = new FOpusDecoderWrapper(Header.SampleRate, NumChannels);
	if (!OpusDecoderWrapper->WasInitialisedSuccessfully())
	{
		delete OpusDecoderWrapper;
		OpusDecoderWrapper = nullptr;
		return false;
	}

	NumRemainingSamplesToSkip = Header.NumSilentSamplesAtBeginning;
	return true;
}

int32 FOpusAudioInfo::GetFrameSize()
{
	// Opus format has variable frame size at the head of each frame...
	// We have to assume that the SrcBufferOffset is at the correct location for the read
	uint16 FrameSize = 0;
	Read(&FrameSize, sizeof(uint16));
	return (int32)FrameSize;
}

uint32 FOpusAudioInfo::GetMaxFrameSizeSamples() const
{
	return Header.SampleRate * OPUS_MAX_FRAME_SIZE_MS / 1000;
}

FDecodeResult FOpusAudioInfo::Decode(const uint8* CompressedData, const int32 CompressedDataSize, uint8* OutPCMData, const int32 OutputPCMDataSize)
{
	FDecodeResult Result;

	if (OpusDecoderWrapper)
	{
		const int32 SampleSize = OutputPCMDataSize / NumChannels * sizeof(int16);
		Result.NumCompressedBytesConsumed = CompressedDataSize;
		Result.NumAudioFramesProduced = OpusDecoderWrapper->Decode(CompressedData, CompressedDataSize, (int16*)OutPCMData, SampleSize);
		if (NumRemainingSamplesToSkip)
		{
			if (NumRemainingSamplesToSkip >= Result.NumAudioFramesProduced)
			{
				NumRemainingSamplesToSkip -= Result.NumAudioFramesProduced;
				Result.NumAudioFramesProduced = 0;
			}
			else
			{
				uint8* FirstUsable = OutPCMData + NumRemainingSamplesToSkip * NumChannels * sizeof(int16);
				int32 UsableSize = (Result.NumAudioFramesProduced - NumRemainingSamplesToSkip) * NumChannels * sizeof(int16);
				FMemory::Memmove(OutPCMData, FirstUsable, UsableSize);
				Result.NumAudioFramesProduced -= NumRemainingSamplesToSkip;
				NumRemainingSamplesToSkip = 0;
			}
		}
		Result.NumPcmBytesProduced = Result.NumAudioFramesProduced * NumChannels * sizeof(int16);
	}
	return Result;
}

void FOpusAudioInfo::PrepareToLoop()
{
	IStreamedCompressedInfo::PrepareToLoop();
	NumRemainingSamplesToSkip = Header.NumSilentSamplesAtBeginning;
}

void FOpusAudioInfo::SeekToTime(const float InSeekTime)
{
	IStreamedCompressedInfo::SeekToTime(InSeekTime);
	NumRemainingSamplesToSkip = Header.NumSilentSamplesAtBeginning;
}

