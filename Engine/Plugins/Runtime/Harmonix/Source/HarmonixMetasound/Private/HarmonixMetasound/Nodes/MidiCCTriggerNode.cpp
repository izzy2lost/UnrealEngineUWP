// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundParamHelper.h"
#include "MetasoundSampleCounter.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundVertex.h"
#include "MetasoundTrigger.h"
#include "HarmonixMetasound/Nodes/MidiCCTriggerNode.h"
#include "HarmonixMetasound/Common.h"
#include "HarmonixMetasound/DataTypes/MidiStream.h"
#include "HarmonixMetasound/DataTypes/MusicTransport.h"
#include "HarmonixMidi/MidiMsg.h"
#include "MetasoundEnumRegistrationMacro.h"
#include "HarmonixMetasound/DataTypes/MidiControllerID.h"

DEFINE_LOG_CATEGORY_STATIC(LogMidiCCTrigger, Log, All);

#define LOCTEXT_NAMESPACE "HarmonixMetaSound"

namespace HarmonixMetasound::Nodes::MidiCCTriggerNode
{
	const Metasound::FNodeClassName& GetClassName()
	{
		static const Metasound::FNodeClassName ClassName
		{
			HarmonixNodeNamespace,
				TEXT("MidiCCTrigger"),
				""
		};
		return ClassName;
	}

	namespace Inputs
	{
		EXTERN_METASOUND_PARAM_ALIAS(Enable, CommonPinNames::Inputs::Enable);
		EXTERN_METASOUND_PARAM_ALIAS(MidiTrackNumber, CommonPinNames::Inputs::MidiTrackNumber);
		EXTERN_METASOUND_PARAM_ALIAS(MidiChannelNumber, CommonPinNames::Inputs::MidiChannelNumber);
		EXTERN_METASOUND_PARAM_D(InputMidiControllerID, "Standard Midi Controller ID", "Standard Midi Controller ID (0-127)");
		EXTERN_METASOUND_PARAM_ALIAS(MidiStream, CommonPinNames::Inputs::MidiStream);
	}

	namespace Outputs
	{
		EXTERN_METASOUND_PARAM_D(OutputControlChangeValueInt32, "Midi Control Change Value (Int32)", "Midi Control Change value (0-127)");
		EXTERN_METASOUND_PARAM_D(OutputControlChangeValueFloat, "Midi Control Change Value (Float)", "normalized Midi Control Change value (0.0-1.0)");
		EXTERN_METASOUND_PARAM_D(OutputTrigger, "Trigger Out", "A trigger when a Midi Control Change message is found");
	}

	class FOp final : public Metasound::TExecutableOperator<FOp>
	{
	public:
		struct FInputs
		{
			Metasound::FBoolReadRef       Enable;
			Metasound::FInt32ReadRef      TrackNumber;
			Metasound::FInt32ReadRef      ChannelNumber;
			Metasound::FEnumStdMidiControllerIDReadRef  ControllerID;
			HarmonixMetasound::FMidiStreamReadRef MidiStream;
		};

		struct FOutputs
		{
			Metasound::FInt32WriteRef ControlChangeValueInt32;
			Metasound::FFloatWriteRef ControlChangeValueFloat;
			Metasound::FTriggerWriteRef TriggerOut;
		};

