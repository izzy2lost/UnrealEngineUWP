// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundParamHelper.h"
#include "MetasoundSampleCounter.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundVertex.h"

#include "HarmonixMetasound/DataTypes/MidiStream.h"
#include "HarmonixMetasound/DataTypes/MusicTransport.h"
#include "HarmonixMetasound/Nodes/MidiStreamTrackFilterNode.h"

#include "HarmonixMetasound/MidiOps/MidiTrackFilter.h"

DEFINE_LOG_CATEGORY_STATIC(LogMidiStreamTrackFilter, Log, All);

#define LOCTEXT_NAMESPACE "HarmonixMetaSound"

namespace HarmonixMetasound::Nodes::MidiTrackFilter
{
	using namespace Metasound;
	
	const FNodeClassName& GetClassName()
	{
		static FNodeClassName ClassName
		{
			HarmonixNodeNamespace,
			"MidiStreamTrackFilter",
			""
		};
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
		DEFINE_INPUT_METASOUND_PARAM(MinTrackIndex, "Min. Track Index", "The first track index to include (1-based, inclusive)");
		DEFINE_INPUT_METASOUND_PARAM(MaxTrackIndex, "Max. Track Index", "The last track index to include (1-based, inclusive)");
		DEFINE_INPUT_METASOUND_PARAM(IncludeConductorTrack, "Include Conductor Track", "Enable to include the conductor track (AKA track 0)");
	}

	namespace Outputs
	{
		DEFINE_METASOUND_PARAM_ALIAS(MidiStream, CommonPinNames::Outputs::MidiStream);
	}
	
