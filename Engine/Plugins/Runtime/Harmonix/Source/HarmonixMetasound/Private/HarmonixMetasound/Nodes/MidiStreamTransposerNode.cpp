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
#include "HarmonixMetasound/DataTypes/MusicTransport.h"

DEFINE_LOG_CATEGORY_STATIC(LogMidiStreamTransposer, Log, All);

#define LOCTEXT_NAMESPACE "HarmonixMetaSound"

namespace HarmonixMetasound
{
	using namespace Metasound;

	class FMidiStreamTransposerOperator : public TExecutableOperator<FMidiStreamTransposerOperator>
	{
	public:
		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface& GetVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults);

		FMidiStreamTransposerOperator(const FBuildOperatorParams& InParams,
								  const FBoolReadRef&       InEnabled,
								  const FStringReadRef&     InTrackSelect,
								  const FStringReadRef&     InChannelSelect,
								  const FInt32ReadRef&      InTransposition,
								  const FMidiStreamReadRef& InMidiStream);

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override;
		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override;
		virtual FDataReferenceCollection GetInputs() const override;
		virtual FDataReferenceCollection GetOutputs() const override;

		void Reset(const FResetParams& ResetParams);

		void Execute();

	private:
		//** INPUTS
		FBoolReadRef       EnableInPin;
		FStringReadRef     TrackSelectInPin;
		FStringReadRef     ChannelSelectInPin;
		FInt32ReadRef      TranspositionInPin;
		FMidiStreamReadRef MidiStreamInPin;

		//** OUTPUTS
		FMidiStreamWriteRef MidiStreamOutPin;

