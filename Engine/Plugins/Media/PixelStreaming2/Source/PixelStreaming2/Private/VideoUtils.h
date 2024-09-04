// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "PixelStreaming2PluginSettings.h"
#include "IPixelStreaming2Streamer.h"
#include "EpicRtcVideoCommon.h"
#include "Video/VideoEncoder.h"

#include "epic_rtc/core/video/video_common.h"
#include "epic_rtc/core/video/video_rate_control.h"

FORCEINLINE bool operator==(const EpicRtcVideoResolution& Lhs, const EpicRtcVideoResolution& Rhs)
{
	return Lhs._width == Rhs._width && Lhs._height == Rhs._height;
}

namespace UE::PixelStreaming2
{
	// HACK (aidan.possemiers) the AVCodecs API surface wants a SharedPtr for the encoded data but EpicRtc already owns that and we don't want AVCodecs to delete it
	struct FFakeDeleter
	{
		void operator()(uint8* Object) const
		{
		}
	};

	constexpr uint32_t NumSimulcastLayers = 3;
	// Each subsequent layer is 1/ScalingFactor the size of the previous
	constexpr uint32_t ScalingFactor = 2;

	// Helper array for all scalability modes. EScalabilityMode::None must always be the last entry
	const TArray<EScalabilityMode> AllScalabilityModes = {
		EScalabilityMode::L1T1,
		EScalabilityMode::L1T2,
		EScalabilityMode::L1T3,
		EScalabilityMode::L2T1,
		EScalabilityMode::L2T1h,
		EScalabilityMode::L2T1_KEY,
		EScalabilityMode::L2T2,
		EScalabilityMode::L2T2h,
		EScalabilityMode::L2T2_KEY,
		EScalabilityMode::L2T2_KEY_SHIFT,
		EScalabilityMode::L2T3,
		EScalabilityMode::L2T3h,
		EScalabilityMode::L2T3_KEY,
		EScalabilityMode::L3T1,
		EScalabilityMode::L3T1h,
		EScalabilityMode::L3T1_KEY,
		EScalabilityMode::L3T2,
		EScalabilityMode::L3T2h,
		EScalabilityMode::L3T2_KEY,
		EScalabilityMode::L3T3,
		EScalabilityMode::L3T3h,
		EScalabilityMode::L3T3_KEY,
		EScalabilityMode::S2T1,
		EScalabilityMode::S2T1h,
		EScalabilityMode::S2T2,
		EScalabilityMode::S2T2h,
		EScalabilityMode::S2T3,
		EScalabilityMode::S2T3h,
		EScalabilityMode::S3T1,
		EScalabilityMode::S3T1h,
		EScalabilityMode::S3T2,
		EScalabilityMode::S3T2h,
		EScalabilityMode::S3T3,
		EScalabilityMode::S3T3h,
		EScalabilityMode::None
	};

