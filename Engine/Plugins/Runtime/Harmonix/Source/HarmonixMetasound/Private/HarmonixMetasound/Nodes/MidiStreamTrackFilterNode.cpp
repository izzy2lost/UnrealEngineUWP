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
#include "HarmonixMetasound/Nodes/MidiStreamTrackFilterNode.h"

DEFINE_LOG_CATEGORY_STATIC(LogMidiStreamTrackFilter, Log, All);

#define LOCTEXT_NAMESPACE "HarmonixMetaSound"

namespace HarmonixMetasound::Nodes::MidiStreamTrackFilter
{
	const Metasound::FNodeClassName& GetClassName()
	{
		static Metasound::FNodeClassName ClassName
		{
			HarmonixNodeNamespace,
			"MidiStreamTrackFilter",
			""
		};
		return ClassName;
	}

	class FOp : public TExecutableOperator<FOp>
	{
	public:
		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface& GetVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults);

		FOp(const FBuildOperatorParams& InParams,
								  const FBoolReadRef&       InEnabled,
								  const FStringReadRef&     InTrackSelect,
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
		FMidiStreamReadRef MidiStreamInPin;

		//** OUTPUTS
		FMidiStreamWriteRef MidiStreamOutPin;

		//** DATA
		FString CurrentTrackIndexFilter;
		FMidiStreamEventTrackChannelFilter Filter;
	};

	class FMidiStreamTrackFilterNode : public FNodeFacade
	{
	public:
		FMidiStreamTrackFilterNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FOp>())
		{}
		virtual ~FMidiStreamTrackFilterNode() = default;
	};

	METASOUND_REGISTER_NODE(FMidiStreamTrackFilterNode)
		
	const FNodeClassMetadata& FOp::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName        = GetClassName();
			Info.MajorVersion     = 0;
			Info.MinorVersion     = 1;
			Info.DisplayName      = METASOUND_LOCTEXT("MidiStreamTrackFilterNode_DisplayName", "Midi Stream Track Filter");
			Info.Description      = METASOUND_LOCTEXT("MidiStreamTrackFilterNode_Description", "Duplicates the incoming midi stream to its output after filtering tracks from the input Midi Stream");
			Info.Author           = PluginAuthor;
			Info.PromptIfMissing  = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface();
			Info.CategoryHierarchy.Emplace(NodeCategories::Music);
			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}

	const FVertexInterface& FOp::GetVertexInterface()
	{
		using namespace CommonPinNames;

		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Enable), true),
				TInputDataVertex<FString>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiTrackIndexFilterSpecifier), FString(TEXT("*"))),
				TInputDataVertex<FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiStream))
			),
			FOutputVertexInterface(
				TOutputDataVertex<FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Outputs::MidiStream))
			)
		);

		return Interface;
	}

	TUniquePtr<IOperator> FOp::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		using namespace CommonPinNames;

		const FMidiStreamTrackFilterNode& LoggerNode = static_cast<const FMidiStreamTrackFilterNode&>(InParams.Node);

		const FInputVertexInterfaceData& InputData = InParams.InputData;
		FBoolReadRef InEnabled = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(Inputs::Enable), InParams.OperatorSettings);
		FStringReadRef InTrackIndexFilter = InputData.GetOrCreateDefaultDataReadReference<FString>(METASOUND_GET_PARAM_NAME(Inputs::MidiTrackIndexFilterSpecifier), InParams.OperatorSettings);
		FMidiStreamReadRef InMidiStream = InputData.GetOrConstructDataReadReference<FMidiStream>(METASOUND_GET_PARAM_NAME(Inputs::MidiStream), InParams.OperatorSettings);

		return MakeUnique<FOp>(InParams, 
			InEnabled,
			InTrackIndexFilter,
			InMidiStream);
	}

	FOp::FOp(const FBuildOperatorParams& InParams,
											   const FBoolReadRef& InEnabled,
											   const FStringReadRef& InTrackSelect,
											   const FMidiStreamReadRef& InMidiStream)
		: EnableInPin(InEnabled)
		, TrackSelectInPin(InTrackSelect)
		, MidiStreamInPin(InMidiStream)
		, MidiStreamOutPin(FMidiStreamWriteRef::CreateNew(InParams.OperatorSettings))
	{
		Reset(InParams);
	}

	void FOp::BindInputs(FInputVertexInterfaceData& InVertexData)
	{
		using namespace CommonPinNames;

		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Enable), EnableInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::MidiTrackIndexFilterSpecifier), TrackSelectInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::MidiStream), MidiStreamInPin);
	}

	void FOp::BindOutputs(FOutputVertexInterfaceData& InVertexData)
	{
		using namespace CommonPinNames;

		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Outputs::MidiStream), MidiStreamOutPin);
	}

	FDataReferenceCollection FOp::GetInputs() const
	{
		checkNoEntry();
		return {};
	}

	FDataReferenceCollection FOp::GetOutputs() const
	{
		checkNoEntry();
		return {};
	}

	void FOp::Reset(const FResetParams& ResetParams)
	{
		MidiStreamOutPin->PrepareBlock();
		
		if (const FMidiClockReadRef* Clock = MidiStreamInPin->GetMidiClockSource())
		{
			MidiStreamOutPin->SetClockSource(*Clock);
		}

		CurrentTrackIndexFilter = *TrackSelectInPin;
		
		FString ParseTrackIndexFilterErrorMessage;
		if (!Filter.SetTrackFilterFromString(CurrentTrackIndexFilter, ParseTrackIndexFilterErrorMessage))
		{
			UE_LOG(LogMidiStreamTrackFilter, Warning, TEXT("Error parsing Track Filter string: %s"), *ParseTrackIndexFilterErrorMessage);
		}

		FString ParseMidiChannelFilterErrorMessage;
		if (!Filter.SetMidiChannelFilterFromString(TEXT("*"), ParseMidiChannelFilterErrorMessage))
		{
			UE_LOG(LogMidiStreamTrackFilter, Warning, TEXT("Error parsing Midi Channel Filter string: %s"), *ParseMidiChannelFilterErrorMessage);
		}
	}

	void FOp::Execute()
	{
		if (CurrentTrackIndexFilter != *TrackSelectInPin)
		{
			CurrentTrackIndexFilter = *TrackSelectInPin;

			FString ParseTrackIndexFilterErrorMessage;
			if (!Filter.SetTrackFilterFromString(CurrentTrackIndexFilter, ParseTrackIndexFilterErrorMessage))
			{
				UE_LOG(LogMidiStreamTrackFilter, Warning, TEXT("Error parsing Track Filter string: %s"), *ParseTrackIndexFilterErrorMessage);
			}
		}

		MidiStreamOutPin->Copy(MidiStreamInPin, [&](const FMidiStreamEvent& InEvent) {return Filter(InEvent); });
	}
}

#undef LOCTEXT_NAMESPACE // "HarmonixMetaSound"
