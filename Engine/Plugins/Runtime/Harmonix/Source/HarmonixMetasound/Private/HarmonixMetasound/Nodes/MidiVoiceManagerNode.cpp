// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundParamHelper.h"
#include "MetasoundSampleCounter.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundVertex.h"
#include "MetasoundTrigger.h"

#include "HarmonixMetasound/Common.h"
#include "HarmonixMetasound/DataTypes/MidiStream.h"
#include "HarmonixMetasound/DataTypes/MusicTransport.h"
#include "HarmonixMidi/MidiMsg.h"

DEFINE_LOG_CATEGORY_STATIC(LogMidiVoiceManager, Log, All);

#define LOCTEXT_NAMESPACE "HarmonixMetaSound"

namespace HarmonixMetasound
{
	using namespace Metasound;

	class FMidiVoiceManagerOperator : public TExecutableOperator<FMidiVoiceManagerOperator>
	{
	public:
		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface& GetVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults);

		FMidiVoiceManagerOperator(
			const FBuildOperatorParams& InParams,
			FMidiStreamReadRef&& InMidiStream,
			int32 InNumVoices);

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override;
		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override;
		virtual FDataReferenceCollection GetInputs() const override;
		virtual FDataReferenceCollection GetOutputs() const override;

		void Reset(const FResetParams& ResetParams);
		
		void Execute();

	private:
		int32 GetNumVoices() const;

		static constexpr int32 MaxVoices = 8;

		//** INPUTS
		FMidiStreamReadRef MidiStreamInPin;
		int32 NumVoices;

		//** OUTPUTS
		TArray<FMidiStreamWriteRef> MidiOutPin;

