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

DEFINE_LOG_CATEGORY_STATIC(LogMidiStreamLogger, Log, All);

#define LOCTEXT_NAMESPACE "HarmonixMetaSound"

namespace HarmonixMetasound
{
	using namespace Metasound;

	class FMidiStreamLoggerOperator : public TExecutableOperator<FMidiStreamLoggerOperator>
	{
	public:
		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface& GetVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults);

		FMidiStreamLoggerOperator(const FBuildOperatorParams& InParams,
								  const FBoolReadRef& InEnabled,
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
		FMidiStreamReadRef MidiStreamInPin;

		//** DATA
		void DumpTransportEvent(const FMidiTimestampTransportState& TransportEvent);
		void DumpMidiEvent(const FMidiStreamEvent& MidiEvent);
	};

	class FMidiStreamLoggerNode : public FNodeFacade
	{
	public:
		FMidiStreamLoggerNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FMidiStreamLoggerOperator>())
		{}
		virtual ~FMidiStreamLoggerNode() = default;
	};

	METASOUND_REGISTER_NODE(FMidiStreamLoggerNode)
		
	const FNodeClassMetadata& FMidiStreamLoggerOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName        = { HarmonixNodeNamespace, TEXT("MidiStreamLogger"), TEXT("")};
			Info.MajorVersion     = 0;
			Info.MinorVersion     = 1;
			Info.DisplayName      = METASOUND_LOCTEXT("MidiStreamLoggerNode_DisplayName", "Midi Stream Logger");
			Info.Description      = METASOUND_LOCTEXT("MidiStreamLoggerNode_Description", "Receives midi messages and writes them to the log.");
			Info.Author           = PluginAuthor;
			Info.PromptIfMissing  = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface();
			Info.CategoryHierarchy.Emplace(NodeCategories::Music);
			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}

	const FVertexInterface& FMidiStreamLoggerOperator::GetVertexInterface()
	{
		using namespace CommonPinNames;

		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Enable), true),
				TInputDataVertex<FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiStream))
			),
			FOutputVertexInterface()
		);

		return Interface;
	}

	TUniquePtr<IOperator> FMidiStreamLoggerOperator::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		using namespace CommonPinNames;

		const FMidiStreamLoggerNode& LoggerNode = static_cast<const FMidiStreamLoggerNode&>(InParams.Node);

		const FInputVertexInterfaceData& InputData = InParams.InputData;
		FBoolReadRef InEnabled = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(Inputs::Enable), InParams.OperatorSettings);
		FMidiStreamReadRef InMidiStream = InputData.GetOrConstructDataReadReference<FMidiStream>(METASOUND_GET_PARAM_NAME(Inputs::MidiStream), InParams.OperatorSettings);

		return MakeUnique<FMidiStreamLoggerOperator>(InParams, InEnabled, InMidiStream);
	}

	FMidiStreamLoggerOperator::FMidiStreamLoggerOperator(
		const FBuildOperatorParams& InParams,
		const FBoolReadRef& InEnabled,
		const FMidiStreamReadRef& InMidiStream)
	: EnableInPin(InEnabled)
	, MidiStreamInPin(InMidiStream)
	{
		Reset(InParams);
	}

	void FMidiStreamLoggerOperator::BindInputs(FInputVertexInterfaceData& InVertexData)
	{
		using namespace CommonPinNames;

		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Enable),     EnableInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::MidiStream), MidiStreamInPin);
	}

	void FMidiStreamLoggerOperator::BindOutputs(FOutputVertexInterfaceData& InVertexData)
	{
	}

	FDataReferenceCollection FMidiStreamLoggerOperator::GetInputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}

	FDataReferenceCollection FMidiStreamLoggerOperator::GetOutputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}

	void FMidiStreamLoggerOperator::Reset(const FResetParams& ResetParams)
	{
	}

	void FMidiStreamLoggerOperator::Execute()
	{
		if (!*EnableInPin)
		{
			return;
		}

		const TArray<FMidiTimestampTransportState>& TransportEvents = MidiStreamInPin->GetTransportChangesInBlock();
		const TArray<FMidiStreamEvent>& MidiEvents = MidiStreamInPin->GetEventsInBlock();
		auto TransportEventIterator = TransportEvents.begin();
		auto MidiEventIterator = MidiEvents.begin();
		while (TransportEventIterator != TransportEvents.end() || MidiEventIterator != MidiEvents.end())
		{
			if (TransportEventIterator != TransportEvents.end() && MidiEventIterator != MidiEvents.end())
			{
				if ((*TransportEventIterator).BlockSampleFrameIndex <= (*MidiEventIterator).BlockSampleFrameIndex)
				{
					DumpTransportEvent(*TransportEventIterator);
					++TransportEventIterator;
				}
				else
				{
					DumpMidiEvent(*MidiEventIterator);
					++MidiEventIterator;
				}
			}
			else if (TransportEventIterator != TransportEvents.end())
			{
				DumpTransportEvent(*TransportEventIterator);
				++TransportEventIterator;
			}
			else
			{
				DumpMidiEvent(*MidiEventIterator);
				++MidiEventIterator;
			}
		}
	}

	void FMidiStreamLoggerOperator::DumpTransportEvent(const FMidiTimestampTransportState& TransportEvent)
	{
		UE_LOG(LogMidiStreamLogger, Log, TEXT("[%d (%f)] Transport: %s"), TransportEvent.BlockSampleFrameIndex, TransportEvent.BlockSampleFrameOffset, *FMusicTransportControllable::StateToString(TransportEvent.TransportState));
	}

	void FMidiStreamLoggerOperator::DumpMidiEvent(const FMidiStreamEvent& MidiEvent)
	{
		UE_LOG(LogMidiStreamLogger, Log, TEXT("[%d (%f)] Track: %d, Auth Tick %d, Render Tick %d, Ms Offset %f, Message: %s"),
			MidiEvent.BlockSampleFrameIndex,
			MidiEvent.BlockSampleFrameOffset,
			MidiEvent.TrackIndex,
			MidiEvent.AuthoredMidiTick,
			MidiEvent.CurrentMidiTick,
			MidiEvent.MsOffset,
			*FMidiMsg::ToString(MidiEvent.MidiMessage));
	}
}

#undef LOCTEXT_NAMESPACE // "HarmonixMetaSound"
