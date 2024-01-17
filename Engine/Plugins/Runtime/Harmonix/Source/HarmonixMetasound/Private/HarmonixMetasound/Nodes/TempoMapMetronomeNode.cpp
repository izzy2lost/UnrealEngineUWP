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

DEFINE_LOG_CATEGORY_STATIC(TempoMapMetronomeOperator, Log, All);

namespace HarmonixMetasound
{
	using namespace Metasound;

	class FTempoMapMetronomeOperator : public TExecutableOperator<FTempoMapMetronomeOperator>, public FMusicTransportControllable
	{
	public:
		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface& GetVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults);

		FTempoMapMetronomeOperator(const FOperatorSettings& InSettings,
                                   const FMidiAssetReadRef& InMidiAsset,
						           const FMusicTransportEventStreamReadRef& InTransport,
		                           const FFloatReadRef& InSpeedMultiplier,
								   const FInt32ReadRef& InSeekPrerollBars);

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override;
		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override;
		virtual FDataReferenceCollection GetInputs() const override;
		virtual FDataReferenceCollection GetOutputs() const override;

		void Reset(const FResetParams& Params);
		void Execute();

	private:
		//** INPUTS
		FMidiAssetReadRef MidiAssetInPin;
		FMusicTransportEventStreamReadRef TransportInPin;
		FFloatReadRef SpeedMultInPin;
		FInt32ReadRef SeekPrerollBarsInPin;

		//** OUTPUTS
		FMidiClockWriteRef MidiClockOutPin;

