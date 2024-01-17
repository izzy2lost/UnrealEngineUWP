// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundParamHelper.h"
#include "MetasoundSampleCounter.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundVertex.h"

#include "HarmonixMetasound/Common.h"
#include "HarmonixMetasound/DataTypes/MidiAsset.h"
#include "HarmonixMetasound/DataTypes/MidiClock.h"
#include "HarmonixMetasound/DataTypes/MusicTransport.h"
#include "HarmonixMetasound/DataTypes/MusicTimestamp.h"
#include <algorithm>

#define LOCTEXT_NAMESPACE "HarmonixMetaSound"

DEFINE_LOG_CATEGORY_STATIC(MediaStreamerTransportControllerOperator, Log, All);

namespace HarmonixMetasound
{
	using namespace Metasound;

	namespace MediaStreamerTransportControllerVertexNames
	{
		METASOUND_PARAM(InputTriggerOnStreamStarts, "On Stream Starts", "Triggers when stream starts delivering data.")
		METASOUND_PARAM(InputTriggerOnStreamStops, "On Stream Stops", "Triggers when stream stops delivering data.")
		METASOUND_PARAM(InputPlaybackTime, "Playback Time", "Returns the current PTS of the meda stream.")
		METASOUND_PARAM(InputPlaybackTimeEndBlock, "Playback Time at end of block", "Returns the current PTS of the meda stream at the end of the current block.")
		METASOUND_PARAM(InputPlaybackRate, "Playback Rate", "Returns the current playback rate of the meda stream.")
	}

	class FMediaStreamerTransportControllerOperator : public TExecutableOperator<FMediaStreamerTransportControllerOperator>
	{
	public:
		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface& GetVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults);

		FMediaStreamerTransportControllerOperator(
			const FBuildOperatorParams& InParams,
			const FMidiAssetReadRef& InMidiAsset,
			const FInt32ReadRef& InPrerollBars,
			const FTriggerReadRef& InTriggerOnStreamStarts,
			const FTriggerReadRef& InTriggerOnStreamStops,
			const FTimeReadRef& InPlaybackTime,
			const FTimeReadRef& InPlaybackTimeEndBlock,
			const FFloatReadRef& InPlaybackRate);

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override;
		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override;
		virtual FDataReferenceCollection GetInputs() const override;
		virtual FDataReferenceCollection GetOutputs() const override;

		void Reset(const FResetParams& ResetParams);
		void Execute();

	private:
		//** INPUTS
		FMidiAssetReadRef MidiAssetInPin;
		FInt32ReadRef SeekPrerollBarsInPin;
		FTriggerReadRef TriggerOnStreamStartsInPin;
		FTriggerReadRef TriggerOnStreamStopsInPin;
		FTimeReadRef PlaybackTimeInPin;
		FTimeReadRef PlaybackTimeEndBlockInPin;
		FFloatReadRef PlaybackRateInPin;

		//** OUTPUTS
		FMusicTransportEventStreamWriteRef TransportOutPin;
		FMidiClockWriteRef MidiClockOutPin;

		//** DATA
		FSampleCount BlockSize;
		float        SampleRate;
		FSampleCount SampleCount;
		static const int32 kMidiGranularity = 128;

		FTime        StopTime;

		EMusicPlayerTransportRequest LastTransportRequest = EMusicPlayerTransportRequest::None;
	};

	class FMediaStreamerTransportControllerNode : public FNodeFacade
	{
	public:
		FMediaStreamerTransportControllerNode(const FNodeInitData& InInitData) :
			FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FMediaStreamerTransportControllerOperator>())
		{}
		virtual ~FMediaStreamerTransportControllerNode() = default;
	};

	METASOUND_REGISTER_NODE(FMediaStreamerTransportControllerNode)
}