		static const Metasound::FVertexInterface& GetVertexInterface()
		{
			const auto MakeInterface = []() -> Metasound::FVertexInterface
			{
				using namespace Metasound;

				return
				{
					FInputVertexInterface
					{
						TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Enable), true),
						TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiTrackNumber), 1),
						TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiChannelNumber), 1),
						TInputDataVertex<FEnumStdMidiControllerID>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::InputMidiControllerID)),
						TInputDataVertex<FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiStream))
					},
					FOutputVertexInterface
					{
						TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(Outputs::OutputControlChangeValueInt32)),
						TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(Outputs::OutputControlChangeValueFloat)),
						TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(Outputs::OutputTrigger))
					}
				};
			};

			static const Metasound::FVertexInterface Interface = MakeInterface();

			return Interface;
		}

		static const Metasound::FNodeClassMetadata& GetNodeInfo()
		{
			auto InitNodeInfo = []() -> FNodeClassMetadata
			{
				FNodeClassMetadata Info;
				Info.ClassName = { HarmonixNodeNamespace, TEXT("MidiCCTrigger"), TEXT("") };
				Info.MajorVersion = 0;
				Info.MinorVersion = 1;
				Info.DisplayName = METASOUND_LOCTEXT("MidiCCTriggerNode_DisplayName", "Midi CC Trigger");
				Info.Description = METASOUND_LOCTEXT("MidiCCTriggerNode_Description", "Find the Midi CC messages in a midi stream and output them as triggers, floats, ints by the specified Midi track, Midi channel, and Midi Controller ID.");
				Info.Author = PluginAuthor;
				Info.PromptIfMissing = PluginNodeMissingPrompt;
				Info.DefaultInterface = GetVertexInterface();
				Info.CategoryHierarchy.Emplace(NodeCategories::Music);
				return Info;
			};

			static const FNodeClassMetadata Info = InitNodeInfo();

			return Info;
		}

		static TUniquePtr<IOperator> CreateOperator(const Metasound::FBuildOperatorParams& InParams, Metasound::FBuildResults& OutResults)
		{
			const Metasound::FOperatorSettings& OperatorSettings = InParams.OperatorSettings;
			const FInputVertexInterfaceData& InputData = InParams.InputData;

			FInputs Inputs
			{
				InputData.GetOrCreateDefaultDataReadReference<bool>(Inputs::EnableName,OperatorSettings),
				InputData.GetOrCreateDefaultDataReadReference<int32>(Inputs::MidiTrackNumberName, OperatorSettings),
				InputData.GetOrCreateDefaultDataReadReference<int32>(Inputs::MidiChannelNumberName, OperatorSettings),
				InputData.GetOrCreateDefaultDataReadReference<FEnumStdMidiControllerID>(Inputs::InputMidiControllerIDName, OperatorSettings),
				InputData.GetOrCreateDefaultDataReadReference<HarmonixMetasound::FMidiStream>(Inputs::MidiStreamName, OperatorSettings)
			};

			FOutputs Outputs
			{
				Metasound::FInt32WriteRef::CreateNew(0),
				Metasound::FFloatWriteRef::CreateNew(0.0f),
				Metasound::FTriggerWriteRef::CreateNew(InParams.OperatorSettings)
			};

			return MakeUnique<FOp>(InParams, MoveTemp(Inputs), MoveTemp(Outputs));
		}

		FOp(const Metasound::FBuildOperatorParams& Params, FInputs&& InInputs, FOutputs&& InOutputs)
			: Inputs(MoveTemp(InInputs))
			, Outputs(MoveTemp(InOutputs))
		{
			Reset(Params);
		}

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override
		{
			InVertexData.BindReadVertex(Inputs::EnableName, Inputs.Enable);
			InVertexData.BindReadVertex(Inputs::MidiTrackNumberName, Inputs.TrackNumber);
			InVertexData.BindReadVertex(Inputs::MidiChannelNumberName, Inputs.ChannelNumber);
			InVertexData.BindReadVertex(Inputs::InputMidiControllerIDName, Inputs.ControllerID);
			InVertexData.BindReadVertex(Inputs::MidiStreamName, Inputs.MidiStream);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override
		{
			InVertexData.BindReadVertex(Outputs::OutputControlChangeValueInt32Name, Outputs.ControlChangeValueInt32);
			InVertexData.BindReadVertex(Outputs::OutputControlChangeValueFloatName, Outputs.ControlChangeValueFloat);
			InVertexData.BindReadVertex(Outputs::OutputTriggerName, Outputs.TriggerOut);
		}

		void Reset(const FResetParams& ResetParams)
		{
			Outputs.TriggerOut->Reset();
			*Outputs.ControlChangeValueInt32 = 0;
			*Outputs.ControlChangeValueFloat = 0.0f;
		}

		void Execute()
		{
			Outputs.TriggerOut->AdvanceBlock();

			//reset output values to 0 upon entering a new block
			*Outputs.ControlChangeValueInt32 = 0;
			*Outputs.ControlChangeValueFloat = 0.0f;
			
			if (!*Inputs.Enable)
			{
				return;
			}

			const TArray<FMidiStreamEvent>& MidiEvents = Inputs.MidiStream->GetEventsInBlock();
			for (const FMidiStreamEvent& Event : MidiEvents)
			{
				if (!Event.MidiMessage.IsStd())
				{
					continue;
				}

				if (Event.TrackIndex != *Inputs.TrackNumber || Event.MidiMessage.GetStdChannel() + 1 != *Inputs.ChannelNumber
					|| Event.MidiMessage.GetStdStatus() != MidiConstants::kControl)
				{
					continue;
				}

				EStdMidiControllerID InCCID = (*Inputs.ControllerID);
				//check if current CC message's controller ID matches with input Controller ID
				if (Event.MidiMessage.GetStdData1() == static_cast<uint8>(InCCID))
				{
					//raw midi control change value (0-127)
					*Outputs.ControlChangeValueInt32 = (int32)Event.MidiMessage.GetStdData2();

					//normalize control change value(0.0-1.0)
					*Outputs.ControlChangeValueFloat = (float)Event.MidiMessage.GetStdData2() / 127.0f;

					Outputs.TriggerOut->TriggerFrame(Event.BlockSampleFrameIndex);
				}
				
			}
		}
		private:
			FInputs Inputs;
			FOutputs Outputs;
	};

	class FMidiCCTriggerNode final : public Metasound::FNodeFacade
	{
	public:
		explicit FMidiCCTriggerNode(const Metasound::FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, Metasound::TFacadeOperatorClass<FOp>())
		{}
	};

	METASOUND_REGISTER_NODE(FMidiCCTriggerNode);
}

#undef LOCTEXT_NAMESPACE