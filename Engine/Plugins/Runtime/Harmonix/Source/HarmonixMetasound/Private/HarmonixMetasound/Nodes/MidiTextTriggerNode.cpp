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

DEFINE_LOG_CATEGORY_STATIC(LogMidiTextTrigger, Log, All);

#define LOCTEXT_NAMESPACE "HarmonixMetaSound_MidiTextTriggerNode"

namespace HarmonixMetasound
{
	using namespace Metasound;

	class FMidiTextTriggerOperator : public TExecutableOperator<FMidiTextTriggerOperator>
	{
	public:
		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface& GetVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults);

		FMidiTextTriggerOperator(const FBuildOperatorParams& InParams,
								 const FBoolReadRef&       InEnabled,
								 const FMidiStreamReadRef& InMidiStream,
								 const FInt32ReadRef&      InTrackNumber,
								 const FStringReadRef&     InText);

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override;
		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override;

		void Reset(const FResetParams& ResetParams);
		
		void Execute();

	private:
		//** INPUTS
		FBoolReadRef       EnableInPin;
		FMidiStreamReadRef MidiStreamInPin;
		FInt32ReadRef      TrackNumberInPin;
		FStringReadRef     TextInPin;

		//** OUTPUTS
		FTriggerWriteRef TriggerOutPin;
	};

	class FMidiTextTriggerNode : public FNodeFacade
	{
	public:
		FMidiTextTriggerNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FMidiTextTriggerOperator>())
		{}
		virtual ~FMidiTextTriggerNode() = default;
	};

	METASOUND_REGISTER_NODE(FMidiTextTriggerNode)

	const FNodeClassMetadata& FMidiTextTriggerOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName        = { HarmonixNodeNamespace, TEXT("MidiTextTrigger"), TEXT("")};
			Info.MajorVersion     = 0;
			Info.MinorVersion     = 1;
			Info.DisplayName      = METASOUND_LOCTEXT("MidiTextTriggerNode_DisplayName", "Midi Text Trigger");
			Info.Description      = METASOUND_LOCTEXT("MidiTextTriggerNode_Description", "Receives a midi stream, filters for the desired messages, and outputs triggers.");
			Info.Author           = PluginAuthor;
			Info.PromptIfMissing  = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface();
			Info.CategoryHierarchy.Emplace(NodeCategories::Music);
			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}

	namespace MidiTextTriggerPinNames
	{
		METASOUND_PARAM(TextInput, "Text", "String of characters to look for.")
		METASOUND_PARAM(TriggerOutput, "Trigger Out", "A trigger when the text is encountered.")
	}

	const FVertexInterface& FMidiTextTriggerOperator::GetVertexInterface()
	{
		using namespace MidiTextTriggerPinNames;
		using namespace CommonPinNames;

		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Enable), true),
				TInputDataVertex<FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiStream)),
				TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiTrackNumber),1),
				TInputDataVertex<FString>(METASOUND_GET_PARAM_NAME_AND_METADATA(TextInput))
			),
			FOutputVertexInterface(
				TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(TriggerOutput))
				)
			);

		return Interface;
	}

	TUniquePtr<IOperator> FMidiTextTriggerOperator::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		using namespace MidiTextTriggerPinNames;
		using namespace CommonPinNames;

		const FMidiTextTriggerNode& LoggerNode = static_cast<const FMidiTextTriggerNode&>(InParams.Node);

		const FInputVertexInterfaceData& InputData = InParams.InputData;
		FBoolReadRef InEnabled          = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(Inputs::Enable), InParams.OperatorSettings);
		FMidiStreamReadRef InMidiStream = InputData.GetOrConstructDataReadReference<FMidiStream>(METASOUND_GET_PARAM_NAME(Inputs::MidiStream), InParams.OperatorSettings);
		FInt32ReadRef InTrackNumber     = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(Inputs::MidiTrackNumber), InParams.OperatorSettings);
		FStringReadRef InText           = InputData.GetOrCreateDefaultDataReadReference<FString>(METASOUND_GET_PARAM_NAME(TextInput), InParams.OperatorSettings);

		return MakeUnique<FMidiTextTriggerOperator>(InParams, InEnabled, InMidiStream, InTrackNumber, InText);
	}

	FMidiTextTriggerOperator::FMidiTextTriggerOperator(const FBuildOperatorParams& InParams,
													   const FBoolReadRef&       InEnabled,
													   const FMidiStreamReadRef& InMidiStream,
													   const FInt32ReadRef&      InTrackNumber,
													   const FStringReadRef&     InText)
		: EnableInPin(InEnabled)
		, MidiStreamInPin(InMidiStream)
		, TrackNumberInPin(InTrackNumber)
		, TextInPin(InText)
		, TriggerOutPin(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
	{
		Reset(InParams);
	}

	void FMidiTextTriggerOperator::BindInputs(FInputVertexInterfaceData& InVertexData)
	{
		using namespace MidiTextTriggerPinNames;
		using namespace CommonPinNames;
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Enable), EnableInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::MidiStream), MidiStreamInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::MidiTrackNumber), TrackNumberInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(TextInput), TextInPin);
	}

	void FMidiTextTriggerOperator::BindOutputs(FOutputVertexInterfaceData& InVertexData)
	{
		using namespace MidiTextTriggerPinNames;
		using namespace CommonPinNames;
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(TriggerOutput), TriggerOutPin);
	}

	void FMidiTextTriggerOperator::Reset(const FResetParams& ResetParams)
	{
		TriggerOutPin->Reset();
	}

	void FMidiTextTriggerOperator::Execute()
	{
		TriggerOutPin->AdvanceBlock();

		if (!*EnableInPin)
		{
			return;
		}

		int32 TrackIndex = *TrackNumberInPin;
		if (TrackIndex >= 0)
		{
			const TArray<FMidiStreamEvent>& MidiEvents = MidiStreamInPin->GetEventsInBlock();
			for (auto& Event : MidiEvents)
			{
				bool IsText = Event.MidiMessage.IsText();

				if (Event.TrackIndex != TrackIndex || !IsText)
				{
					continue;
				}

				const FString* EventString = MidiStreamInPin->GetMidiTrackText(TrackIndex, Event.MidiMessage.GetTextIndex());
				if (EventString && EventString->Compare(*TextInPin) == 0)
				{
					TriggerOutPin->TriggerFrame(Event.BlockSampleFrameIndex);
				}
			}
		}
	
	}
}

#undef LOCTEXT_NAMESPACE // "HarmonixMetaSound"

