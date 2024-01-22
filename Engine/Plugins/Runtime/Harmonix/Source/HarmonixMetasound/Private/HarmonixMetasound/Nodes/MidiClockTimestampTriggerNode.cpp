// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tickable.h"
#include "Containers/Queue.h"

#include <atomic>
#include <limits>

#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundParamHelper.h"
#include "MetasoundSampleCounter.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundEnumRegistrationMacro.h"
#include "MetasoundVertex.h"

#include "HarmonixMetasound/Common.h"
#include "HarmonixMetasound/DataTypes/MidiClock.h"

DEFINE_LOG_CATEGORY_STATIC(LogMidiClockTimestampTrigger, Log, All);

#define LOCTEXT_NAMESPACE "HarmonixMetaSound_MidiClockTimestampTriggerNode"

namespace HarmonixMetasound
{
	using namespace Metasound;

	class FMidiClockTimestampTriggerOperator : public TExecutableOperator<FMidiClockTimestampTriggerOperator>, public FMidiPlayCursor
	{
	public:
		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface& GetVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults);

		FMidiClockTimestampTriggerOperator(const FBuildOperatorParams& InParams,
									const FBoolReadRef&      InEnabled,
		                            const FMidiClockReadRef& InMidiClock,
									const FInt32ReadRef&     InBar,
									const FFloatReadRef&     InBeat,
									const FBoolReadRef&      InTriggerDuringSeek);
		virtual ~FMidiClockTimestampTriggerOperator() override;

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override;
		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override;
		virtual FDataReferenceCollection GetInputs() const override;
		virtual FDataReferenceCollection GetOutputs() const override;

		void Reset(const FResetParams& ResetParams);
		
		void Execute();

	private:
		//** INPUTS
		FMidiClockReadRef   MidiClockInPin;
		FBoolReadRef        EnableInPin;
		FInt32ReadRef		BarInPin;
		FFloatReadRef		BeatInPin;
		FBoolReadRef		TriggerDuringSeekInPin;

		//** OUTPUTS
		FTriggerWriteRef   TriggerOutPin;

 		//** DATA (current state)
		int32 Bar         = 1;
		float Beat        = 1.0f;
		int32 TriggerTick = 0;
		bool  AdvanceBlockNeeded = true;

		//** BEGIN FMidiPlayCursor
		virtual void SeekToTick(int32 Tick) override { SeekThruTick(Tick - 1); }
		virtual void SeekThruTick(int32 Tick) override;
		virtual void AdvanceThruTick(int32 Tick, bool IsPreRoll) override;
		// We have to override this to disambiguate the FMidiPlayCursor Reset and the MS operator Reset
		virtual void Reset(bool ForceNoBroadcast) override { FMidiPlayCursor::Reset(ForceNoBroadcast); }
		//** END FMidiPlayCursor

