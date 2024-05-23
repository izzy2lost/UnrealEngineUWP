// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundStandardNodesNames.h"
#include "DSP/FloatArrayMath.h"
#include "DSP/ReverbFast.h"
#include "Internationalization/Text.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_PlateReverb"

namespace Metasound
{
	namespace PlateReverb
	{
		namespace Inputs
		{
			METASOUND_PARAM(Bypass, "Bypass", "Toggle to bypass the effect and send audio through unaltered.")
			METASOUND_PARAM(AudioLeft, "In Left", "Left channel audio input.")
			METASOUND_PARAM(AudioRight, "In Right", "Right channel audio input.")
			METASOUND_PARAM(DryLevel, "Dry Level", "The level of the dry signal (linear).")
			METASOUND_PARAM(WetLevel, "Wet Level", "The level of the wet signal (linear).")
		}

		namespace Outputs
		{
			METASOUND_PARAM(AudioLeft, "Out Left", "Left channel audio output.")
			METASOUND_PARAM(AudioRight, "Out Right", "Right channel audio output.")
		}
	}

	class FPlateReverbOperator final : public TExecutableOperator<FPlateReverbOperator>
	{
	public:
		struct FInputs
		{
			FBoolReadRef Bypass;
			FAudioBufferReadRef AudioLeft;
			FAudioBufferReadRef AudioRight;
			FFloatReadRef DryLevel;
			FFloatReadRef WetLevel;
		};
		
		struct FOutputs
		{
			FAudioBufferWriteRef AudioLeft;
			FAudioBufferWriteRef AudioRight;
		};
		
		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				FNodeClassMetadata Metadata
				{
					FNodeClassName { StandardNodes::Namespace, "Plate Reverb", "Stereo" },
					1, // Major Version
					0, // Minor Version
					METASOUND_LOCTEXT("PlateReverbStereoDisplayName", "Plate Reverb (Stereo)"),
					METASOUND_LOCTEXT("PlateReverbDesc", "Plate reverb with configurable early and late reflections."),
					PluginAuthor,
					PluginNodeMissingPrompt,
					GetVertexInterface(),
					{ NodeCategories::Reverbs },
					{},
					FNodeDisplayStyle()
				};

