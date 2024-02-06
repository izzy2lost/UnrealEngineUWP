// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/Nodes/MidiStreamTransposerNode.h"

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

namespace HarmonixMetasound::Nodes::MidiNoteTranspose
{
	using namespace Metasound;

	const FNodeClassName& GetClassName()
	{
		static const FNodeClassName ClassName{ HarmonixNodeNamespace, TEXT("MidiStreamTransposer"), TEXT("") };
		return ClassName;
	}

	int32 GetCurrentMajorVersion()
	{
		return 1;
	}

	namespace Inputs
	{
		DEFINE_METASOUND_PARAM_ALIAS(Enable, CommonPinNames::Inputs::Enable);
		DEFINE_METASOUND_PARAM_ALIAS(MidiStream, CommonPinNames::Inputs::MidiStream);
		DEFINE_METASOUND_PARAM_ALIAS(Transposition, CommonPinNames::Inputs::Transposition);
	}

	namespace Outputs
	{
		DEFINE_METASOUND_PARAM_ALIAS(MidiStream, CommonPinNames::Outputs::MidiStream);
	}

	class FMidiNoteTransposeOperator final : public TExecutableOperator<FMidiNoteTransposeOperator>
	{
	public:
		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto InitNodeInfo = []() -> FNodeClassMetadata
			{
				FNodeClassMetadata Info;
				Info.ClassName        = { HarmonixNodeNamespace, TEXT("MidiStreamTransposer"), TEXT("")};
				Info.MajorVersion     = 1;
				Info.MinorVersion     = 0;
				Info.DisplayName      = METASOUND_LOCTEXT("MidiNoteTransposeNode_DisplayName", "MIDI Note Transpose");
				Info.Description      = METASOUND_LOCTEXT("MidiNoteTransposeNode_Description", "Duplicates the incoming MIDI stream to its output with the note on/off messages transposed by the specified number of semitones.");
				Info.Author           = PluginAuthor;
				Info.PromptIfMissing  = PluginNodeMissingPrompt;
				Info.DefaultInterface = GetVertexInterface();
				Info.CategoryHierarchy.Emplace(NodeCategories::Music);
				return Info;
			};

			static const FNodeClassMetadata Info = InitNodeInfo();

			return Info;
		}
		
