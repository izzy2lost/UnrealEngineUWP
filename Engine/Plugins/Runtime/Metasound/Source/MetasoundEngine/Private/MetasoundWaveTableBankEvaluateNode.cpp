// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "MetasoundDataFactory.h"
#include "MetasoundEngineNodesNames.h"
#include "MetasoundEnumRegistrationMacro.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundPrimitives.h"
#include "MetasoundParamHelper.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundWaveTable.h"
#include "WaveTable.h"
#include "WaveTableSampler.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes"

namespace Metasound
{
	namespace WaveTableBankEvaluateNode
	{
		METASOUND_PARAM(WaveTableBankParam, "WaveTableBank", "The WaveTableBank to evaluate");
		METASOUND_PARAM(InputParam, "Input", "[0, 1] the X input with which to evaluate the wavetable");
		METASOUND_PARAM(IndexParam, "Index", "The table index to interpolate and evaluate the result of (wraps over number of entries)");
		METASOUND_PARAM(OutParam, "Output", "The linearly mixed value of the provided WaveTableBank's applicable entries");
	} // WaveTableBankEvaluateNode
	
	class FMetasoundWaveTableBankEvaluateNodeOperator : public TExecutableOperator<FMetasoundWaveTableBankEvaluateNodeOperator>
	{
	public:
		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace WaveTable;
			using namespace WaveTableBankEvaluateNode;

