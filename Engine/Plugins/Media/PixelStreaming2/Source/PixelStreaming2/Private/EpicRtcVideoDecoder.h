// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EpicRtcMemory.h"
#include "Video/Decoders/Configs/VideoDecoderConfigAV1.h"
#include "Video/Decoders/Configs/VideoDecoderConfigH264.h"
#include "Video/Decoders/Configs/VideoDecoderConfigVP8.h"
#include "Video/Decoders/Configs/VideoDecoderConfigVP9.h"
#include "Video/Resources/VideoResourceCPU.h"
#include "Video/Resources/VideoResourceRHI.h"
#include "Video/VideoDecoder.h"

#include "epic_rtc/core/video/video_decoder.h"

namespace UE::PixelStreaming2
{
	template <std::derived_from<FVideoResource> TVideoResource>
	class TEpicRtcVideoDecoder : public EpicRtcVideoDecoderInterface, public TRefCountingMixin<TEpicRtcVideoDecoder<TVideoResource>>
	{
	public:
		TEpicRtcVideoDecoder(EpicRtcVideoCodecInfoInterface* CodecInfo);

		/* Begin EpicRtcVideoDecoderInterface */
		[[nodiscard]] virtual EpicRtcStringView GetName() const override;
		virtual EpicRtcVideoDecoderConfig		GetConfig() const override;
		virtual EpicRtcMediaResult				SetConfig(const EpicRtcVideoDecoderConfig& VideoDecoderConfig) override;
		virtual EpicRtcMediaResult				Decode(const EpicRtcEncodedVideoFrame& Frame) override;
		virtual void							RegisterCallback(EpicRtcVideoDecoderCallbackInterface* Callback) override;
		virtual void							Reset() override;
		/* End EpicRtcVideoDecoderInterface */
	private:
		TSharedPtr<TVideoDecoder<TVideoResource>>		   Decoder;
		TUniquePtr<FVideoDecoderConfig>					   InitialVideoConfig;
		EpicRtcVideoDecoderConfig						   DecoderConfig;
		TRefCountPtr<EpicRtcVideoDecoderCallbackInterface> VideoDecoderCallback;
		TRefCountPtr<EpicRtcVideoCodecInfoInterface>	   CodecInfo;
		uint16_t										   FrameCount;

	private:
		bool LateInitDecoder();

	public:
		/* Begin EpicRtcRefCountInterface */
		virtual uint32_t AddRef() override final { return TRefCountingMixin<TEpicRtcVideoDecoder>::AddRef(); }
		virtual uint32_t Release() override final { return TRefCountingMixin<TEpicRtcVideoDecoder>::Release(); }
		virtual uint32_t Count() const override final { return TRefCountingMixin<TEpicRtcVideoDecoder>::GetRefCount(); }
		/* End EpicRtcRefCountInterface */
	};
} // namespace UE::PixelStreaming2