		//** DATA
		FMidiClockEventCursor MidiClockEventCursor;
		FSampleCount BlockSize;
		float        SampleRate;
	};

	class FTempoMapMetronomeNode : public FNodeFacade
	{
	public:
		FTempoMapMetronomeNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FTempoMapMetronomeOperator>())
		{}
		virtual ~FTempoMapMetronomeNode() = default;
	};

	METASOUND_REGISTER_NODE(FTempoMapMetronomeNode)

	const FNodeClassMetadata& FTempoMapMetronomeOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName        = { HarmonixNodeNamespace, TEXT("TempoMapMetronome"), TEXT("") };
			Info.MajorVersion     = 0;
			Info.MinorVersion     = 1;
			Info.DisplayName      = METASOUND_LOCTEXT("TempoMapMetronomeNode_DisplayName", "Tempo Map Midi Clock Generator");
			Info.Description      = METASOUND_LOCTEXT("TempoMapMetronomeNode_Description", "Provides a midi clock using the tempo map in the specified Midi file.");
			Info.Author           = PluginAuthor;
			Info.PromptIfMissing  = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface();
			Info.CategoryHierarchy.Emplace(NodeCategories::Music);

			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}

	const FVertexInterface& FTempoMapMetronomeOperator::GetVertexInterface()
	{
		using namespace CommonPinNames;

		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<FMidiAsset>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiFileAsset)),
				TInputDataVertex<FMusicTransportEventStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Transport)),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Speed), 1.0f),
				TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::PrerollBars), 8)
			),
			FOutputVertexInterface(
				TOutputDataVertex<FMidiClock>(METASOUND_GET_PARAM_NAME_AND_METADATA(Outputs::MidiClock))
			)
		);

		return Interface;
	}

	TUniquePtr<IOperator> FTempoMapMetronomeOperator::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		using namespace CommonPinNames;

		const FTempoMapMetronomeNode& TempoClockNode = static_cast<const FTempoMapMetronomeNode&>(InParams.Node);

		const FInputVertexInterfaceData& InputData = InParams.InputData;
		FMidiAssetReadRef InMidiAsset = InputData.GetOrConstructDataReadReference<FMidiAsset>(METASOUND_GET_PARAM_NAME(Inputs::MidiFileAsset));
		FMusicTransportEventStreamReadRef InTransport = InputData.GetOrConstructDataReadReference<FMusicTransportEventStream>(METASOUND_GET_PARAM_NAME(Inputs::Transport), InParams.OperatorSettings);
		FFloatReadRef InSpeed = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Inputs::Speed), InParams.OperatorSettings);
		FInt32ReadRef InPrerollBars = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(Inputs::PrerollBars), InParams.OperatorSettings);

		return MakeUnique<FTempoMapMetronomeOperator>(InParams.OperatorSettings, InMidiAsset, InTransport, InSpeed, InPrerollBars);
	}

	FTempoMapMetronomeOperator::FTempoMapMetronomeOperator(const FOperatorSettings& InSettings, 
                                                           const FMidiAssetReadRef& InMidiAsset,
	                                                       const FMusicTransportEventStreamReadRef& InTransport,
	                                                       const FFloatReadRef& InSpeedMultiplier,
														   const FInt32ReadRef& InPrerollBars)
		: FMusicTransportControllable(EMusicPlayerTransportState::Prepared) 
		, MidiAssetInPin(InMidiAsset)
		, TransportInPin(InTransport)
		, SpeedMultInPin(InSpeedMultiplier)
		, SeekPrerollBarsInPin(InPrerollBars)
		, MidiClockOutPin(FMidiClockWriteRef::CreateNew(InSettings))
		, MidiClockEventCursor(MidiClockOutPin)
		, BlockSize(InSettings.GetNumFramesPerBlock())
		, SampleRate(InSettings.GetSampleRate())
	{
		const FMidiFileProxyPtr& MidiProxy = MidiAssetInPin->GetMidiProxy();
		MidiClockOutPin->AttachToMidiResource(MidiProxy ? MidiProxy->GetMidiFile() : nullptr);
	}

	void FTempoMapMetronomeOperator::BindInputs(FInputVertexInterfaceData& InVertexData)
	{
		using namespace CommonPinNames;
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::MidiFileAsset), MidiAssetInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Transport), TransportInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Speed), SpeedMultInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::PrerollBars), SeekPrerollBarsInPin);
	}

	void FTempoMapMetronomeOperator::BindOutputs(FOutputVertexInterfaceData& InVertexData)
	{
		using namespace CommonPinNames;
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Outputs::MidiClock), MidiClockOutPin);
	}

	FDataReferenceCollection FTempoMapMetronomeOperator::GetInputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}

	FDataReferenceCollection FTempoMapMetronomeOperator::GetOutputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}

	void FTempoMapMetronomeOperator::Reset(const FResetParams& Params)
	{
		BlockSize = Params.OperatorSettings.GetNumFramesPerBlock();
		SampleRate = Params.OperatorSettings.GetSampleRate();

		const FMidiFileProxyPtr& MidiProxy = MidiAssetInPin->GetMidiProxy();
		MidiClockOutPin->AttachToMidiResource(MidiProxy ? MidiProxy->GetMidiFile() : nullptr);
		MidiClockOutPin->ResetAndStart(0, true);
	}

	void FTempoMapMetronomeOperator::Execute()
	{
		MidiClockOutPin->PrepareBlock();
		MidiClockOutPin->AddSpeedChangeToBlock({0, 0.0f, *SpeedMultInPin});

		TransportSpanProcessor TransportHandler = [this](int32 StartFrameIndex, int32 EndFrameIndex, EMusicPlayerTransportState CurrentState)
		{
			switch (CurrentState)
			{
			case EMusicPlayerTransportState::Invalid:
			case EMusicPlayerTransportState::Preparing:
				MidiClockOutPin->AddTransportStateChangeToBlock({ StartFrameIndex, 0.0f, EMusicPlayerTransportState::Prepared });
				MidiClockOutPin->WriteNoAdvance(StartFrameIndex, EndFrameIndex);
				return EMusicPlayerTransportState::Prepared;

			case EMusicPlayerTransportState::Prepared:
				MidiClockOutPin->WriteNoAdvance(StartFrameIndex, EndFrameIndex);
				return CurrentState;

			case EMusicPlayerTransportState::Starting:
				// Play from the beginning if we haven't received a seek call while we were stopped...
				MidiClockOutPin->ResetAndStart(StartFrameIndex, !ReceivedSeekWhileStopped());
				MidiClockOutPin->WriteAdvance(StartFrameIndex, EndFrameIndex, *SpeedMultInPin);
				return EMusicPlayerTransportState::Playing;

			case EMusicPlayerTransportState::Playing:
				MidiClockOutPin->WriteAdvance(StartFrameIndex, EndFrameIndex, *SpeedMultInPin);
				return EMusicPlayerTransportState::Playing;

			case EMusicPlayerTransportState::Seeking:
				MidiClockOutPin->SeekTo(StartFrameIndex, TransportInPin->GetNextSeekDestination(), *SeekPrerollBarsInPin);
				// Here we will return that we want to be in the same state we were in before this request to 
				// seek since we can seek "instantaneously"...
				return GetTransportState();

			case EMusicPlayerTransportState::Continuing:
				MidiClockOutPin->AddTransportStateChangeToBlock({ StartFrameIndex, 0.0f, EMusicPlayerTransportState::Playing });
				MidiClockOutPin->WriteAdvance(StartFrameIndex, EndFrameIndex, *SpeedMultInPin);
				return EMusicPlayerTransportState::Playing;

			case EMusicPlayerTransportState::Pausing:
				MidiClockOutPin->AddTransportStateChangeToBlock({ StartFrameIndex, 0.0f, EMusicPlayerTransportState::Paused });
				MidiClockOutPin->WriteNoAdvance(StartFrameIndex, EndFrameIndex);
				return EMusicPlayerTransportState::Paused;

			case EMusicPlayerTransportState::Paused:
				MidiClockOutPin->WriteNoAdvance(StartFrameIndex, EndFrameIndex);
				return EMusicPlayerTransportState::Paused;

			case EMusicPlayerTransportState::Stopping:
				MidiClockOutPin->AddTransportStateChangeToBlock({ StartFrameIndex, 0.0f, EMusicPlayerTransportState::Prepared });
				MidiClockOutPin->WriteNoAdvance(StartFrameIndex, EndFrameIndex);
				return EMusicPlayerTransportState::Prepared;

			case EMusicPlayerTransportState::Killing:
				MidiClockOutPin->AddTransportStateChangeToBlock({ StartFrameIndex, 0.0f, EMusicPlayerTransportState::Prepared });
				MidiClockOutPin->WriteNoAdvance(StartFrameIndex, EndFrameIndex);
				return EMusicPlayerTransportState::Prepared;

			default:
				checkNoEntry();
				return EMusicPlayerTransportState::Invalid;
			}
		};
		ExecuteTransportSpans(TransportInPin, BlockSize, TransportHandler);
	}

}

#undef LOCTEXT_NAMESPACE // "HarmonixMetaSound"