	// Make sure EpicRtcVideoScalabilityMode and EScalabilityMode match up
	static_assert(EpicRtcVideoScalabilityMode::L1T1 == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L1T1));
	static_assert(EpicRtcVideoScalabilityMode::L1T2 == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L1T2));
	static_assert(EpicRtcVideoScalabilityMode::L1T3 == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L1T3));
	static_assert(EpicRtcVideoScalabilityMode::L2T1 == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L2T1));
	static_assert(EpicRtcVideoScalabilityMode::L2T1h == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L2T1h));
	static_assert(EpicRtcVideoScalabilityMode::L2T1Key == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L2T1_KEY));
	static_assert(EpicRtcVideoScalabilityMode::L2T2 == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L2T2));
	static_assert(EpicRtcVideoScalabilityMode::L2T2h == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L2T2h));
	static_assert(EpicRtcVideoScalabilityMode::L2T2Key == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L2T2_KEY));
	static_assert(EpicRtcVideoScalabilityMode::L2T2KeyShift == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L2T2_KEY_SHIFT));
	static_assert(EpicRtcVideoScalabilityMode::L2T3 == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L2T3));
	static_assert(EpicRtcVideoScalabilityMode::L2T3h == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L2T3h));
	static_assert(EpicRtcVideoScalabilityMode::L2T3Key == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L2T3_KEY));
	static_assert(EpicRtcVideoScalabilityMode::L3T1 == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L3T1));
	static_assert(EpicRtcVideoScalabilityMode::L3T1h == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L3T1h));
	static_assert(EpicRtcVideoScalabilityMode::L3T1Key == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L3T1_KEY));
	static_assert(EpicRtcVideoScalabilityMode::L3T2 == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L3T2));
	static_assert(EpicRtcVideoScalabilityMode::L3T2h == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L3T2h));
	static_assert(EpicRtcVideoScalabilityMode::L3T2Key == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L3T2_KEY));
	static_assert(EpicRtcVideoScalabilityMode::L3T3 == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L3T3));
	static_assert(EpicRtcVideoScalabilityMode::L3T3h == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L3T3h));
	static_assert(EpicRtcVideoScalabilityMode::L3T3Key == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::L3T3_KEY));
	static_assert(EpicRtcVideoScalabilityMode::S2T1 == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::S2T1));
	static_assert(EpicRtcVideoScalabilityMode::S2T1h == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::S2T1h));
	static_assert(EpicRtcVideoScalabilityMode::S2T2 == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::S2T2));
	static_assert(EpicRtcVideoScalabilityMode::S2T2h == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::S2T2h));
	static_assert(EpicRtcVideoScalabilityMode::S2T3 == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::S2T3));
	static_assert(EpicRtcVideoScalabilityMode::S2T3h == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::S2T3h));
	static_assert(EpicRtcVideoScalabilityMode::S3T1 == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::S3T1));
	static_assert(EpicRtcVideoScalabilityMode::S3T1h == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::S3T1h));
	static_assert(EpicRtcVideoScalabilityMode::S3T2 == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::S3T2));
	static_assert(EpicRtcVideoScalabilityMode::S3T2h == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::S3T2h));
	static_assert(EpicRtcVideoScalabilityMode::S3T3 == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::S3T3));
	static_assert(EpicRtcVideoScalabilityMode::S3T3h == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::S3T3h));
	static_assert(EpicRtcVideoScalabilityMode::None == static_cast<EpicRtcVideoScalabilityMode>(EScalabilityMode::None));

	/**
	 * A struct representing the simulcast paramaters of a single simulcast layer used by PixelStreaming2.
	 * Specifically, each layer has a `Scaling`, `MinBitrate` and `MaxBitrate`.
	 */
	struct FPixelStreaming2SimulcastLayer
	{
		float Scaling;
		int	  MinBitrate;
		int	  MaxBitrate;
	};

	inline TArray<FPixelStreaming2SimulcastLayer> GetSimulcastParameters()
	{
		TArray<FPixelStreaming2SimulcastLayer> SimulcastParams;

		if (UPixelStreaming2PluginSettings::CVarEncoderEnableSimulcast.GetValueOnAnyThread())
		{
			int MinBps = UPixelStreaming2PluginSettings::CVarWebRTCMinBitrate.GetValueOnAnyThread();
			int MaxBps = UPixelStreaming2PluginSettings::CVarWebRTCMaxBitrate.GetValueOnAnyThread();
			int BpsRange = MaxBps - MinBps;

			// Bitrates assignment per layer. Effectively
			// 0: 0 -> 1/4
			// 1: 1/4 -> 1/2
			// 2: 1/2 -> 1
			TArray<TTuple<int, int>> Bitrates = {
				{ MinBps, MinBps + (BpsRange / 4) },
				{ MinBps + (BpsRange / 4), MinBps + (BpsRange / 2) },
				{ MinBps + (BpsRange / 2), MaxBps }
			};

			for (int i = 0; i < NumSimulcastLayers; i++)
			{
				// EpicRtc expects the layers to be added in order of scaling factors from largest to smallest (ie smallest res to largest res)
				float Scaling = ScalingFactor * (NumSimulcastLayers - i - 1);

				// clang-format off
				SimulcastParams.Add({
					.Scaling = Scaling > 0 ? Scaling : 1.f,
					.MinBitrate = Bitrates[i].Get<0>(),
					.MaxBitrate = Bitrates[i].Get<1>(),
				});
				// clang-format on
			}
		}
		else
		{
			// clang-format off
			SimulcastParams.Add({ 
				.Scaling = 1.f,
				.MinBitrate = UPixelStreaming2PluginSettings::CVarWebRTCMinBitrate.GetValueOnAnyThread(),
				.MaxBitrate = UPixelStreaming2PluginSettings::CVarWebRTCMaxBitrate.GetValueOnAnyThread() 
			});
			// clang-format on
		}

		return SimulcastParams;
	}

	class FGenericFrameInfoWrapper : public EpicRtcGenericFrameInfoInterface, public TRefCountingMixin<FGenericFrameInfoWrapper>
	{
	public:
		FGenericFrameInfoWrapper(const FGenericFrameInfo& GenericFrameInfo)
			: SpatialId(GenericFrameInfo.SpatialId)
			, TemporalId(GenericFrameInfo.TemporalId)
			, DecodeTargetIndications(MakeRefCount<FEpicRtcDecodeTargetIndicationArray>(GenericFrameInfo.DecodeTargetIndications))
			, FrameDiffs(MakeRefCount<FEpicRtcInt32Array>(GenericFrameInfo.FrameDiffs))
			, ChainDiffs(MakeRefCount<FEpicRtcInt32Array>(GenericFrameInfo.ChainDiffs))
			, EncoderBuffers(MakeRefCount<FEpicRtcCodecBufferUsageArray>(GenericFrameInfo.EncoderBuffers))
			, PartOfChain(MakeRefCount<FEpicRtcBoolArray>(GenericFrameInfo.PartOfChain))
			, ActiveDecodeTargets(MakeRefCount<FEpicRtcBoolArray>(GenericFrameInfo.ActiveDecodeTargets))
		{
		}

		virtual int32_t										 GetSpatialLayerId() override { return SpatialId; }
		virtual int32_t										 GetTemporalLayerId() override { return TemporalId; }
		virtual EpicRtcDecodeTargetIndicationArrayInterface* GetDecodeTargetIndications() override { return DecodeTargetIndications; }
		virtual EpicRtcInt32ArrayInterface*					 GetFrameDiffs() override { return FrameDiffs; }
		virtual EpicRtcInt32ArrayInterface*					 GetChainDiffs() override { return ChainDiffs; }
		virtual EpicRtcCodecBufferUsageArrayInterface*		 GetEncoderBufferUsages() override { return EncoderBuffers; }
		virtual EpicRtcBoolArrayInterface*					 GetPartOfChain() override { return PartOfChain; }
		virtual EpicRtcBoolArrayInterface*					 GetActiveDecodeTargets() override { return ActiveDecodeTargets; }

	private:
		int32_t											  SpatialId;
		int32_t											  TemporalId;
		TRefCountPtr<FEpicRtcDecodeTargetIndicationArray> DecodeTargetIndications;
		TRefCountPtr<FEpicRtcInt32Array>				  FrameDiffs;
		TRefCountPtr<FEpicRtcInt32Array>				  ChainDiffs;
		TRefCountPtr<FEpicRtcCodecBufferUsageArray>		  EncoderBuffers;
		TRefCountPtr<FEpicRtcBoolArray>					  PartOfChain;
		TRefCountPtr<FEpicRtcBoolArray>					  ActiveDecodeTargets;

	public:
		/* Begin EpicRtcRefCountInterface */
		virtual uint32_t AddRef() override final { return TRefCountingMixin<FGenericFrameInfoWrapper>::AddRef(); }
		virtual uint32_t Release() override final { return TRefCountingMixin<FGenericFrameInfoWrapper>::Release(); }
		virtual uint32_t Count() const override final { return TRefCountingMixin<FGenericFrameInfoWrapper>::GetRefCount(); }
		/* End EpicRtcRefCountInterface */
	};

	class FFrameDependencyStructureWrapper : public EpicRtcFrameDependencyStructure, public TRefCountingMixin<FFrameDependencyStructureWrapper>
	{
	public:
		FFrameDependencyStructureWrapper(const FFrameDependencyStructure& FrameDependencyStructure)
			: StructureId(FrameDependencyStructure.StructureId)
			, NumDecodeTargets(FrameDependencyStructure.NumDecodeTargets)
			, NumChains(FrameDependencyStructure.NumChains)
			, DecodeTargetProtectedByChain(MakeRefCount<FEpicRtcInt32Array>(FrameDependencyStructure.DecodeTargetProtectedByChain))
			, Resolutions(MakeRefCount<FEpicRtcVideoResolutionArray>(FrameDependencyStructure.Resolutions))
		{
			TArray<EpicRtcGenericFrameInfoInterface*> GenericFrameInfoArray;
			GenericFrameInfoArray.SetNum(FrameDependencyStructure.Templates.Num());

			for (size_t i = 0; i < FrameDependencyStructure.Templates.Num(); i++)
			{
				FGenericFrameInfo GenericFrameInfo;
				GenericFrameInfo.SpatialId = FrameDependencyStructure.Templates[i].SpatialId;
				GenericFrameInfo.TemporalId = FrameDependencyStructure.Templates[i].TemporalId;
				GenericFrameInfo.DecodeTargetIndications = FrameDependencyStructure.Templates[i].DecodeTargetIndications;
				GenericFrameInfo.FrameDiffs = FrameDependencyStructure.Templates[i].FrameDiffs;
				GenericFrameInfo.ChainDiffs = FrameDependencyStructure.Templates[i].ChainDiffs;

				GenericFrameInfoArray[i] = new FGenericFrameInfoWrapper(GenericFrameInfo);
			}

			Templates = MakeRefCount<FEpicRtcGenericFrameInfoArray>(GenericFrameInfoArray);
		}

		virtual int32_t								   GetStructureId() override { return StructureId; }
		virtual int32_t								   GetNumDecodeTargets() override { return NumDecodeTargets; }
		virtual int32_t								   GetNumChains() override { return NumChains; }
		virtual EpicRtcInt32ArrayInterface*			   GetDecodeTargetProtectedByChain() override { return DecodeTargetProtectedByChain; }
		virtual EpicRtcVideoResolutionArrayInterface*  GetResolutions() override { return Resolutions; }
		virtual EpicRtcGenericFrameInfoArrayInterface* GetTemplates() override { return Templates; }

		friend bool operator==(FFrameDependencyStructureWrapper& Lhs, FFrameDependencyStructureWrapper& Rhs)
		{
			TArray<int32_t> LhsDecodeTargetProtectedByChain(Lhs.GetDecodeTargetProtectedByChain()->Get(), Lhs.GetDecodeTargetProtectedByChain()->Size());
			TArray<int32_t> RhsDecodeTargetProtectedByChain(Rhs.GetDecodeTargetProtectedByChain()->Get(), Rhs.GetDecodeTargetProtectedByChain()->Size());

			TArray<EpicRtcVideoResolution> LhsResolutions(Lhs.GetResolutions()->Get(), Lhs.GetResolutions()->Size());
			TArray<EpicRtcVideoResolution> RhsResolutions(Rhs.GetResolutions()->Get(), Rhs.GetResolutions()->Size());

			TArray<EpicRtcGenericFrameInfoInterface*> LhsTemplates(Lhs.GetTemplates()->Get(), Lhs.GetTemplates()->Size());
			TArray<EpicRtcGenericFrameInfoInterface*> RhsTemplates(Rhs.GetTemplates()->Get(), Rhs.GetTemplates()->Size());

			return Lhs.NumDecodeTargets == Rhs.NumDecodeTargets
				&& Lhs.NumChains == Rhs.NumChains
				&& LhsDecodeTargetProtectedByChain == RhsDecodeTargetProtectedByChain
				&& LhsResolutions == RhsResolutions
				&& LhsTemplates == RhsTemplates;
		}

	private:
		int											StructureId;
		int											NumDecodeTargets;
		int											NumChains;
		TRefCountPtr<FEpicRtcInt32Array>			DecodeTargetProtectedByChain;
		TRefCountPtr<FEpicRtcVideoResolutionArray>	Resolutions;
		TRefCountPtr<FEpicRtcGenericFrameInfoArray> Templates;

	public:
		/* Begin EpicRtcRefCountInterface */
		virtual uint32_t AddRef() override final { return TRefCountingMixin<FFrameDependencyStructureWrapper>::AddRef(); }
		virtual uint32_t Release() override final { return TRefCountingMixin<FFrameDependencyStructureWrapper>::Release(); }
		virtual uint32_t Count() const override final { return TRefCountingMixin<FFrameDependencyStructureWrapper>::GetRefCount(); }
		/* End EpicRtcRefCountInterface */
	};
} // namespace UE::PixelStreaming2