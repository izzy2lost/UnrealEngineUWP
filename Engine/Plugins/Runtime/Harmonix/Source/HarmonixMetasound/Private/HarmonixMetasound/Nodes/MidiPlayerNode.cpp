// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundParamHelper.h"
#include "MetasoundSampleCounter.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundVertex.h"

#include "HarmonixMetasound/Common.h"
#include "HarmonixMetasound/DataTypes/MidiStream.h"
#include "HarmonixMetasound/DataTypes/MidiClock.h"
#include "HarmonixMetasound/DataTypes/MidiClockEvent.h"
#include "HarmonixMetasound/DataTypes/MidiAsset.h"
#include "HarmonixMetasound/DataTypes/MusicTransport.h"

#include "HarmonixMidi/MidiPlayCursorMgr.h"
#include "HarmonixMidi/MidiPlayCursor.h"
#include "HarmonixMidi/MidiVoiceId.h"

#define LOCTEXT_NAMESPACE "HarmonixMetaSound"

namespace HarmonixMetasound
{
	using namespace Metasound;

	namespace MidiPlayerNodePinNames
	{
		METASOUND_PARAM(KillVoicesOnSeek, "Kill Voices On Seek", "If true, a \"Kill All Voices\" midi message will be sent when seeking. Otherwise an \"All Notes Off\" will be sent which allows to ADSR release phases.")
		METASOUND_PARAM(KillVoicesOnMidiChange, "Kill Voices On Midi File Change", "If true, a \"Kill All Voices\" midi message will be sent when the midi file asset is changed. Otherwise an \"All Notes Off\" will be sent which allows to ADSR release phases.")
	}

	class FMidiPlayerOperator : public TExecutableOperator<FMidiPlayerOperator>, public FMidiPlayCursor, public FMidiVoiceGeneratorBase, public FMusicTransportControllable
	{
	public:
		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface&   GetVertexInterface();
		static TUniquePtr<IOperator>     CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults);

		FMidiPlayerOperator(const FOperatorSettings& InSettings, 
							const FMidiAssetReadRef& InMidiAsset,
							const FMusicTransportEventStreamReadRef& InTransport,
							const FBoolReadRef& InLoop,
							const FFloatReadRef& InSpeedMultiplier,
                            const int32 InPrerollBars,
							const bool bInKillVoicesOnSeek,
							const bool bInKillVoicesOnMidiChange);

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override;
		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override;
		
		virtual void Reset(const FResetParams& Params);
		virtual void Execute();
		bool IsPlaying() const;

		//~ BEGIN FMidiPlayCursor Overrides
		virtual void Reset(bool ForceNoBroadcast = false) override;
		virtual void OnLoop(int32 LoopStartTick, int32 LoopEndTick) override;
		virtual void SeekToTick(int32 Tick) override;
		virtual void SeekThruTick(int32 Tick) override;
		virtual void AdvanceThruTick(int32 Tick, bool IsPreRoll) override;

		virtual void OnMidiMessage(int32 TrackIndex, int32 Tick, uint8 Status, uint8 Data1, uint8 Data2, bool IsPreroll) override;
		virtual void OnTempo(int32 TrackIndex, int32 Tick, int32 Tempo, bool IsPreroll) override;
		virtual void OnText(int32 TrackIndex, int32 Tick, int32 TextIndex, const FString& Str, uint8 Type, bool IsPreroll = false) override;
		virtual void OnPreRollNoteOn(int32 TrackIndex, int32 EventTick, int32 CurrentTick, float PreRollMs, uint8 Status, uint8 Data1, uint8 Data2) override;
		//~ END FMidiPlayCursor Overrides

	protected:
		//** INPUTS
		FMidiAssetReadRef MidiAssetInPin;
		FMusicTransportEventStreamReadRef TransportInPin;
		FBoolReadRef  LoopInPin;
		FFloatReadRef SpeedMultInPin;
		int32 PrerollBars;
		bool bKillVoicesOnSeek;
		bool bKillVoicesOnMidiChange;

		//** OUTPUTS
		FMidiStreamWriteRef MidiOutPin;
		FMidiClockWriteRef  MidiClockOut;

		//** DATA
		FMidiClockEventCursor MidiClockEventCursor;
		FMidiFileProxyPtr CurrentMidiFile;
		FSampleCount BlockSize      = 0;
		int32 CurrentBlockSpanStart = 0;
		