		//** DATA
		int32 LastVoiceIndex = -1;
		FMidiVoiceId VoiceIds[MaxVoices];
	};

	class FMidiVoiceManagerNode : public FNodeFacade
	{
	public:
		FMidiVoiceManagerNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FMidiVoiceManagerOperator>())
		{}
		virtual ~FMidiVoiceManagerNode() = default;
	};

	METASOUND_REGISTER_NODE(FMidiVoiceManagerNode)

	const FNodeClassMetadata& FMidiVoiceManagerOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName        = { HarmonixNodeNamespace, TEXT("MidiVoiceManager"), TEXT("")};
			Info.MajorVersion     = 0;
			Info.MinorVersion     = 1;
			Info.DisplayName      = METASOUND_LOCTEXT("MidiVoiceManagerNode_DisplayName", "Midi Voice Manager");
			Info.Description      = METASOUND_LOCTEXT("MidiVoiceManagerNode_Description", "Receives a midi stream, and splits it into multiple streams, for voice polyphony.");
			Info.Author           = PluginAuthor;
			Info.PromptIfMissing  = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface();
			Info.CategoryHierarchy.Emplace(NodeCategories::Music);
			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}

	namespace MidiVoiceManagerPinNames
	{
		METASOUND_PARAM(NumVoices, "Num Voices", "Number of voices to cycle through.")
		METASOUND_PARAM(MidiStreamVoice, "Voice {0}", "Midi Stream for voice {0}.");
	}

	const FVertexInterface& FMidiVoiceManagerOperator::GetVertexInterface()
	{
		using namespace CommonPinNames;
		using namespace MidiVoiceManagerPinNames;

		auto CreateVertexInterface = []() -> FVertexInterface
		{
			FInputVertexInterface InputInterface;
			InputInterface.Add(TInputDataVertex<FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiStream)));
			InputInterface.Add(TInputConstructorVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(NumVoices), MaxVoices));

			FOutputVertexInterface OutputInterface;
			for (uint32 i = 0; i < MaxVoices; ++i)
			{
				OutputInterface.Add(TOutputDataVertex<FMidiStream>(METASOUND_GET_PARAM_NAME_WITH_INDEX_AND_METADATA(MidiStreamVoice, i)));
			}
			return FVertexInterface(InputInterface, OutputInterface);
		};

		static const FVertexInterface Interface = CreateVertexInterface();

		return Interface;
	}

	TUniquePtr<IOperator> FMidiVoiceManagerOperator::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		using namespace CommonPinNames;
		using namespace MidiVoiceManagerPinNames;

		const FMidiVoiceManagerNode& LoggerNode = static_cast<const FMidiVoiceManagerNode&>(InParams.Node);

		const FInputVertexInterfaceData& InputData = InParams.InputData;
		FMidiStreamReadRef InMidiStream = InputData.GetOrConstructDataReadReference<FMidiStream>(
			METASOUND_GET_PARAM_NAME(Inputs::MidiStream),
			InParams.OperatorSettings);
		const TDataReadReference<int32> InNumVoices = InputData.GetOrCreateDefaultDataReadReference<int32>(
			METASOUND_GET_PARAM_NAME(NumVoices),
			InParams.OperatorSettings);

		return MakeUnique<FMidiVoiceManagerOperator>(InParams, MoveTemp(InMidiStream), *InNumVoices);
	}

	FMidiVoiceManagerOperator::FMidiVoiceManagerOperator(
		const FBuildOperatorParams& InParams,
		FMidiStreamReadRef&& InMidiStream,
		const int32 InNumVoices)
	: MidiStreamInPin(MoveTemp(InMidiStream))
	, NumVoices(InNumVoices)
	{
		for (uint32 i = 0; i < MaxVoices; ++i)
		{
			MidiOutPin.Add(FMidiStreamWriteRef::CreateNew(InParams.OperatorSettings));
		}

		Reset(InParams);
	}

	void FMidiVoiceManagerOperator::BindInputs(FInputVertexInterfaceData& InVertexData)
	{
		using namespace CommonPinNames;
		using namespace MidiVoiceManagerPinNames;

		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::MidiStream), MidiStreamInPin);
		InVertexData.SetValue(METASOUND_GET_PARAM_NAME(NumVoices), NumVoices);
	}

	void FMidiVoiceManagerOperator::BindOutputs(FOutputVertexInterfaceData& InVertexData)
	{
		using namespace MidiVoiceManagerPinNames;

		for (uint32 i = 0; i < MaxVoices; ++i)
		{
			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME_WITH_INDEX(MidiStreamVoice, i), MidiOutPin[i]);
		}
	}
	
	FDataReferenceCollection FMidiVoiceManagerOperator::GetInputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}

	FDataReferenceCollection FMidiVoiceManagerOperator::GetOutputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}

	void FMidiVoiceManagerOperator::Reset(const FResetParams& ResetParams)
	{
		// Reset the MIDI streams
		for (FMidiStreamWriteRef& MidiOut : MidiOutPin)
		{
			MidiOut->PrepareBlock();
		}

		// Reset the voices
		for (int32 i = 0; i < MaxVoices; ++i)
		{
			VoiceIds[i] = FMidiVoiceId::None();
		}

		LastVoiceIndex = -1;
	}

	int32 FMidiVoiceManagerOperator::GetNumVoices() const
	{
		return FMath::Clamp(NumVoices, 1, MaxVoices);
	}

	void FMidiVoiceManagerOperator::Execute()
	{
		for (int32 i = 0; i < GetNumVoices(); ++i)
		{
			MidiOutPin[i]->PrepareBlock();
		}

		const TArray<FMidiStreamEvent>& MidiEvents = MidiStreamInPin->GetEventsInBlock();
		for (auto& Event : MidiEvents)
		{
			if (Event.MidiMessage.IsNoteOn()) 
			{
				int32 voiceIndex = (LastVoiceIndex + 1) % GetNumVoices();
				MidiOutPin[voiceIndex]->AddMidiEvent(Event);
				VoiceIds[voiceIndex] = Event.GetVoiceId();
				LastVoiceIndex = voiceIndex;
			}

			if (Event.MidiMessage.IsNoteOff())
			{
				for (int32 voiceIndex = 0; voiceIndex < GetNumVoices(); ++voiceIndex)
				{
					if (VoiceIds[voiceIndex] == Event.GetVoiceId())
					{
						MidiOutPin[voiceIndex]->AddMidiEvent(Event);
					}
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE // "HarmonixMetaSound"