		//** DATA
		FString CurrentTrackFilter;
		FString CurrentMidiChannelFilter;
		FMidiStreamEventTrackChannelFilter Filter;
	};

	class FMidiStreamTransposerNode : public FNodeFacade
	{
	public:
		FMidiStreamTransposerNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FMidiStreamTransposerOperator>())
		{}
		virtual ~FMidiStreamTransposerNode() = default;
	};

	METASOUND_REGISTER_NODE(FMidiStreamTransposerNode)
		
	const FNodeClassMetadata& FMidiStreamTransposerOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName        = { HarmonixNodeNamespace, TEXT("MidiStreamTransposer"), TEXT("")};
			Info.MajorVersion     = 0;
			Info.MinorVersion     = 1;
			Info.DisplayName      = METASOUND_LOCTEXT("MidiStreamTransposerNode_DisplayName", "Midi Stream Transposer");
			Info.Description      = METASOUND_LOCTEXT("MidiStreamTransposerNode_Description", "Duplicates the incoming midi stream to its output with the note on/off messages transposed by the specified number of semitones.");
			Info.Author           = PluginAuthor;
			Info.PromptIfMissing  = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface();
			Info.CategoryHierarchy.Emplace(NodeCategories::Music);
			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}

	const FVertexInterface& FMidiStreamTransposerOperator::GetVertexInterface()
	{
		using namespace CommonPinNames;

		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Enable), true),
				TInputDataVertex<FString>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiTrackIndexFilterSpecifier), FString(TEXT("*"))),
				TInputDataVertex<FString>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiChannelFilterSpecifier), FString(TEXT("*"))),
				TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Transposition), 0),
				TInputDataVertex<FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiStream))
			),
			FOutputVertexInterface(
				TOutputDataVertex<FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Outputs::MidiStream))
			)
		);

		return Interface;
	}

	TUniquePtr<IOperator> FMidiStreamTransposerOperator::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		using namespace CommonPinNames;

		const FMidiStreamTransposerNode& LoggerNode = static_cast<const FMidiStreamTransposerNode&>(InParams.Node);

		const FInputVertexInterfaceData& InputData = InParams.InputData;
		FBoolReadRef InEnabled = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(Inputs::Enable), InParams.OperatorSettings);
		FStringReadRef InTrackFilter = InputData.GetOrCreateDefaultDataReadReference<FString>(METASOUND_GET_PARAM_NAME(Inputs::MidiTrackIndexFilterSpecifier), InParams.OperatorSettings);
		FStringReadRef InMidiChannelFilter = InputData.GetOrCreateDefaultDataReadReference<FString>(METASOUND_GET_PARAM_NAME(Inputs::MidiChannelFilterSpecifier), InParams.OperatorSettings);
		FInt32ReadRef InTransposition = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(Inputs::Transposition), InParams.OperatorSettings);
		FMidiStreamReadRef InMidiStream = InputData.GetOrConstructDataReadReference<FMidiStream>(METASOUND_GET_PARAM_NAME(Inputs::MidiStream), InParams.OperatorSettings);

		return MakeUnique<FMidiStreamTransposerOperator>(InParams, 
			InEnabled,
			InTrackFilter,
			InMidiChannelFilter,
			InTransposition,
			InMidiStream);
	}

	FMidiStreamTransposerOperator::FMidiStreamTransposerOperator(const FBuildOperatorParams& InParams,
											   const FBoolReadRef& InEnabled,
											   const FStringReadRef& InTrackSelect,
											   const FStringReadRef& InChannelSelect,
											   const FInt32ReadRef& InTransposition,
											   const FMidiStreamReadRef& InMidiStream)
		: EnableInPin(InEnabled)
		, TrackSelectInPin(InTrackSelect)
		, ChannelSelectInPin(InChannelSelect)
		, TranspositionInPin(InTransposition)
		, MidiStreamInPin(InMidiStream)
		, MidiStreamOutPin(FMidiStreamWriteRef::CreateNew(InParams.OperatorSettings))
	{
		Reset(InParams);
	}

	void FMidiStreamTransposerOperator::BindInputs(FInputVertexInterfaceData& InVertexData)
	{
		using namespace CommonPinNames;

		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Enable), EnableInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::MidiTrackIndexFilterSpecifier), TrackSelectInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::MidiChannelFilterSpecifier), ChannelSelectInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Transposition), TranspositionInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::MidiStream), MidiStreamInPin);
	}

	void FMidiStreamTransposerOperator::BindOutputs(FOutputVertexInterfaceData& InVertexData)
	{
		using namespace CommonPinNames;

		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Outputs::MidiStream), MidiStreamOutPin);
	}

	FDataReferenceCollection FMidiStreamTransposerOperator::GetInputs() const
	{
		checkNoEntry();
		return {};
	}

	FDataReferenceCollection FMidiStreamTransposerOperator::GetOutputs() const
	{
		checkNoEntry();
		return {};
	}

	void FMidiStreamTransposerOperator::Reset(const FResetParams& ResetParams)
	{
		MidiStreamOutPin->PrepareBlock();
		
		if (const FMidiClockReadRef* Clock = MidiStreamInPin->GetMidiClockSource())
		{
			MidiStreamOutPin->SetClockSource(*Clock);
		}
		CurrentTrackFilter = *TrackSelectInPin;
		CurrentMidiChannelFilter = *ChannelSelectInPin;
		
		FString ParseTrackFilterErrorMessage;
		if (!Filter.SetTrackFilterFromString(CurrentTrackFilter, ParseTrackFilterErrorMessage))
		{
			UE_LOG(LogMidiStreamTransposer, Warning, TEXT("Error parsing Track Filter string: %s"), *ParseTrackFilterErrorMessage);
		}

		FString ParseMidiChannelFilterErrorMessage;
		if (!Filter.SetMidiChannelFilterFromString(CurrentMidiChannelFilter, ParseMidiChannelFilterErrorMessage))
		{
			UE_LOG(LogMidiStreamTransposer, Warning, TEXT("Error parsing Channel Filter string: %s"), *ParseMidiChannelFilterErrorMessage);
		}
	}

	void FMidiStreamTransposerOperator::Execute()
	{
		if (CurrentTrackFilter != *TrackSelectInPin)
		{
			CurrentTrackFilter = *TrackSelectInPin;
			FString ParseTrackFilterErrorMessage;
			if (!Filter.SetTrackFilterFromString(CurrentTrackFilter, ParseTrackFilterErrorMessage))
			{
				UE_LOG(LogMidiStreamTransposer, Warning, TEXT("Error parsing Track Filter string: %s"), *ParseTrackFilterErrorMessage);
			}
		}

		if (CurrentMidiChannelFilter != *ChannelSelectInPin)
		{
			CurrentMidiChannelFilter = *ChannelSelectInPin;
			FString ParseMidiChannelFilterErrorMessage;
			if (!Filter.SetMidiChannelFilterFromString(CurrentMidiChannelFilter, ParseMidiChannelFilterErrorMessage))
			{
				UE_LOG(LogMidiStreamTransposer, Warning, TEXT("Error parsing Channel Filter string: %s"), *ParseMidiChannelFilterErrorMessage);
			}
		}

		auto Transformer = [&](const FMidiStreamEvent& InEvent) -> FMidiStreamEvent
			{
				FMidiStreamEvent NewEvent = InEvent;
				if (Filter(NewEvent) && (NewEvent.MidiMessage.IsNoteOn() || NewEvent.MidiMessage.IsNoteOff()))
				{
					const int32 NewNote = FMath::Clamp(*TranspositionInPin + NewEvent.MidiMessage.Data1, 0, 127);
					NewEvent.MidiMessage.Data1 = NewNote;
				}
				return NewEvent;
			};
		MidiStreamOutPin->Copy(MidiStreamInPin, [](const FMidiStreamEvent&){return true;}, Transformer);
	}
}

#undef LOCTEXT_NAMESPACE // "HarmonixMetaSound"