const Metasound::FNodeClassMetadata& HarmonixMetasound::FMediaStreamerTransportControllerOperator::GetNodeInfo()
{
	auto InitNodeInfo = []() -> FNodeClassMetadata
	{
		FNodeClassMetadata Info;
		Info.ClassName = { HarmonixNodeNamespace, TEXT("MediaStreamerTransportController"), TEXT("") };
		Info.MajorVersion = 0;
		Info.MinorVersion = 1;
		Info.DisplayName = METASOUND_LOCTEXT("MediaStreamerTransportControllerNode_DisplayName", "Media Streamer Transport Controller");
		Info.Description = METASOUND_LOCTEXT("MediaStreamerTransportControllerNode_Description", "Provides a transport and midi clock controlled by signals from a Media Streamer using the tempo map in the specified Midi file.");
		Info.Author = PluginAuthor;
		Info.PromptIfMissing = PluginNodeMissingPrompt;
		Info.DefaultInterface = GetVertexInterface();
		Info.CategoryHierarchy.Emplace(NodeCategories::Music);

		return Info;
	};

	static const FNodeClassMetadata Info = InitNodeInfo();

	return Info;
}

const Metasound::FVertexInterface& HarmonixMetasound::FMediaStreamerTransportControllerOperator::GetVertexInterface()
{
	using namespace CommonPinNames;
	using namespace MediaStreamerTransportControllerVertexNames;

	static const FVertexInterface Interface(
		FInputVertexInterface(
			TInputDataVertex<FMidiAsset>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiFileAsset)),
			TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::PrerollBars), 8),
			TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerOnStreamStarts)),
			TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerOnStreamStops)),
			TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputPlaybackTime)),
			TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputPlaybackTimeEndBlock)),
			TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputPlaybackRate))
		),
		FOutputVertexInterface(
			TOutputDataVertex<FMusicTransportEventStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Outputs::Transport)),
			TOutputDataVertex<FMidiClock>(METASOUND_GET_PARAM_NAME_AND_METADATA(Outputs::MidiClock))
		)
	);

	return Interface;
}

TUniquePtr<Metasound::IOperator> HarmonixMetasound::FMediaStreamerTransportControllerOperator::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
{
	using namespace CommonPinNames;
	using namespace MediaStreamerTransportControllerVertexNames;

	const FInputVertexInterfaceData& InputData = InParams.InputData;

	FMidiAssetReadRef MidiAsset = InputData.GetOrConstructDataReadReference<FMidiAsset>(METASOUND_GET_PARAM_NAME(Inputs::MidiFileAsset));
	FInt32ReadRef PrerollBars = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(Inputs::PrerollBars), InParams.OperatorSettings);

	FTriggerReadRef TriggerOnStreamStarts = InputData.GetOrConstructDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTriggerOnStreamStarts), InParams.OperatorSettings);
	FTriggerReadRef TriggerOnStreamStops = InputData.GetOrConstructDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTriggerOnStreamStops), InParams.OperatorSettings);
	FTimeReadRef PlaybackTime = InputData.GetOrConstructDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(InputPlaybackTime));
	FTimeReadRef PlaybackTimeEndBlock = InputData.GetOrConstructDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(InputPlaybackTimeEndBlock));
	FFloatReadRef PlaybackRate = InputData.GetOrConstructDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputPlaybackRate));

	return MakeUnique<FMediaStreamerTransportControllerOperator>(InParams, MidiAsset, PrerollBars, TriggerOnStreamStarts, TriggerOnStreamStops, PlaybackTime, PlaybackTimeEndBlock, PlaybackRate);
}