		virtual void SetupNewMidiFile(const FMidiFileProxyPtr& NewMidi);
	};

	class FExternallyClockedMidiPlayerOperator : public FMidiPlayerOperator
	{
	public:
		FExternallyClockedMidiPlayerOperator(const FOperatorSettings& InSettings,
		                                     const FMidiAssetReadRef& InMidiAsset,
		                                     const FMusicTransportEventStreamReadRef& InTransport,
		                                     const FMidiClockReadRef& InMidiClock,
											 const FBoolReadRef& InLoop,
		                                     const FFloatReadRef& InSpeedMultiplier,
			                                 const int32 InPrerollBars,
											 const bool bInKillVoicesOnSeek,
											 const bool bInKillVoicesOnMidiChange);

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override;
		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override;
		virtual FDataReferenceCollection GetInputs() const override;
		virtual FDataReferenceCollection GetOutputs() const override;

		virtual void Reset(const FResetParams& Params) override;

		virtual void Execute() override;

	private:
		//** INPUTS **********************************
		FMidiClockReadRef MidiClockIn;

		//** DATA   **********************************
		int32 LoopOffsetTick = 0;

		void UpdateLoopOffsetTickFromTick(int32 Tick);
	};

	class FSelfClockedMidiPlayerOperator : public FMidiPlayerOperator
	{
	public:
		FSelfClockedMidiPlayerOperator(const FOperatorSettings& InSettings,
		                               const FMidiAssetReadRef& InMidiAsset,
		                               const FMusicTransportEventStreamReadRef& InTransport,
									   const FBoolReadRef& InLoop,
		                               const FFloatReadRef& InSpeedMultiplier,
			                           const int32 InPrerollBars,
									   const bool bInKillVoicesOnSeek,
									   const bool bInKillVoicesOnMidiChange );

		virtual void Reset(const FResetParams& Params) override;

		virtual void Execute() override;

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override;
		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override;
		virtual FDataReferenceCollection GetInputs() const override;
		virtual FDataReferenceCollection GetOutputs() const override;
	};

	class FMidiPlayerNode : public FNodeFacade
	{
	public:
		FMidiPlayerNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FMidiPlayerOperator>())
		{}
		virtual ~FMidiPlayerNode() = default;
	};

	METASOUND_REGISTER_NODE(FMidiPlayerNode)

	const FNodeClassMetadata& FMidiPlayerOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName        = { HarmonixNodeNamespace, TEXT("MidiPlayer"), TEXT("") };
			Info.MajorVersion     = 0;
			Info.MinorVersion     = 1;
			Info.DisplayName      = METASOUND_LOCTEXT("MidiPlayerNode_DisplayName", "Midi Player");
			Info.Description      = METASOUND_LOCTEXT("MidiPlayerNode_Description", "Plays a standard midi file.");
			Info.Author           = PluginAuthor;
			Info.PromptIfMissing  = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface();
			Info.CategoryHierarchy.Emplace(NodeCategories::Music);

			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}

	const FVertexInterface& FMidiPlayerOperator::GetVertexInterface()
	{
		using namespace CommonPinNames;

		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<FMidiAsset>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiFileAsset)),
				TInputDataVertex<FMusicTransportEventStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Transport)),
				TInputDataVertex<FMidiClock>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiClock)),
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Loop), false),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Speed), 1.0f),
				TInputConstructorVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::PrerollBars), 8),
				TInputConstructorVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(MidiPlayerNodePinNames::KillVoicesOnSeek), false),
				TInputConstructorVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(MidiPlayerNodePinNames::KillVoicesOnMidiChange), false)
			),
			FOutputVertexInterface(
				TOutputDataVertex<FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Outputs::MidiStream)),
				TOutputDataVertex<FMidiClock>(METASOUND_GET_PARAM_NAME_AND_METADATA(Outputs::MidiClock))
			)
		);

		return Interface;
	}

	TUniquePtr<IOperator> FMidiPlayerOperator::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		using namespace CommonPinNames;

		const FMidiPlayerNode& MidiPlayerNode = static_cast<const FMidiPlayerNode&>(InParams.Node);

		const FInputVertexInterfaceData& InputData = InParams.InputData;

		const FOperatorSettings& Settings = InParams.OperatorSettings;

		FMidiAssetReadRef InMidiAsset = InputData.GetOrConstructDataReadReference<FMidiAsset>(METASOUND_GET_PARAM_NAME(Inputs::MidiFileAsset));
		FMusicTransportEventStreamReadRef InTransport  = InputData.GetOrConstructDataReadReference<FMusicTransportEventStream>(METASOUND_GET_PARAM_NAME(Inputs::Transport), Settings);
		FBoolReadRef InLoop = InputData.GetOrConstructDataReadReference<bool>(METASOUND_GET_PARAM_NAME(Inputs::Loop), false);
		FFloatReadRef InSpeed = InputData.GetOrConstructDataReadReference<float>(METASOUND_GET_PARAM_NAME(Inputs::Speed), 1.0f);
		int32 InPrerollBars = InputData.GetOrCreateDefaultValue<int32>(METASOUND_GET_PARAM_NAME(Inputs::PrerollBars), Settings);
		bool bInKillVoicesOnSeek = InputData.GetOrCreateDefaultValue<bool>(METASOUND_GET_PARAM_NAME(MidiPlayerNodePinNames::KillVoicesOnSeek), Settings);
		bool bInKillVoicesOnMidiChange = InputData.GetOrCreateDefaultValue<bool>(METASOUND_GET_PARAM_NAME(MidiPlayerNodePinNames::KillVoicesOnMidiChange), Settings);
		if (InputData.IsVertexBound(METASOUND_GET_PARAM_NAME(Inputs::MidiClock)))
		{
			FMidiClockReadRef InMidiClock = InputData.GetOrConstructDataReadReference<FMidiClock>(METASOUND_GET_PARAM_NAME(Inputs::MidiClock), Settings);
			return MakeUnique<FExternallyClockedMidiPlayerOperator>(InParams.OperatorSettings, InMidiAsset, InTransport, InMidiClock, InLoop, InSpeed, InPrerollBars, bInKillVoicesOnSeek, bInKillVoicesOnMidiChange);
		}

		return MakeUnique<FSelfClockedMidiPlayerOperator>(InParams.OperatorSettings, InMidiAsset, InTransport, InLoop, InSpeed, InPrerollBars, bInKillVoicesOnSeek, bInKillVoicesOnMidiChange);
	}

	FMidiPlayerOperator::FMidiPlayerOperator(const FOperatorSettings& InSettings, 
											 const FMidiAssetReadRef& InMidiAsset,
											 const FMusicTransportEventStreamReadRef& InTransport,
											 const FBoolReadRef& InLoop,
											 const FFloatReadRef& InSpeed,
		                                     const int32 InPrerollBars,
											 const bool bInKillVoicesOnSeek,
											 const bool bInKillVoicesOnMidiChange)
		: FMusicTransportControllable(EMusicPlayerTransportState::Prepared)
		, MidiAssetInPin(InMidiAsset)
		, TransportInPin(InTransport)
		, LoopInPin(InLoop)
		, SpeedMultInPin(InSpeed)
		, PrerollBars(InPrerollBars)
		, bKillVoicesOnSeek(bInKillVoicesOnSeek)
		, bKillVoicesOnMidiChange(bInKillVoicesOnMidiChange)
		, MidiOutPin(FMidiStreamWriteRef::CreateNew(InSettings))
		, MidiClockOut(FMidiClockWriteRef::CreateNew(InSettings))
		, MidiClockEventCursor(MidiClockOut)
		, BlockSize(InSettings.GetNumFramesPerBlock())
	{
		MidiClockOut->RegisterHiResPlayCursor(this);
		MidiOutPin->SetClockSource(MidiClockOut);
		MidiOutPin->AddTransportStateChangeMessage(0, EMusicPlayerTransportState::Prepared);
	}

	FExternallyClockedMidiPlayerOperator::FExternallyClockedMidiPlayerOperator(const FOperatorSettings& InSettings,
																			   const FMidiAssetReadRef& InMidiAsset, 
																			   const FMusicTransportEventStreamReadRef& InTransport,
																			   const FMidiClockReadRef& InMidiClock, 
																			   const FBoolReadRef& InLoop,
																			   const FFloatReadRef& InSpeedMultiplier,
		                                                                       const int32 InPrerollBars,
																			   const bool bInKillVoicesOnSeek,
																			   const bool bInKillVoicesOnMidiChange)
		: FMidiPlayerOperator(InSettings, InMidiAsset, InTransport, InLoop, InSpeedMultiplier, InPrerollBars, bInKillVoicesOnSeek, bInKillVoicesOnMidiChange)
		, MidiClockIn(InMidiClock)
	{
	}

	FSelfClockedMidiPlayerOperator::FSelfClockedMidiPlayerOperator(const FOperatorSettings& InSettings, 
																   const FMidiAssetReadRef& InMidiAsset, 
																   const FMusicTransportEventStreamReadRef& InTransport,
																   const FBoolReadRef& InLoop,
																   const FFloatReadRef& InSpeedMultiplier,
		                                                           int32 InPrerollBars,
																   const bool bInKillVoicesOnSeek,
																   const bool bInKillVoicesOnMidiChange)
		: FMidiPlayerOperator(InSettings, InMidiAsset, InTransport, InLoop, InSpeedMultiplier, InPrerollBars, bInKillVoicesOnSeek, bInKillVoicesOnMidiChange)
	{}

	void FSelfClockedMidiPlayerOperator::Reset(const FResetParams& Params)
	{
		FMidiPlayerOperator::Reset(Params);
	}

	void FExternallyClockedMidiPlayerOperator::BindInputs(FInputVertexInterfaceData& InVertexData)
	{
		FMidiPlayerOperator::BindInputs(InVertexData);
		using namespace CommonPinNames;
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::MidiClock), MidiClockIn);

		MidiClockOut->AttachToTimeAuthority(*MidiClockIn);
	}

	void FExternallyClockedMidiPlayerOperator::BindOutputs(FOutputVertexInterfaceData& InVertexData)
	{
		FMidiPlayerOperator::BindOutputs(InVertexData);
	}

	FDataReferenceCollection FExternallyClockedMidiPlayerOperator::GetInputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}

	FDataReferenceCollection FExternallyClockedMidiPlayerOperator::GetOutputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}

	void FExternallyClockedMidiPlayerOperator::Reset(const FResetParams& Params)
	{
		FMidiPlayerOperator::Reset(Params);
		LoopOffsetTick = 0;
	}

	void FSelfClockedMidiPlayerOperator::BindInputs(FInputVertexInterfaceData& InVertexData)
	{
		FMidiPlayerOperator::BindInputs(InVertexData);
	}

	void FSelfClockedMidiPlayerOperator::BindOutputs(FOutputVertexInterfaceData& InVertexData)
	{
		FMidiPlayerOperator::BindOutputs(InVertexData);
	}

	FDataReferenceCollection FSelfClockedMidiPlayerOperator::GetInputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}

	FDataReferenceCollection FSelfClockedMidiPlayerOperator::GetOutputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}

	void FMidiPlayerOperator::Execute()
	{
		MidiOutPin->PrepareBlock();
		MidiClockOut->PrepareBlock();

		if (CurrentMidiFile != MidiAssetInPin->GetMidiProxy())
		{
			SetupNewMidiFile(MidiAssetInPin->GetMidiProxy());
		}
	}

	bool FMidiPlayerOperator::IsPlaying() const
	{
		return GetTransportState() == EMusicPlayerTransportState::Playing
			|| GetTransportState() == EMusicPlayerTransportState::Starting
			|| GetTransportState() == EMusicPlayerTransportState::Continuing;
	}

	void FExternallyClockedMidiPlayerOperator::Execute()
	{
		FMidiPlayerOperator::Execute();

		MidiClockOut->CopySpeedAndTempoChanges(MidiClockIn.Get(), *SpeedMultInPin);

		int32 CurrentMidiClockEventIndex = -1;
		TransportSpanPostProcessor HandleMidiClockEventsInBlock = [&](int32 StartFrameIndex, int32 EndFrameIndex, EMusicPlayerTransportState CurrentState)
		{
			const TArray<FMidiClockEvent>& MidiClockEvents = MidiClockIn->GetMidiClockEventsInBlock();
			for (int32 EventIndex = FMath::Max(CurrentMidiClockEventIndex, 0); EventIndex < MidiClockEvents.Num(); ++EventIndex)
			{
				const FMidiClockEvent& Event = MidiClockEvents[EventIndex];

				// we've run past the end of the events. Might as well stop iterating.
				if (Event.BlockFrameIndex >= EndFrameIndex)
				{
					break;
				}

				if (Event.BlockFrameIndex >= StartFrameIndex && Event.BlockFrameIndex < EndFrameIndex)
				{
					// We should be advancing midi clock events in order (never repeating)!
					check(CurrentMidiClockEventIndex < EventIndex);
					CurrentMidiClockEventIndex = EventIndex;
					switch (Event.Type)
					{
					case FMidiClockEvent::EType::Reset:
					{
						UpdateLoopOffsetTickFromTick(Event.StartTick);
						int32 Tick = Event.StartTick - LoopOffsetTick;

						float Ms = MidiClockOut->GetSongMaps().TickToMs(Tick);
						FMusicSeekTarget SeekTarget;
						SeekTarget.Type = ESeekPointType::Millisecond;
						SeekTarget.Ms = Ms;
						MidiClockOut->SeekTo(Event.BlockFrameIndex, SeekTarget, PrerollBars);
						break;
					}
					case FMidiClockEvent::EType::Loop:
					{
						// loops should actually be handled by seeking and advancing...
						break;
					}
					case FMidiClockEvent::EType::SeekTo:
					{
						UpdateLoopOffsetTickFromTick(Event.StartTick);
						int32 Tick = Event.StartTick - LoopOffsetTick;

						float Ms = MidiClockOut->GetSongMaps().TickToMs(Tick);
						FMusicSeekTarget SeekTarget;
						SeekTarget.Type = ESeekPointType::Millisecond;
						SeekTarget.Ms = Ms;
						MidiClockOut->SeekTo(Event.BlockFrameIndex, SeekTarget, PrerollBars);
						break;
					}
					case FMidiClockEvent::EType::SeekThru:
					{
						UpdateLoopOffsetTickFromTick(Event.StartTick);
						int32 Tick = Event.StartTick - LoopOffsetTick;

						float Ms = MidiClockOut->GetSongMaps().TickToMs(Tick + 1);
						FMusicSeekTarget SeekTarget;
						SeekTarget.Type = ESeekPointType::Millisecond;
						SeekTarget.Ms = Ms;
						MidiClockOut->SeekTo(Event.BlockFrameIndex, SeekTarget, PrerollBars);
						break;
					}
					case FMidiClockEvent::EType::AdvanceThru:
					{
						
						// The MidiClock handles looping on its own, so we can conveniently advance it
						int32 Tick = Event.StartTick - LoopOffsetTick;
						float Ms = MidiClockOut->GetSongMaps().TickToMs(Tick);

						float ClockInSpeed = MidiClockIn->GetSpeedAtBlockSampleFrame(StartFrameIndex);
						// midi clock needs to know how fast its advancing based on their authority
						MidiClockOut->InformOfCurrentAdvanceRate(ClockInSpeed * *SpeedMultInPin);
						MidiClockOut->AdvanceHiResToMs(Event.BlockFrameIndex, Ms, true);

						// we have to update our loop offset index _after_ the advance
						// which we can do a little bit differently here
						
						// check if the clock looped back
						if (MidiClockOut->DoesLoop())
						{
							int32 NewTick = MidiClockOut->GetCurrentHiResTick();
							if (NewTick < Tick)
							{
								UpdateLoopOffsetTickFromTick(Event.StartTick);
							}
						}
						break;
					}
					}
				}
			}
		};

		TransportSpanProcessor TransportHandler = [&](int32 StartFrameIndex, int32 EndFrameIndex, EMusicPlayerTransportState CurrentState)
		{
			CurrentBlockSpanStart = StartFrameIndex;
			switch (CurrentState)
			{
			case EMusicPlayerTransportState::Invalid:
			case EMusicPlayerTransportState::Preparing:
				MidiOutPin->AddTransportStateChangeMessage(StartFrameIndex, EMusicPlayerTransportState::Prepared);
				return EMusicPlayerTransportState::Prepared;

			case EMusicPlayerTransportState::Prepared:
				MidiOutPin->AddTransportStateChangeMessage(StartFrameIndex, CurrentState);
				return CurrentState;

			case EMusicPlayerTransportState::Starting:
				//SampleCounter.SetNumSamples(0);
				MidiOutPin->AddTransportStateChangeMessage(StartFrameIndex, EMusicPlayerTransportState::Playing);
				return EMusicPlayerTransportState::Playing;

			case EMusicPlayerTransportState::Playing:
				return EMusicPlayerTransportState::Playing;

			case EMusicPlayerTransportState::Seeking:
				// We have nothing to do because our incoming midi clock would have seeked us. 
				return GetTransportState();

			case EMusicPlayerTransportState::Continuing:
				MidiOutPin->AddTransportStateChangeMessage(StartFrameIndex, EMusicPlayerTransportState::Continuing);
				return EMusicPlayerTransportState::Playing;

			case EMusicPlayerTransportState::Pausing:
				MidiOutPin->AddTransportStateChangeMessage(StartFrameIndex, EMusicPlayerTransportState::Pausing);
				return EMusicPlayerTransportState::Paused;

			case EMusicPlayerTransportState::Paused:
				return EMusicPlayerTransportState::Paused;

			case EMusicPlayerTransportState::Stopping:
				MidiOutPin->AddTransportStateChangeMessage(StartFrameIndex, EMusicPlayerTransportState::Stopping);
				{
					FMidiStreamEvent MidiEvent(this, FMidiMsg::CreateAllNotesOff());
					MidiEvent.BlockSampleFrameIndex = MidiClockOut->GetCurrentBlockFrameIndex();
					MidiEvent.BlockSampleFrameOffset = 0.0f;
					MidiEvent.AuthoredMidiTick = MidiClockOut->GetCurrentHiResTick();
					MidiEvent.CurrentMidiTick = MidiClockOut->GetCurrentHiResTick();
					MidiEvent.MsOffset = 0.0f;
					MidiEvent.TrackIndex = 0;
					MidiOutPin->AddMidiEvent(MidiEvent);
				}
				return EMusicPlayerTransportState::Prepared;

			case EMusicPlayerTransportState::Killing:
				MidiOutPin->AddTransportStateChangeMessage(StartFrameIndex, EMusicPlayerTransportState::Killing);
				return EMusicPlayerTransportState::Prepared;

			default:
				checkNoEntry();
				return EMusicPlayerTransportState::Invalid;
			}
		};
		ExecuteTransportSpans(TransportInPin, BlockSize, TransportHandler, HandleMidiClockEventsInBlock);

		GetTransportState();
	}

	void FExternallyClockedMidiPlayerOperator::UpdateLoopOffsetTickFromTick(int32 Tick)
	{
		// only update our loop offset if we're actually looping
		// if our external clock is seeking us
		// we need to update our loop offset ticks
		if (MidiClockOut->DoesLoop())
		{
			int32 LoopStartTick = MidiClockOut->GetLoopStartTick();
			int32 LoopEndTick = MidiClockOut->GetLoopEndTick();
			int32 LoopLengthTicks = LoopEndTick - LoopStartTick;
			if (LoopLengthTicks > 0)
			{
				// the whole number part of the division tells us 
				// how many times we "should have looped" based on the incoming tick
				int32 LoopNum = (Tick - LoopStartTick) / LoopLengthTicks;

				// So we can multiply it back to get the LoopOffsetTick
				LoopOffsetTick = LoopLengthTicks * LoopNum + LoopStartTick;
			}
		}
	}

	void FSelfClockedMidiPlayerOperator::Execute()
	{
		FMidiPlayerOperator::Execute();

		MidiClockOut->AddSpeedChangeToBlock({ 0, 0.0f, *SpeedMultInPin });

		TransportSpanPostProcessor MidiClockTransportHandler = [&](int32 StartFrameIndex, int32 EndFrameIndex, EMusicPlayerTransportState CurrentState)
		{
			switch (CurrentState)
			{
			case EMusicPlayerTransportState::Playing:
				MidiClockOut->WriteAdvance(StartFrameIndex, EndFrameIndex, *SpeedMultInPin);
				return;

			case EMusicPlayerTransportState::Continuing:
				MidiClockOut->WriteAdvance(StartFrameIndex, EndFrameIndex, *SpeedMultInPin);
				return;
			}
		};

		TransportSpanProcessor TransportHandler = [this](int32 StartFrameIndex, int32 EndFrameIndex, EMusicPlayerTransportState CurrentState)
		{
			switch (CurrentState)
			{
			case EMusicPlayerTransportState::Invalid:
			case EMusicPlayerTransportState::Preparing:
				// midi clock out
				MidiClockOut->AddTransportStateChangeToBlock({ StartFrameIndex, 0.0f, EMusicPlayerTransportState::Prepared });
				MidiClockOut->WriteNoAdvance(StartFrameIndex, EndFrameIndex);

				// midi out
				MidiOutPin->AddTransportStateChangeMessage(StartFrameIndex, EMusicPlayerTransportState::Prepared);
				return EMusicPlayerTransportState::Prepared;

			case EMusicPlayerTransportState::Prepared:
				// midi clock out
				MidiClockOut->WriteNoAdvance(StartFrameIndex, EndFrameIndex);

				// midi out
				MidiOutPin->AddTransportStateChangeMessage(StartFrameIndex, CurrentState);
				return CurrentState;

			case EMusicPlayerTransportState::Starting:
				// midi clock out 
				MidiClockOut->ResetAndStart(StartFrameIndex, !ReceivedSeekWhileStopped());

				// midi out
				MidiOutPin->AddTransportStateChangeMessage(StartFrameIndex, EMusicPlayerTransportState::Playing);
				return EMusicPlayerTransportState::Playing;

			case EMusicPlayerTransportState::Playing:
				return EMusicPlayerTransportState::Playing;

			case EMusicPlayerTransportState::Seeking:
				// midi clock out
				MidiClockOut->SeekTo(StartFrameIndex, TransportInPin->GetNextSeekDestination(), PrerollBars);

				// We have nothing to do because our incoming midi clock would have seeked us. 
				return GetTransportState();

			case EMusicPlayerTransportState::Continuing:
				// midi clock
				MidiClockOut->AddTransportStateChangeToBlock({ StartFrameIndex, 0.0f, EMusicPlayerTransportState::Playing });

				// midi out
				MidiOutPin->AddTransportStateChangeMessage(StartFrameIndex, EMusicPlayerTransportState::Continuing);
				return EMusicPlayerTransportState::Playing;

			case EMusicPlayerTransportState::Pausing:
				// midi clock out
				MidiClockOut->AddTransportStateChangeToBlock({ StartFrameIndex, 0.0f, EMusicPlayerTransportState::Paused });
				MidiClockOut->WriteNoAdvance(StartFrameIndex, EndFrameIndex);

				// midi out
				return EMusicPlayerTransportState::Paused;

			case EMusicPlayerTransportState::Paused:
				// midi clock out
				MidiClockOut->WriteNoAdvance(StartFrameIndex, EndFrameIndex);

				// midi out
				return EMusicPlayerTransportState::Paused;

			case EMusicPlayerTransportState::Stopping:
				// midi clock out
				MidiClockOut->AddTransportStateChangeToBlock({ StartFrameIndex, 0.0f, EMusicPlayerTransportState::Prepared });
				MidiClockOut->WriteNoAdvance(StartFrameIndex, EndFrameIndex);

				// midi out
				MidiOutPin->AddTransportStateChangeMessage(StartFrameIndex, EMusicPlayerTransportState::Stopping);
				return EMusicPlayerTransportState::Prepared;

			case EMusicPlayerTransportState::Killing:
				// midi clock out
				MidiClockOut->AddTransportStateChangeToBlock({ StartFrameIndex, 0.0f, EMusicPlayerTransportState::Prepared });
				MidiClockOut->WriteNoAdvance(StartFrameIndex, EndFrameIndex);

				// midi out
				MidiOutPin->AddTransportStateChangeMessage(StartFrameIndex, EMusicPlayerTransportState::Killing);
				return EMusicPlayerTransportState::Prepared;

			default:
				checkNoEntry();
				return EMusicPlayerTransportState::Invalid;
			}
		};
		ExecuteTransportSpans(TransportInPin, BlockSize, TransportHandler, MidiClockTransportHandler);
	}

	void FMidiPlayerOperator::BindInputs(FInputVertexInterfaceData& InVertexData)
	{
		using namespace CommonPinNames;
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::MidiFileAsset), MidiAssetInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Transport), TransportInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Loop), LoopInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Speed), SpeedMultInPin);
		InVertexData.SetValue(METASOUND_GET_PARAM_NAME(Inputs::PrerollBars), PrerollBars);
		InVertexData.SetValue(METASOUND_GET_PARAM_NAME(MidiPlayerNodePinNames::KillVoicesOnSeek), bKillVoicesOnSeek);
		InVertexData.SetValue(METASOUND_GET_PARAM_NAME(MidiPlayerNodePinNames::KillVoicesOnMidiChange), bKillVoicesOnMidiChange);

		SetupNewMidiFile(MidiAssetInPin->GetMidiProxy());
	}

	void FMidiPlayerOperator::BindOutputs(FOutputVertexInterfaceData& InVertexData)
	{
		using namespace CommonPinNames;
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Outputs::MidiStream), MidiOutPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Outputs::MidiClock), MidiClockOut);
	}
	
	void FMidiPlayerOperator::Reset(const FResetParams& Params)
	{
		BlockSize = Params.OperatorSettings.GetNumFramesPerBlock();
		CurrentBlockSpanStart = 0;

		MidiOutPin->SetClockSource(MidiClockOut);
		MidiClockOut->ResetAndStart(0, true);
	}

	void FMidiPlayerOperator::Reset(bool ForceNoBroadcast /*= false*/)
	{
		FMidiPlayCursor::Reset(ForceNoBroadcast);
	}

	void FMidiPlayerOperator::OnLoop(int32 LoopStartTick, int32 LoopEndTick)
	{
		//TRACE_BOOKMARK(TEXT("Midi Looping"));
		FMidiPlayCursor::OnLoop(LoopStartTick, LoopEndTick);
	}

	void FMidiPlayerOperator::SeekToTick(int32 Tick)
	{
		//TRACE_BOOKMARK(TEXT("Midi Seek To Tick"));
		FMidiPlayCursor::SeekToTick(Tick);

		if (IsPlaying())
		{
			FMidiStreamEvent MidiEvent(this, bKillVoicesOnSeek ? FMidiMsg::CreateAllNotesKill() : FMidiMsg::CreateAllNotesOff());
			MidiEvent.BlockSampleFrameIndex = MidiClockOut->GetCurrentBlockFrameIndex();
			MidiEvent.BlockSampleFrameOffset = 0.0f;
			MidiEvent.AuthoredMidiTick = MidiClockOut->GetCurrentHiResTick();
			MidiEvent.CurrentMidiTick = MidiClockOut->GetCurrentHiResTick();
			MidiEvent.MsOffset = 0.0f;
			MidiEvent.TrackIndex = 0;
			MidiOutPin->AddMidiEvent(MidiEvent);
		}
	}

	void FMidiPlayerOperator::SeekThruTick(int32 Tick)
	{
		//TRACE_BOOKMARK(TEXT("Midi Seek Thru Tick"));
		FMidiPlayCursor::SeekThruTick(Tick);

		if (IsPlaying())
		{
			FMidiStreamEvent MidiEvent(this, bKillVoicesOnSeek ? FMidiMsg::CreateAllNotesKill() : FMidiMsg::CreateAllNotesOff());
			MidiEvent.BlockSampleFrameIndex = MidiClockOut->GetCurrentBlockFrameIndex();
			MidiEvent.BlockSampleFrameOffset = 0.0f;
			MidiEvent.AuthoredMidiTick = MidiClockOut->GetCurrentHiResTick();
			MidiEvent.CurrentMidiTick = MidiClockOut->GetCurrentHiResTick();
			MidiEvent.MsOffset = 0.0f;
			MidiEvent.TrackIndex = 0;
			MidiOutPin->AddMidiEvent(MidiEvent);
		}
	}

	void FMidiPlayerOperator::AdvanceThruTick(int32 Tick, bool IsPreRoll)
	{
		FMidiPlayCursor::AdvanceThruTick(Tick, IsPreRoll);
	}

	void FMidiPlayerOperator::OnMidiMessage(int32 TrackIndex, int32 Tick, uint8 Status, uint8 Data1, uint8 Data2, bool IsPreroll)
	{
		if (IsPlaying())
		{
			FMidiStreamEvent MidiEvent(this, FMidiMsg(Status, Data1, Data2));
			MidiEvent.BlockSampleFrameIndex = MidiClockOut->GetCurrentBlockFrameIndex();
			MidiEvent.BlockSampleFrameOffset = 0.0f;
			MidiEvent.AuthoredMidiTick = Tick;
			MidiEvent.CurrentMidiTick = Tick;
			MidiEvent.MsOffset = 0.0f;
			MidiEvent.TrackIndex = TrackIndex;
			MidiOutPin->AddMidiEvent(MidiEvent);
		}
	}

	void FMidiPlayerOperator::OnTempo(int32 TrackIndex, int32 Tick, int32 tempo, bool isPreroll)
	{
		if (IsPlaying())
		{
			FMidiStreamEvent MidiEvent(this, FMidiMsg((int32)tempo));
			MidiEvent.BlockSampleFrameIndex  = MidiClockOut->GetCurrentBlockFrameIndex();
			MidiEvent.BlockSampleFrameOffset = 0.0f;
			MidiEvent.AuthoredMidiTick = Tick;
			MidiEvent.CurrentMidiTick  = Tick;
			MidiEvent.MsOffset         = 0.0f;
			MidiEvent.TrackIndex       = TrackIndex;
			MidiOutPin->AddMidiEvent(MidiEvent);
		}
	}

	void FMidiPlayerOperator::OnText(int32 TrackIndex, int32 Tick, int32 TextIndex, const FString& Str, uint8 Type, bool IsPreroll)
	{
		if (!IsPreroll && IsPlaying())
		{
			FMidiStreamEvent MidiEvent(this, FMidiMsg::CreateText(TextIndex, Type));
			MidiEvent.BlockSampleFrameIndex = MidiClockOut->GetCurrentBlockFrameIndex();
			MidiEvent.BlockSampleFrameOffset = 0.0f;
			MidiEvent.AuthoredMidiTick = Tick;
			MidiEvent.CurrentMidiTick = Tick;
			MidiEvent.MsOffset = 0.0f;
			MidiEvent.TrackIndex = TrackIndex;
			MidiOutPin->AddMidiEvent(MidiEvent);
		}
	}


	void FMidiPlayerOperator::OnPreRollNoteOn(int32 TrackIndex, int32 EventTick, int32 InCurrentTick, float InPrerollMs, uint8 InStatus, uint8 Data1, uint8 Data2)
	{
		if (IsPlaying())
		{
			FMidiStreamEvent MidiEvent(this, FMidiMsg(InStatus, Data1, Data2));
			MidiEvent.BlockSampleFrameIndex = MidiClockOut->GetCurrentBlockFrameIndex();
			MidiEvent.BlockSampleFrameOffset = 0.0f;
			MidiEvent.AuthoredMidiTick = EventTick;
			MidiEvent.CurrentMidiTick = InCurrentTick;
			MidiEvent.MsOffset = InPrerollMs;
			MidiEvent.TrackIndex = TrackIndex;
			MidiOutPin->AddMidiEvent(MidiEvent);
		}
	}

	void FMidiPlayerOperator::SetupNewMidiFile(const FMidiFileProxyPtr& NewMidi)
	{
		CurrentMidiFile = NewMidi;
		MidiOutPin->SetMidiFile(CurrentMidiFile);

		FMidiStreamEvent MidiEvent(this, bKillVoicesOnMidiChange ? FMidiMsg::CreateAllNotesKill() : FMidiMsg::CreateAllNotesOff());
		MidiEvent.BlockSampleFrameIndex = 0;
		MidiEvent.BlockSampleFrameOffset = 0.0f;
		MidiEvent.AuthoredMidiTick = 0;
		MidiEvent.CurrentMidiTick = 0;
		MidiEvent.MsOffset = 0.0f;
		MidiEvent.TrackIndex = 0;
		MidiOutPin->InsertMidiEvent(MidiEvent);

		if (CurrentMidiFile.IsValid())
		{			
			MidiClockOut->AttachToMidiResource(CurrentMidiFile->GetMidiFile(), !IsPlaying(), PrerollBars);
			MidiOutPin->SetTicksPerQuarterNote(CurrentMidiFile->GetMidiFile()->TicksPerQuarterNote);
			if (*LoopInPin)
			{
				const FSongLengthData& SongLengthData = CurrentMidiFile->GetMidiFile()->SongMaps.GetSongLengthData();
				int32 LoopStartTick = 0;
				int32 LoopEndTick = SongLengthData.LengthTicks;

				// Round the content authored ticks to bar boundaries based on the bar map. 
				const FBarMap& BarMap = CurrentMidiFile->GetMidiFile()->SongMaps.GetBarMap();
				int32 LoopStartBarIndex = FMath::RoundToInt32(BarMap.TickToFractionalBarIncludingCountIn(LoopStartTick));
				int32 LoopEndBarIndex = FMath::RoundToInt32(BarMap.TickToFractionalBarIncludingCountIn(LoopEndTick));

				// Make sure there's at least 1 bar of looping
				if (LoopEndBarIndex <= LoopStartBarIndex)
				{
					LoopEndBarIndex = LoopStartBarIndex + 1;
				}
				LoopStartTick = BarMap.BarIncludingCountInToTick(LoopStartBarIndex);
				LoopEndTick = BarMap.BarIncludingCountInToTick(LoopEndBarIndex);
				
				MidiClockOut->SetLoop(LoopStartTick, LoopEndTick);
			}
			else
			{
				MidiClockOut->ClearLoop();
			}
		}
		else
		{
			MidiClockOut->AttachToMidiResource(nullptr, !IsPlaying(), 0);
		}
	}
}

#undef LOCTEXT_NAMESPACE // "HarmonixMetaSound"