		void CalculateTriggerTick();
	};

	class FMidiClockTimestampTriggerNode : public FNodeFacade
	{
	public:
		FMidiClockTimestampTriggerNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FMidiClockTimestampTriggerOperator>())
		{}
		virtual ~FMidiClockTimestampTriggerNode() = default;
	};

	METASOUND_REGISTER_NODE(FMidiClockTimestampTriggerNode)
		
	const FNodeClassMetadata& FMidiClockTimestampTriggerOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName        = { HarmonixNodeNamespace, TEXT("MidiClockTimestampTrigger"), TEXT("")};
			Info.MajorVersion     = 0;
			Info.MinorVersion     = 1;
			Info.DisplayName      = METASOUND_LOCTEXT("MidiClockTimestampTriggerNode_DisplayName", "Midi Clock Timestamp Trigger");
			Info.Description      = METASOUND_LOCTEXT("MidiClockTimestampTriggerNode_Description", "Watches a midi clock and outputs triggers at the specified musical timestamp.");
			Info.Author           = PluginAuthor;
			Info.PromptIfMissing  = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface();
			Info.CategoryHierarchy.Emplace(NodeCategories::Music);
			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}

	namespace MidiClockTimestampTriggerPinNames
	{
		METASOUND_PARAM(TriggerDuringSeek, "Trigger During Seek", "Whether a trigger should be generated is a seek over the timestamp is detected.")
		METASOUND_PARAM(TriggerOutput, "Trigger Out", "A trigger when the timestamp is detected.")
	}

	const FVertexInterface& FMidiClockTimestampTriggerOperator::GetVertexInterface()
	{
		using namespace MidiClockTimestampTriggerPinNames;
		using namespace CommonPinNames;

		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<FMidiClock>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiClock)),
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Enable), true),
				TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Bar), 0),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::FloatBeat), 0.0f),
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(TriggerDuringSeek), false)
				),
			FOutputVertexInterface(
				TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(TriggerOutput))
			)
		);

		return Interface;
	}

	TUniquePtr<IOperator> FMidiClockTimestampTriggerOperator::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		using namespace MidiClockTimestampTriggerPinNames;
		using namespace CommonPinNames;

		const FMidiClockTimestampTriggerNode& TheNode = static_cast<const FMidiClockTimestampTriggerNode&>(InParams.Node);

		const FInputVertexInterfaceData& InputData = InParams.InputData;
		FMidiClockReadRef InMidiClock     = InputData.GetOrConstructDataReadReference<FMidiClock>(METASOUND_GET_PARAM_NAME(Inputs::MidiClock), InParams.OperatorSettings);
		FBoolReadRef  InEnabled           = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(Inputs::Enable), InParams.OperatorSettings);
		FInt32ReadRef InBar           = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(Inputs::Bar), InParams.OperatorSettings);
		FFloatReadRef InBeat              = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Inputs::FloatBeat), InParams.OperatorSettings);
		FBoolReadRef  InTriggerDuringSeek = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(TriggerDuringSeek), InParams.OperatorSettings);

		return MakeUnique<FMidiClockTimestampTriggerOperator>(InParams,
			InEnabled,
			InMidiClock,
			InBar,
			InBeat,
			InTriggerDuringSeek);
	}

	FMidiClockTimestampTriggerOperator::FMidiClockTimestampTriggerOperator(const FBuildOperatorParams& InParams,
															 const FBoolReadRef&      InEnabled,
															 const FMidiClockReadRef& InMidiClock,
															 const FInt32ReadRef&     InBar,
															 const FFloatReadRef&     InBeat,
															 const FBoolReadRef&      InTriggerDuringSeek)
		: MidiClockInPin(InMidiClock)
		, EnableInPin(InEnabled)
		, BarInPin(InBar)
		, BeatInPin(InBeat)
		, TriggerDuringSeekInPin(InTriggerDuringSeek)
		, TriggerOutPin(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
	{
		Reset(InParams);
	}

	FMidiClockTimestampTriggerOperator::~FMidiClockTimestampTriggerOperator()
	{
	}

	void FMidiClockTimestampTriggerOperator::BindInputs(FInputVertexInterfaceData& InVertexData)
	{
		using namespace CommonPinNames;
		using namespace MidiClockTimestampTriggerPinNames;

		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Enable),    EnableInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::MidiClock), MidiClockInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Bar),  BarInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::FloatBeat), BeatInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(TriggerDuringSeek), TriggerDuringSeekInPin);
	}

	void FMidiClockTimestampTriggerOperator::BindOutputs(FOutputVertexInterfaceData& InVertexData)
	{
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(MidiClockTimestampTriggerPinNames::TriggerOutput), TriggerOutPin);
	}
	
	FDataReferenceCollection FMidiClockTimestampTriggerOperator::GetInputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}

	FDataReferenceCollection FMidiClockTimestampTriggerOperator::GetOutputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}

	void FMidiClockTimestampTriggerOperator::Reset(const FResetParams& ResetParams)
	{
		TriggerOutPin->Reset();
		
		FMidiPlayCursor::Reset(true);
		
		SetMessageFilter(FMidiPlayCursor::EFilterPassFlags::None);
		MidiClockInPin->RegisterHiResPlayCursor(this);

		Bar = *BarInPin;
		Beat = *BeatInPin;
		TriggerTick = 0;
		AdvanceBlockNeeded = true;

		CalculateTriggerTick();
	}

	void FMidiClockTimestampTriggerOperator::CalculateTriggerTick()
	{
		const FSongMaps& SongMaps = MidiClockInPin->GetSongMaps();
		int32 TicksPerBeat;
		int32 BarStartTick = SongMaps.GetBarMap().MusicTimestampBarToTick(Bar, nullptr, &TicksPerBeat);
		int32 TicksPer32nd = MidiConstants::kTicksPerQuarterNoteInt / 8;
		int32 BeatStartTick = (FMath::FloorToInt32((Beat - 1.0f) * TicksPerBeat) + (TicksPer32nd / 2)) / TicksPer32nd;
		BeatStartTick *= TicksPer32nd;
		TriggerTick = BarStartTick + BeatStartTick;
	}

	void FMidiClockTimestampTriggerOperator::Execute()
	{
		// first let's see if our configuration has changed at all...
		if (*BarInPin != Bar || *BeatInPin != Beat)
		{
			Bar = *BarInPin;
			Beat = *BeatInPin;
			CalculateTriggerTick();
		}

		AdvanceBlockNeeded = true;
	}

	void FMidiClockTimestampTriggerOperator::SeekThruTick(int32 Tick)
	{
		if (AdvanceBlockNeeded)
		{
			TriggerOutPin->AdvanceBlock();
			AdvanceBlockNeeded = false;
		}

		int32 TickProceedingThisAdvance = CurrentTick;
		FMidiPlayCursor::SeekThruTick(Tick);

		if (Tick < TickProceedingThisAdvance || !*TriggerDuringSeekInPin || !*EnableInPin)
		{
			return;
		}

		if (TickProceedingThisAdvance < TriggerTick && CurrentTick >= TriggerTick)
		{
			TriggerOutPin->TriggerFrame(MidiClockInPin->GetCurrentBlockFrameIndex());
		}
	}

	void FMidiClockTimestampTriggerOperator::AdvanceThruTick(int32 Tick, bool IsPreRoll)
	{
		if (AdvanceBlockNeeded)
		{
			TriggerOutPin->AdvanceBlock();
			AdvanceBlockNeeded = false;
		}

		int32 TickProceedingThisAdvance = CurrentTick;
		FMidiPlayCursor::AdvanceThruTick(Tick, IsPreRoll);

		// don't trigger during preroll...
		if (IsPreRoll || !*EnableInPin)
		{
			return;
		}

		if (TickProceedingThisAdvance < TriggerTick && CurrentTick >= TriggerTick)
		{
			TriggerOutPin->TriggerFrame(MidiClockInPin->GetCurrentBlockFrameIndex());
		}
	}
}

#undef LOCTEXT_NAMESPACE // "HarmonixMetaSound"