HarmonixMetasound::FMediaStreamerTransportControllerOperator::FMediaStreamerTransportControllerOperator(
	const FBuildOperatorParams& InParams,
	const FMidiAssetReadRef& InMidiAsset,
	const FInt32ReadRef& InPrerollBars,
	const FTriggerReadRef& InTriggerOnStreamStarts,
	const FTriggerReadRef& InTriggerOnStreamStops,
	const FTimeReadRef& InPlaybackTime,
	const FTimeReadRef& InPlaybackTimeEndBlock,
	const FFloatReadRef& InPlaybackRate) :
	MidiAssetInPin(InMidiAsset),
	SeekPrerollBarsInPin(InPrerollBars),
	TriggerOnStreamStartsInPin(InTriggerOnStreamStarts),
	TriggerOnStreamStopsInPin(InTriggerOnStreamStops),
	PlaybackTimeInPin(InPlaybackTime),
	PlaybackTimeEndBlockInPin(InPlaybackTimeEndBlock),
	PlaybackRateInPin(InPlaybackRate),
	TransportOutPin(FMusicTransportEventStreamWriteRef::CreateNew(InParams.OperatorSettings)),
	MidiClockOutPin(FMidiClockWriteRef::CreateNew(InParams.OperatorSettings)),
	BlockSize(InParams.OperatorSettings.GetNumFramesPerBlock()),
	SampleRate(InParams.OperatorSettings.GetSampleRate())
{
	Reset(InParams);
}

void HarmonixMetasound::FMediaStreamerTransportControllerOperator::BindInputs(FInputVertexInterfaceData& InVertexData)
{
	using namespace CommonPinNames;
	using namespace MediaStreamerTransportControllerVertexNames;

	InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::MidiFileAsset), MidiAssetInPin);
	InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::PrerollBars), SeekPrerollBarsInPin);
	InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerOnStreamStarts), TriggerOnStreamStartsInPin);
	InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerOnStreamStops), TriggerOnStreamStopsInPin);
	InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputPlaybackTime), PlaybackTimeInPin);
	InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputPlaybackTimeEndBlock), PlaybackTimeEndBlockInPin);
	InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputPlaybackRate), PlaybackRateInPin);
}

void HarmonixMetasound::FMediaStreamerTransportControllerOperator::BindOutputs(FOutputVertexInterfaceData& InVertexData)
{
	using namespace CommonPinNames;
	using namespace MediaStreamerTransportControllerVertexNames;

	InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Outputs::Transport), TransportOutPin);
	InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Outputs::MidiClock), MidiClockOutPin);
}

Metasound::FDataReferenceCollection HarmonixMetasound::FMediaStreamerTransportControllerOperator::GetInputs() const
{
	// This should never be called. Bind(...) is called instead. This method
	// exists as a stop-gap until the API can be deprecated and removed.
	checkNoEntry();
	return {};
}

Metasound::FDataReferenceCollection HarmonixMetasound::FMediaStreamerTransportControllerOperator::GetOutputs() const
{
	// This should never be called. Bind(...) is called instead. This method
	// exists as a stop-gap until the API can be deprecated and removed.
	checkNoEntry();
	return {};
}

void HarmonixMetasound::FMediaStreamerTransportControllerOperator::Reset(const FResetParams& ResetParams)
{
	TransportOutPin->Reset();
	MidiClockOutPin->PrepareBlock();
	BlockSize = ResetParams.OperatorSettings.GetNumFramesPerBlock();
	SampleRate = ResetParams.OperatorSettings.GetSampleRate();
	SampleCount = 0;
	StopTime = FTime{};
	LastTransportRequest = EMusicPlayerTransportRequest::None;
		
	const FMidiFileProxyPtr& MidiProxy = MidiAssetInPin->GetMidiProxy();
	MidiClockOutPin->AttachToMidiResource(MidiProxy ? MidiProxy->GetMidiFile() : nullptr);
}