	class FMidiStreamTrackFilterOperator_V1 final : public TExecutableOperator<FMidiStreamTrackFilterOperator_V1>
	{
	public:
		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto InitNodeInfo = []() -> FNodeClassMetadata
			{
				FNodeClassMetadata Info;
				Info.ClassName        = GetClassName();
				Info.MajorVersion     = 1;
				Info.MinorVersion     = 0;
				Info.DisplayName      = METASOUND_LOCTEXT("MidiStreamTrackFilterNodeV1_DisplayName", "Midi Stream Track Filter");
				Info.Description      = METASOUND_LOCTEXT("MidiStreamTrackFilterNodeV1_Description", "Duplicates the incoming midi stream to its output after filtering tracks from the input Midi Stream");
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
					TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MinTrackIndex), 0),
					TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MaxTrackIndex), 0),
					TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::IncludeConductorTrack), false)
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
			FInt32ReadRef MinTrackIndex;
			FInt32ReadRef MaxTrackIndex;
			FBoolReadRef IncludeConductorTrack;
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
				InputData.GetOrCreateDefaultDataReadReference<bool>(Inputs::EnableName, InParams.OperatorSettings),
				InputData.GetOrConstructDataReadReference<FMidiStream>(Inputs::MidiStreamName, InParams.OperatorSettings),
				InputData.GetOrCreateDefaultDataReadReference<int32>(Inputs::MinTrackIndexName, InParams.OperatorSettings),
				InputData.GetOrCreateDefaultDataReadReference<int32>(Inputs::MaxTrackIndexName, InParams.OperatorSettings),
				InputData.GetOrCreateDefaultDataReadReference<bool>(Inputs::IncludeConductorTrackName, InParams.OperatorSettings)
			};

			FOutputs Outputs
			{
				FMidiStreamWriteRef::CreateNew(InParams.OperatorSettings)
			};
			
			return MakeUnique<FMidiStreamTrackFilterOperator_V1>(InParams, MoveTemp(Inputs), MoveTemp(Outputs));
		}

		FMidiStreamTrackFilterOperator_V1(const FBuildOperatorParams& InParams, FInputs&& InInputs, FOutputs&& InOutputs)
			: Inputs(MoveTemp(InInputs))
			, Outputs(MoveTemp(InOutputs))
		{
			Reset(InParams);
		}

		
		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override
		{
			InVertexData.BindReadVertex(Inputs::EnableName, Inputs.Enabled);
			InVertexData.BindReadVertex(Inputs::MidiStreamName, Inputs.MidiStream);
			InVertexData.BindReadVertex(Inputs::MinTrackIndexName, Inputs.MinTrackIndex);
			InVertexData.BindReadVertex(Inputs::MaxTrackIndexName, Inputs.MaxTrackIndex);
			InVertexData.BindReadVertex(Inputs::IncludeConductorTrackName, Inputs.IncludeConductorTrack);

			ReassignOutputClock = true;
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override
		{
			InVertexData.BindReadVertex(Outputs::MidiStreamName, Outputs.MidiStream);
			
			ReassignOutputClock = true;
		}

		void Reset(const FResetParams& ResetParams)
		{
			ReassignOutputClock = true;
		}

		void Execute()
		{
			if (ReassignOutputClock)
			{
				if (auto* Clock = Inputs.MidiStream->GetMidiClockSource())
				{
					Outputs.MidiStream->SetClockSource(*Clock);
				}
				ReassignOutputClock = false;
			}

			Filter.SetTrackRange(*Inputs.MinTrackIndex, *Inputs.MaxTrackIndex, *Inputs.IncludeConductorTrack);

			Outputs.MidiStream->PrepareBlock();

			if (*Inputs.Enabled)
			{
				Filter.Process(*Inputs.MidiStream, *Outputs.MidiStream);
			}
		}
	private:
		FInputs Inputs;
		FOutputs Outputs;
		Harmonix::Midi::Ops::FMidiTrackFilter Filter;
		bool ReassignOutputClock = true;
	};

	class FMidiStreamTrackFilterNode_V1 final : public FNodeFacade
	{
	public:
		explicit FMidiStreamTrackFilterNode_V1(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FMidiStreamTrackFilterOperator_V1>())
		{}
		virtual ~FMidiStreamTrackFilterNode_V1() override = default;
	};

	METASOUND_REGISTER_NODE(FMidiStreamTrackFilterNode_V1)
	
	class FMidiStreamTrackFilterOperator_V0 : public TExecutableOperator<FMidiStreamTrackFilterOperator_V0>
	{
	public:
		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface& GetVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults);

		FMidiStreamTrackFilterOperator_V0(const FBuildOperatorParams& InParams,
								  const FBoolReadRef&       InEnabled,
								  const FStringReadRef&     InTrackSelect,
								  const FMidiStreamReadRef& InMidiStream);

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override;
		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override;

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

	class FMidiStreamTrackFilterNode_V0 : public FNodeFacade
	{
	public:
		FMidiStreamTrackFilterNode_V0(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FMidiStreamTrackFilterOperator_V0>())
		{}
		virtual ~FMidiStreamTrackFilterNode_V0() = default;
	};

	METASOUND_REGISTER_NODE(FMidiStreamTrackFilterNode_V0)
		
	const FNodeClassMetadata& FMidiStreamTrackFilterOperator_V0::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName        = GetClassName();
			Info.MajorVersion     = 0;
			Info.MinorVersion     = 1;
			Info.DisplayName      = METASOUND_LOCTEXT("MidiStreamTrackFilterNodeV0_DisplayName", "Midi Stream Track Filter");
			Info.Description      = METASOUND_LOCTEXT("MidiStreamTrackFilterNodeV0_Description", "Duplicates the incoming midi stream to its output after filtering tracks from the input Midi Stream");
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

	const FVertexInterface& FMidiStreamTrackFilterOperator_V0::GetVertexInterface()
	{
		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Enable), true),
				TInputDataVertex<FString>(METASOUND_GET_PARAM_NAME_AND_METADATA(CommonPinNames::Inputs::MidiTrackIndexFilterSpecifier), FString(TEXT("*"))),
				TInputDataVertex<FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiStream))
			),
			FOutputVertexInterface(
				TOutputDataVertex<FMidiStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Outputs::MidiStream))
			)
		);

		return Interface;
	}

	TUniquePtr<IOperator> FMidiStreamTrackFilterOperator_V0::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		const FMidiStreamTrackFilterNode_V0& LoggerNode = static_cast<const FMidiStreamTrackFilterNode_V0&>(InParams.Node);

		const FInputVertexInterfaceData& InputData = InParams.InputData;
		FBoolReadRef InEnabled = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(Inputs::Enable), InParams.OperatorSettings);
		FStringReadRef InTrackIndexFilter = InputData.GetOrCreateDefaultDataReadReference<FString>(METASOUND_GET_PARAM_NAME(CommonPinNames::Inputs::MidiTrackIndexFilterSpecifier), InParams.OperatorSettings);
		FMidiStreamReadRef InMidiStream = InputData.GetOrConstructDataReadReference<FMidiStream>(METASOUND_GET_PARAM_NAME(Inputs::MidiStream), InParams.OperatorSettings);

		return MakeUnique<FMidiStreamTrackFilterOperator_V0>(InParams, 
			InEnabled,
			InTrackIndexFilter,
			InMidiStream);
	}

	FMidiStreamTrackFilterOperator_V0::FMidiStreamTrackFilterOperator_V0(const FBuildOperatorParams& InParams,
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

	void FMidiStreamTrackFilterOperator_V0::BindInputs(FInputVertexInterfaceData& InVertexData)
	{
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Enable), EnableInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(CommonPinNames::Inputs::MidiTrackIndexFilterSpecifier), TrackSelectInPin);
		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::MidiStream), MidiStreamInPin);
	}

	void FMidiStreamTrackFilterOperator_V0::BindOutputs(FOutputVertexInterfaceData& InVertexData)
	{
		using namespace CommonPinNames;

		InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Outputs::MidiStream), MidiStreamOutPin);
	}

	void FMidiStreamTrackFilterOperator_V0::Reset(const FResetParams& ResetParams)
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

	void FMidiStreamTrackFilterOperator_V0::Execute()
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

		MidiStreamOutPin->PrepareBlock();
		MidiStreamOutPin->Copy(MidiStreamInPin, [&](const FMidiStreamEvent& InEvent) {return Filter(InEvent); });
	}
}

#undef LOCTEXT_NAMESPACE // "HarmonixMetaSound"