				return Metadata;
			};

			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
			return Metadata;
		}
		
		static const FVertexInterface& GetVertexInterface()
		{
			static const FVertexInterface Interface
			{
				FInputVertexInterface
				{
					TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(PlateReverb::Inputs::Bypass), false),
					TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(PlateReverb::Inputs::AudioLeft)),
					TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(PlateReverb::Inputs::AudioRight)),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(PlateReverb::Inputs::DryLevel), 1.0f),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(PlateReverb::Inputs::WetLevel), 1.0f)
				},
				FOutputVertexInterface
				{
					TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(PlateReverb::Outputs::AudioLeft)),
					TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(PlateReverb::Outputs::AudioRight))
				}
			};

			return Interface;
		}
		
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			FInputs Inputs
			{
				InParams.InputData.GetOrCreateDefaultDataReadReference<bool>(PlateReverb::Inputs::BypassName, InParams.OperatorSettings),
				InParams.InputData.GetOrConstructDataReadReference<FAudioBuffer>(PlateReverb::Inputs::AudioLeftName, InParams.OperatorSettings),
				InParams.InputData.GetOrConstructDataReadReference<FAudioBuffer>(PlateReverb::Inputs::AudioRightName, InParams.OperatorSettings),
				InParams.InputData.GetOrCreateDefaultDataReadReference<float>(PlateReverb::Inputs::DryLevelName, InParams.OperatorSettings),
				InParams.InputData.GetOrCreateDefaultDataReadReference<float>(PlateReverb::Inputs::WetLevelName, InParams.OperatorSettings)
			};

			return MakeUnique<FPlateReverbOperator>(InParams, MoveTemp(Inputs));
		}

		FPlateReverbOperator(const FBuildOperatorParams& BuildParams, FInputs&& Inputs)
			: Inputs(MoveTemp(Inputs))
			, Outputs({ FAudioBufferWriteRef::CreateNew(BuildParams.OperatorSettings), FAudioBufferWriteRef::CreateNew(BuildParams.OperatorSettings) })
			, Reverb(BuildParams.OperatorSettings.GetSampleRate())
		{
			Reset(BuildParams);
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			InOutVertexData.BindReadVertex(PlateReverb::Inputs::BypassName, Inputs.Bypass);
			InOutVertexData.BindReadVertex(PlateReverb::Inputs::AudioLeftName, Inputs.AudioLeft);
			InOutVertexData.BindReadVertex(PlateReverb::Inputs::AudioRightName, Inputs.AudioRight);
			InOutVertexData.BindReadVertex(PlateReverb::Inputs::DryLevelName, Inputs.DryLevel);
			InOutVertexData.BindReadVertex(PlateReverb::Inputs::WetLevelName, Inputs.WetLevel);
		}
		
		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			InOutVertexData.BindReadVertex(PlateReverb::Outputs::AudioLeftName, Outputs.AudioLeft);
			InOutVertexData.BindReadVertex(PlateReverb::Outputs::AudioRightName, Outputs.AudioRight);
		}

		void Reset(const FResetParams&)
		{
		}
		
		void Execute()
		{
			const int32 NumFrames = Inputs.AudioLeft->Num();
			check(Inputs.AudioRight->Num() == NumFrames);
			
			if (*Inputs.Bypass)
			{
				FMemory::Memcpy(Outputs.AudioLeft->GetData(), Inputs.AudioLeft->GetData(), NumFrames * sizeof(float));
				FMemory::Memcpy(Outputs.AudioRight->GetData(), Inputs.AudioRight->GetData(), NumFrames * sizeof(float));
				return;
			}

			// Copy the inputs to the work buffers
			WorkBufferLeft.SetNumUninitialized(NumFrames);
			WorkBufferRight.SetNumUninitialized(NumFrames);
			FMemory::Memcpy(WorkBufferLeft.GetData(), Inputs.AudioLeft->GetData(), NumFrames * sizeof(float));
			FMemory::Memcpy(WorkBufferRight.GetData(), Inputs.AudioRight->GetData(), NumFrames * sizeof(float));
			
			const float CurrentWetLevel = FMath::Clamp(*Inputs.WetLevel, 0.0f, 1.0f);

			// Apply the wet gain to the input to preserve the reverb tail
			if (LastWetLevel >= 0.0f && !FMath::IsNearlyEqual(CurrentWetLevel, LastWetLevel))
			{
				Audio::ArrayFade(WorkBufferLeft, LastWetLevel, CurrentWetLevel);
				Audio::ArrayFade(WorkBufferRight, LastWetLevel, CurrentWetLevel);
			}
			else
			{
				Audio::ArrayMultiplyByConstantInPlace(WorkBufferLeft, CurrentWetLevel);
				Audio::ArrayMultiplyByConstantInPlace(WorkBufferRight, CurrentWetLevel);
			}
			
			LastWetLevel = CurrentWetLevel;

			// Process
			Reverb.ProcessAudioStereoNonInterleaved(WorkBufferLeft, WorkBufferRight, *Outputs.AudioLeft, *Outputs.AudioRight);

			// Mix in the dry signal
			const float DryLevel = FMath::Clamp(*Inputs.DryLevel, 0.0f, 1.0f);
			Audio::ArrayMixIn(*Inputs.AudioLeft, *Outputs.AudioLeft, DryLevel);
			Audio::ArrayMixIn(*Inputs.AudioRight, *Outputs.AudioRight, DryLevel);
		}

	private:
		FInputs Inputs;
		FOutputs Outputs;
		
		Audio::FPlateReverbFast Reverb;
		Audio::FAlignedFloatBuffer WorkBufferLeft;
		Audio::FAlignedFloatBuffer WorkBufferRight;
		float LastWetLevel{ -1 };
	};

	class FPlateReverbNode final : public FNodeFacade
	{
	public:
		explicit FPlateReverbNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FPlateReverbOperator>())
		{
		}
	};

	METASOUND_REGISTER_NODE(FPlateReverbNode)
}

#undef LOCTEXT_NAMESPACE