void HarmonixMetasound::FMediaStreamerTransportControllerOperator::Execute()
{
	TransportOutPin->Reset();
	MidiClockOutPin->PrepareBlock();
	MidiClockOutPin->AddSpeedChangeToBlock({ 0, 0.0f, *PlaybackRateInPin });

	// Parse triggers and render audio
	int32 PlayTrigIndex = 0;
	int32 NextPlayFrame = 0;
	const int32 NumPlayTrigs = TriggerOnStreamStartsInPin->NumTriggeredInBlock();

	int32 StopTrigIndex = 0;
	int32 NextStopFrame = 0;
	const int32 NumStopTrigs = TriggerOnStreamStopsInPin->NumTriggeredInBlock();

	int32 CurrAudioFrame = 0;
	int32 NextAudioFrame = 0;
	const int32 LastAudioFrame = BlockSize - 1;
	const int32 NoTrigger = BlockSize << 1;

	while (NextAudioFrame < LastAudioFrame)
	{
		// get the next Play and Stop indices
		// (play)
		if (PlayTrigIndex < NumPlayTrigs)
		{
			NextPlayFrame = (*TriggerOnStreamStartsInPin)[PlayTrigIndex];
		}
		else
		{
			NextPlayFrame = NoTrigger;
		}

		// (stop)
		if (StopTrigIndex < NumStopTrigs)
		{
			NextStopFrame = (*TriggerOnStreamStopsInPin)[StopTrigIndex];
		}
		else
		{
			NextStopFrame = NoTrigger;
		}

		// determine the next audio frame we are going to render up to
		NextAudioFrame = FMath::Min(NextPlayFrame, NextStopFrame);

		// no more triggers, rendering to the end of the block
		if (NextAudioFrame == NoTrigger)
		{
			NextAudioFrame = BlockSize;
		}

		CurrAudioFrame = NextAudioFrame;

		// execute the next trigger
		if (CurrAudioFrame == NextPlayFrame)
		{
			switch (LastTransportRequest)
			{
			default:
				LastTransportRequest = EMusicPlayerTransportRequest::Play;
				TransportOutPin->AddTransportRequest(EMusicPlayerTransportRequest::Play, CurrAudioFrame);
				MidiClockOutPin->ResetAndStart(CurrAudioFrame);
				break;
			case EMusicPlayerTransportRequest::Continue:
			case EMusicPlayerTransportRequest::Play:
				break;
			case EMusicPlayerTransportRequest::Pause:
				LastTransportRequest = EMusicPlayerTransportRequest::Continue;
				TransportOutPin->AddTransportRequest(EMusicPlayerTransportRequest::Continue, CurrAudioFrame);
				break;
			}

			FTime PlayTime = *PlaybackTimeEndBlockInPin - *PlaybackRateInPin * (BlockSize - CurrAudioFrame) / SampleRate;
			if (!FMath::IsNearlyEqual(PlayTime.GetSeconds(), StopTime.GetSeconds(), 0.5 / SampleRate))
			{
				FMusicSeekTarget Target = { ESeekPointType::Millisecond };
				Target.Ms = float(PlayTime.GetSeconds()) * 1000.f;
				TransportOutPin->AddSeekRequest(CurrAudioFrame, Target);
				MidiClockOutPin->SeekTo(Target, *SeekPrerollBarsInPin);
			}

			++PlayTrigIndex;
		}

		if (CurrAudioFrame == NextStopFrame)
		{
			StopTime = *PlaybackTimeInPin + *PlaybackRateInPin * CurrAudioFrame / SampleRate;
			LastTransportRequest = EMusicPlayerTransportRequest::Pause;
			TransportOutPin->AddTransportRequest(EMusicPlayerTransportRequest::Pause, CurrAudioFrame);

			++StopTrigIndex;
		}
	}

	switch (LastTransportRequest)
	{
	case EMusicPlayerTransportRequest::Continue:
	case EMusicPlayerTransportRequest::Play:
		MidiClockOutPin->AdvanceHiResToMs(BlockSize, float(PlaybackTimeEndBlockInPin->GetSeconds()) * 1000.f, true);
		break;
	}
}

#undef LOCTEXT_NAMESPACE // "HarmonixMetaSound"