			static const FVertexInterface DefaultInterface(
				FInputVertexInterface(
					TInputDataVertex<FWaveTableBankAsset>(METASOUND_GET_PARAM_NAME_AND_METADATA(WaveTableBankParam)),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputParam), 0.0f),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(IndexParam), 0.0f),
					TInputDataVertex<FEnumWaveTableInterpolationMode>("Interpolation", FDataVertexMetadata
					{
						LOCTEXT("MetasoundWaveTableBankEvaluateNode_InterpDescription", "How interpolation occurs between WaveTable values."),
						LOCTEXT("MetasoundWaveTableBankEvaluateNode_Interp", "Interpolation"),
						true /* bIsAdvancedDisplay */
					}, static_cast<int32>(FWaveTableSampler::EInterpolationMode::Linear))
				),
				FOutputVertexInterface(
					TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutParam))
				)
			);

			return DefaultInterface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				FNodeClassMetadata Metadata
				{
					{ EngineNodes::Namespace, "WaveTableBankEvaluate", "" },
					1, // Major Version
					0, // Minor Version
					LOCTEXT("MetasoundWaveTableBankEvaluateNode_Name", "Evaluate WaveTableBank"),
					LOCTEXT("MetasoundWaveTableBankEvaluateNode_Description",
						"Evaluates a WaveTableBank's given entires for a given input value, linearly interpolating inline between using the provided index. More performant "
						"than using 'WaveTableGet' and using the 'WaveTableEvaluate' nodes as no resulting WaveTable is generated any time the input float index is changed."),
					PluginAuthor,
					PluginNodeMissingPrompt,
					GetDefaultInterface(),
					{ NodeCategories::WaveTables },
					{ NodeCategories::Envelopes, METASOUND_LOCTEXT("WaveTableEvaluateCurveKeyword", "Curve") },
					{ }
				};

				return Metadata;
			};

			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
			return Metadata;
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace WaveTable;

			const FInputVertexInterfaceData& InputData = InParams.InputData;

			FWaveTableBankAssetReadRef InWaveTableReadRef = InputData.GetOrConstructDataReadReference<FWaveTableBankAsset>("WaveTableBank");
			FFloatReadRef InInputReadRef = InputData.GetOrCreateDefaultDataReadReference<float>("Input", InParams.OperatorSettings);
			FFloatReadRef InIndexReadRef = InputData.GetOrCreateDefaultDataReadReference<float>("Index", InParams.OperatorSettings);
			FEnumWaveTableInterpModeReadRef InInterpReadRef = InputData.GetOrCreateDefaultDataReadReference<FEnumWaveTableInterpolationMode>("Interpolation", InParams.OperatorSettings);

			return MakeUnique<FMetasoundWaveTableBankEvaluateNodeOperator>(InParams, InWaveTableReadRef, InInputReadRef, InIndexReadRef, InInterpReadRef);
		}

		FMetasoundWaveTableBankEvaluateNodeOperator(
			const FBuildOperatorParams& InParams,
			const FWaveTableBankAssetReadRef& InWaveTableBankReadRef,
			const FFloatReadRef& InInputReadRef,
			const FFloatReadRef& InIndexReadRef,
			const FEnumWaveTableInterpModeReadRef& InInterpModeReadRef)
			: WaveTableBankReadRef(InWaveTableBankReadRef)
			, InputReadRef(InInputReadRef)
			, IndexReadRef(InIndexReadRef)
			, InterpModeReadRef(InInterpModeReadRef)
			, OutWriteRef(TDataWriteReferenceFactory<float>::CreateAny(InParams.OperatorSettings))
		{
			Reset(InParams);
		}

		virtual ~FMetasoundWaveTableBankEvaluateNodeOperator() = default;

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace WaveTableBankEvaluateNode;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(WaveTableBankParam), WaveTableBankReadRef);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputParam), InputReadRef);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(IndexParam), IndexReadRef);
			InOutVertexData.BindReadVertex("Interpolation", InterpModeReadRef);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace WaveTableBankEvaluateNode;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutParam), OutWriteRef);
		}
		
		virtual FDataReferenceCollection GetInputs() const override
		{
			// This should never be called. Bind(...) is called instead. This method
			// exists as a stop-gap until the API can be deprecated and removed. 
			checkNoEntry();

			FDataReferenceCollection InputDataReferences;
			return InputDataReferences;
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			// This should never be called. Bind(...) is called instead. This method
			// exists as a stop-gap until the API can be deprecated and removed. 
			checkNoEntry();

			FDataReferenceCollection OutputDataReferences;
			return OutputDataReferences;
		}

		void Execute()
		{
			using namespace WaveTable;

			const FWaveTableBankAsset& WaveTableBankAsset = *WaveTableBankReadRef;
			FWaveTableBankAssetProxyPtr Proxy = WaveTableBankAsset.GetProxy();
			float NewIndex = 0.0f;
			if (!ResolveNextComputeIndex(Proxy, NewIndex))
			{
				return;
			}

			const TArray<FWaveTableData>& WaveTables = WaveTableBankAsset->GetWaveTableData();
			const int32 IndexFloor = FMath::FloorToInt32(NewIndex) % WaveTables.Num();
			const int32 IndexCeil = FMath::CeilToInt32(NewIndex) % WaveTables.Num();

			const FWaveTableData& IndexFloorTable = WaveTables[IndexFloor];

			float IndexFloorValue = 0.0f;
			constexpr FWaveTableSampler::ESingleSampleMode SampleMode =  FWaveTableSampler::ESingleSampleMode::Hold;
			const float Input = FMath::Clamp(*InputReadRef, 0.0f, 1.0f);

			Sampler.Reset();
			Sampler.SetPhase(Input);
			Sampler.Process(IndexFloorTable, IndexFloorValue, SampleMode);

			if (IndexFloor != IndexCeil)
			{
				const FWaveTableData& IndexCeilTable = WaveTables[IndexCeil];
				float IndexCeilValue = 0.0f;
				Sampler.Reset();
				Sampler.SetPhase(Input);
				Sampler.Process(IndexCeilTable, IndexCeilValue, SampleMode);

				const float Fractional = FMath::Frac(NewIndex);
				LastValue = (IndexFloorValue * (1.0f - Fractional)) + (IndexCeilValue * Fractional);
			}
			else
			{
				LastValue = IndexFloorValue;
			}

			*OutWriteRef = LastValue;
		}

		void Reset(const IOperator::FResetParams& InParams)
		{
			using namespace WaveTable;
			FWaveTableSampler::FSettings Settings;
			Settings.Freq = 0.0f; // Sampler phase is manually progressed via this node
			Sampler = FWaveTableSampler(MoveTemp(Settings));

			*OutWriteRef = 0.f;

			LastInterpolationMode = WaveTable::FWaveTableSampler::EInterpolationMode::COUNT;
			LastIndex = -1.0f;
			LastValue = 0.0f;
		}

	private:
		// Returns true if new computation is required, setting OutIndex to index to compute.
		// Returns false if new computation isn't required, resetting output & cached data accordingly.
		bool ResolveNextComputeIndex(const FWaveTableBankAssetProxyPtr& Proxy, float& OutIndex)
		{
			using namespace WaveTable;

			if (!Proxy.IsValid())
			{
				LastValue = 0.0f;
				*OutWriteRef = LastValue;
				return false;
			}

			const float Index = *IndexReadRef;
			const FWaveTableSampler::EInterpolationMode NewInterpolationMode = *InterpModeReadRef;
			if (LastInterpolationMode == NewInterpolationMode)
			{
				if (FMath::IsNearlyEqual(Index, LastIndex))
				{
					*OutWriteRef = LastValue;
					return false;
				}
			}

			LastInterpolationMode = NewInterpolationMode;
			LastIndex = Index;
			Sampler.SetInterpolationMode(NewInterpolationMode);
			OutIndex = FMath::Abs(Index); // Avoids fractional, interpolative flip at zero crossing
			return true;
		}

		FWaveTableBankAssetReadRef WaveTableBankReadRef;
		FFloatReadRef InputReadRef;
		FFloatReadRef IndexReadRef;
		FEnumWaveTableInterpModeReadRef InterpModeReadRef;

		WaveTable::FWaveTableSampler Sampler;

		FFloatWriteRef OutWriteRef;

		WaveTable::FWaveTableSampler::EInterpolationMode LastInterpolationMode = WaveTable::FWaveTableSampler::EInterpolationMode::COUNT;
		float LastIndex = -1.0f;
		float LastValue = 0.0f;
	};

	class FMetasoundWaveTableBankEvaluateNode : public FNodeFacade
	{
	public:
		FMetasoundWaveTableBankEvaluateNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FMetasoundWaveTableBankEvaluateNodeOperator>())
		{
		}

		virtual ~FMetasoundWaveTableBankEvaluateNode() = default;
	};

	METASOUND_REGISTER_NODE(FMetasoundWaveTableBankEvaluateNode)
} // namespace Metasound

#undef LOCTEXT_NAMESPACE // MetasoundStandardNodes
