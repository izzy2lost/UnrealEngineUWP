// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EpicRtcVideoCommon.h"

#include "Containers/StaticArray.h"
#include "Video/VideoConfig.h"

#include "epic_rtc/core/video/video_encoder.h"

namespace UE::PixelStreaming2
{

	class PIXELSTREAMING2_API FEpicRtcVideoEncoderInitializer : public EpicRtcVideoEncoderInitializerInterface, public TRefCountingMixin<FEpicRtcVideoEncoderInitializer>
	{
	public:
		FEpicRtcVideoEncoderInitializer() = default;
		virtual ~FEpicRtcVideoEncoderInitializer() = default;

		/* Begin EpicRtcVideoEncoderInitializerInterface */
		virtual void								 CreateEncoder(EpicRtcVideoCodecInfoInterface* CodecInfo, EpicRtcVideoEncoderInterface** OutEncoder) override;
		virtual EpicRtcStringView					 GetName() override;
		virtual EpicRtcVideoCodecInfoArrayInterface* GetSupportedCodecs() override;
		/* End EpicRtcVideoEncoderInitializerInterface */

	private:
		// The list of each individual codec we have support for (order of this array is preference order after selected codec)
		static TStaticArray<EVideoCodec, 4> SupportedCodecList;

	private:
		TMap<EVideoCodec, TArray<TRefCountPtr<EpicRtcVideoCodecInfoInterface>>> CreateSupportedEncoderMap();

	public:
		/* Begin EpicRtcRefCountInterface */
		virtual uint32_t AddRef() override final { return TRefCountingMixin<FEpicRtcVideoEncoderInitializer>::AddRef(); }
		virtual uint32_t Release() override final { return TRefCountingMixin<FEpicRtcVideoEncoderInitializer>::Release(); }
		virtual uint32_t Count() const override final { return TRefCountingMixin<FEpicRtcVideoEncoderInitializer>::GetRefCount(); }
		/* End EpicRtcRefCountInterface */
	};

} // namespace UE::PixelStreaming2