		static const FVertexInterface& GetVertexInterface()
		{
			static const FVertexInterface Interface(
				FInputVertexInterface(
					TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Enable), true),
					TInputDataVertex<FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiStream)),
					TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Transposition), 0)
				),
				FOutputVertexInterface(
					TOutputDataVertex<FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Outputs::MidiStream))
				)
			);

			return Interface;
		}

		struct FInputs
		{
			FBoolReadRef Enabled;
			FMidiStreamReadRef MidiStream;
			FInt32ReadRef Transposition;
		};

		struct FOutputs
		{
			FMidiStreamWriteRef MidiStream;
		};

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			const FInputVertexInterfaceData& InputData = InParams.InputData;

			FInputs Inputs
			{
				InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(Inputs::Enable), InParams.OperatorSettings),
				InputData.GetOrConstructDataReadReference<FMidiStream>(METASOUND_GET_PARAM_NAME(Inputs::MidiStream), InParams.OperatorSettings),
				InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(Inputs::Transposition), InParams.OperatorSettings)
			};

			FOutputs Outputs
			{
				FMidiStreamWriteRef::CreateNew(InParams.OperatorSettings)
			};

			return MakeUnique<FMidiNoteTransposeOperator>(InParams, MoveTemp(Inputs), MoveTemp(Outputs));
		}

		FMidiNoteTransposeOperator(const FBuildOperatorParams& InParams, FInputs&& InInputs, FOutputs&& InOutputs)
			: Inputs(MoveTemp(InInputs))
			, Outputs(MoveTemp(InOutputs))
		{
			Reset(InParams);
		}

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override
		{
			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Enable), Inputs.Enabled);
			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::MidiStream), Inputs.MidiStream);
			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Transposition), Inputs.Transposition);

			ReassignOutputClock = true;
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override
		{
			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Outputs::MidiStream), Outputs.MidiStream);

			ReassignOutputClock = true;
		}

		void Reset(const FResetParams&)
		{
			ReassignOutputClock = true;
		}

		void Execute()
		{
			if (!*Inputs.Enabled)
			{
				return;
			}
			
			if (ReassignOutputClock)
			{
				if (const FMidiClockReadRef* Clock = Inputs.MidiStream->GetMidiClockSource())
				{
					Outputs.MidiStream->SetClockSource(*Clock);
				}

				ReassignOutputClock = false;
			}

			const int32 Transposition = *Inputs.Transposition;

			const auto Transformer = [Transposition](const FMidiStreamEvent& InEvent) -> FMidiStreamEvent
			{
				FMidiStreamEvent NewEvent = InEvent;
				
				if (NewEvent.MidiMessage.IsNoteOn() || NewEvent.MidiMessage.IsNoteOff())
				{
					NewEvent.MidiMessage.Data1 = FMath::Clamp(NewEvent.MidiMessage.Data1 + Transposition, 0, 127);
				}
				
				return NewEvent;
			};
			
			Outputs.MidiStream->Copy(Inputs.MidiStream, [](const FMidiStreamEvent&){return true;}, Transformer);
		}
		
	private:
		FInputs Inputs;
		FOutputs Outputs;
		bool ReassignOutputClock = true;
	};

	class FMidiNoteTransposeNode final : public FNodeFacade
	{
	public:
		explicit FMidiNoteTransposeNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FMidiNoteTransposeOperator>())
		{}
		virtual ~FMidiNoteTransposeNode() override = default;
	};

	METASOUND_REGISTER_NODE(FMidiNoteTransposeNode)

	class FMidiStreamTransposerOperator_V0 : public TExecutableOperator<FMidiStreamTransposerOperator_V0>
	{
	public:
		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface& GetVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults);

		FMidiStreamTransposerOperator_V0(const FBuildOperatorParams& InParams,
								  const FBoolReadRef&       InEnabled,
								  const FStringReadRef&     InTrackSelect,
								  const FStringReadRef&     InChannelSelect,
								  const FInt32ReadRef&      InTransposition,
								  const FMidiStreamReadRef& InMidiStream);

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override;
		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override;

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

	class FMidiStreamTransposerNode_v0 : public FNodeFacade
	{
	public:
		FMidiStreamTransposerNode_v0(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FMidiStreamTransposerOperator_V0>())
		{}
		virtual ~FMidiStreamTransposerNode_v0() = default;
	};

	METASOUND_REGISTER_NODE(FMidiStreamTransposerNode_v0)
		
	const FNodeClassMetadata& FMidiStreamTransposerOperator_V0::GetNodeInfo()
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
			Info.bDeprecated = true;
			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}

	const FVertexInterface& FMidiStreamTransposerOperator_V0::GetVertexInterface()
	{
		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Enable), true),
				TInputDataVertex<FString>(METASOUND_GET_PARAM_NAME_AND_METADATA(CommonPinNames::Inputs::MidiTrackIndexFilterSpecifier), FString(TEXT("*"))),
				TInputDataVertex<FString>(METASOUND_GET_PARAM_NAME_AND_METADATA(CommonPinNames::Inputs::MidiChannelFilterSpecifier), FString(TEXT("*"))),
				TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Transposition), 0),
				TInputDataVertex<FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiStream))
			),
			FOutputVertexInterface(
				TOutputDataVertex<FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Outputs::MidiStream))
			)
		);

		return Interface;
	}

	TUniquePtr<IOperator> FMidiStreamTransposerOperator_V0::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		const FMidiStreamTransposerNode_v0& LoggerNode = static_cast<const FMidiStreamTransposerNode_v0&>(InParams.Node);

		const FInputVertexInterfaceData& InputData = InParams.InputData;
		FBoolReadRef InEnabled = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(Inputs::Enable), InParams.OperatorSettings);
		FStringReadRef InTrackFilter = InputData.GetOrCreateDefaultDataReadReference<FString>(CommonPinNames::METASOUND_GET_PARAM_NAME(Inputs::MidiTrackIndexFilterSpecifier), InParams.OperatorSettings);
		FStringReadRef InMidiChannelFilter = InputData.GetOrCreateDefaultDataReadReference<FString>(CommonPinNames::METASOUND_GET_PARAM_NAME(Inputs::MidiChannelFilterSpecifier), InParams.OperatorSettings);
		FInt32ReadRef InTransposition = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(Inputs::Transposition), InParams.OperatorSettings);
		FMidiStreamReadRef InMidiStream = InputData.GetOrConstructDataReadReference<FMidiStream>(METASOUND_GET_PARAM_NAME(Inputs::MidiStream), InParams.OperatorSettings);

		return MakeUnique<FMidiStreamTransposerOperator_V0>(InParams, 
			InEnabled,
			InTrackFilter,
			InMidiChannelFilter,
			InTransposition,
			InMidiStream);
	}

	FMidiStreamTransposerOperator_V0::FMidiStreamTransposerOperator_V0(const FBuildOperatorParams& InParams,
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

	void FMidiStreamTransposerOperator_V0::BindInputs(FInputVertexInterfaceData& InVertexData)
	{
		using namespace CommonPinNames;

		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Enable), EnableInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(CommonPinNames::Inputs::MidiTrackIndexFilterSpecifier), TrackSelectInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(CommonPinNames::Inputs::MidiChannelFilterSpecifier), ChannelSelectInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Transposition), TranspositionInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::MidiStream), MidiStreamInPin);
	}

	void FMidiStreamTransposerOperator_V0::BindOutputs(FOutputVertexInterfaceData& InVertexData)
	{
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Outputs::MidiStream), MidiStreamOutPin);
	}

	void FMidiStreamTransposerOperator_V0::Reset(const FResetParams& ResetParams)
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

	void FMidiStreamTransposerOperator_V0::Execute()
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
