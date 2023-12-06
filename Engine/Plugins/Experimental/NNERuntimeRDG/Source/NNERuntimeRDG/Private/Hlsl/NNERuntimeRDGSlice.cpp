// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGSlice.h"
#include "NNEHlslShadersSliceCS.h"
#include "NNERuntimeRDGHelperSlice.h"
#include "NNERuntimeRDGHlslHelper.h"
#include "NNETensor.h"
#include "NNETypes.h"
#include "RenderGraphUtils.h"

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	DECLARE_GPU_STAT_NAMED(FNNEOperatorSlice, TEXT("NNE.Operator.Hlsl.Slice"));

	/**
	 * Slice operator implementation
	 */
	class FSlice : public FOperatorHlsl
	{
	public:

		FSlice() {}
		virtual ~FSlice() = default;

	private:

		TArray<int32> AxesAttr;
		TArray<int32> EndsAttr;
		TArray<int32> StartsAttr;

		inline void GetStartAndEndFromInputShape(TConstArrayView<uint32> InputShapeData, 
			TArray<int32, TInlineAllocator<NNE::FTensorShape::MaxRank>>& Start,
			TArray<int32, TInlineAllocator<NNE::FTensorShape::MaxRank>>& End) const
		{
			const int32 InputRank = InputShapeData.Num();
			TArray<int32, TInlineAllocator<NNE::FTensorShape::MaxRank>> Axes(AxesAttr);
			TArray<int32, TInlineAllocator<NNE::FTensorShape::MaxRank>> Ends(EndsAttr);
			TArray<int32, TInlineAllocator<NNE::FTensorShape::MaxRank>> Starts(StartsAttr);

			//see https://github.com/onnx/onnx/blob/main/docs/Operators.md#slice for algorithm
			Start.SetNum(InputRank);
			End.SetNum(InputRank);
			for (int32 i = 0; i < InputRank; ++i)
			{
				Start[i] = 0;
				End[i] = InputShapeData[i];
			}
			for (int32 i = 0; i < Axes.Num(); ++i)
			{
				if (Axes[i] < 0)
				{
					Axes[i] += InputRank;
				}
			}
			for (int32 i = 0; i < Starts.Num(); ++i)
			{
				if (Starts[i] < 0)
				{
					Starts[i] += InputShapeData[Axes[i]];
				}
			}
			for (int32 i = 0; i < Ends.Num(); ++i)
			{
				if (Ends[i] < 0)
				{
					Ends[i] += InputShapeData[Axes[i]];
				}
			}
			for (int32 i = 0; i < Axes.Num(); ++i)
			{
				Start[Axes[i]] = FMath::Clamp(Starts[i], 0, InputShapeData[Axes[i]]);
				End[Axes[i]] = FMath::Clamp(Ends[i], 0, InputShapeData[Axes[i]]);
			}
		}

	public:

		virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
		{
			check(InputTensors.Num() == 1);
			check(OutputTensors.Num() == 1);

			TConstArrayView<uint32> InputShapeData(InputTensors[0]->GetShape().GetData());
			const int32 InputRank = InputShapeData.Num();
			TArray<int32, TInlineAllocator<NNE::FTensorShape::MaxRank>> Start(AxesAttr);
			TArray<int32, TInlineAllocator<NNE::FTensorShape::MaxRank>> End(EndsAttr);

			GetStartAndEndFromInputShape(InputShapeData, Start, End);

			TArray<uint32> OutputShapeData;

			OutputShapeData.Reserve(InputRank);
			for (int32 i = 0; i < InputRank; ++i)
			{
				OutputShapeData.Add(End[i] - Start[i]);
			}
			
			NNE::FTensorShape OutputShape = NNE::FTensorShape::Make(OutputShapeData);

			OutputTensors[0]->SetShape(OutputShape);
			
			Internal::CPUHelper::Slice::Apply(*InputTensors[0], *OutputTensors[0], Start);

			return 0;
		};

		virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> InputTensorDescs, TConstArrayView<NNE::FTensorDesc> OutputTensorDescs, const NNE::FAttributeMap& Attributes) override
		{
			check(InputTensorDescs.Num() == 1);
			check(OutputTensorDescs.Num() == 1);

			EndsAttr = Attributes.GetValue<TArray<int32>>(TEXT("ends"));
			StartsAttr = Attributes.GetValue<TArray<int32>>(TEXT("starts"));
			
			TArray<int32> AxesDefault;
			
			for (int i = 0; i < StartsAttr.Num(); ++i)
			{
				AxesDefault.Add(i);
			}
			
			AxesAttr = Attributes.GetValueOrDefault<TArray<int32>>(TEXT("axes"), AxesDefault);

			if (EndsAttr.Num() != StartsAttr.Num() || AxesAttr.Num() != StartsAttr.Num())
			{
				UE_LOG(LogNNE, Warning, TEXT("Unsqueeze: Starts, Ends and Axes must be of the same size."));
				return false;
			}
			
			return true;
		}

		virtual void Dispatch(FRDGBuilder& GraphBuilder, TConstArrayView<FTensorRDGRef> InputTensors, TConstArrayView<FTensorRDGRef> OutputTensors) override
		{
			using namespace UE::NNEHlslShaders::Internal;

			check(InputTensors.Num() == 1);
			check(OutputTensors.Num() == 1);
			check(InputTensors[0] != nullptr);
			check(OutputTensors[0] != nullptr);

			const FTensorRDG& Input = *InputTensors[0];
			const FTensorRDG& Output = *OutputTensors[0];
			const FRDGBufferSRVRef InputSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Input.GetBuffer(), PF_R32_FLOAT));
			const FRDGBufferUAVRef OutputUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Output.GetBuffer(), PF_R32_FLOAT));
			const FIntVector ThreadGroupCount = ComputeElementWiseThreadGroups(Output.GetVolume(), FSliceConstants::NUM_GROUP_THREADS);

			TConstArrayView<uint32> InputShapeData(Input.GetShape().GetData());
			TArray<int32, TInlineAllocator<NNE::FTensorShape::MaxRank>> Start(AxesAttr);
			TArray<int32, TInlineAllocator<NNE::FTensorShape::MaxRank>> End(EndsAttr);

			GetStartAndEndFromInputShape(InputShapeData, Start, End);

			// Set parameters
			FSliceCS::FParameters* Params = GraphBuilder.AllocParameters<FSliceCS::FParameters>();
			Params->Input = InputSRV;
			Params->Output = OutputUAV;
			Params->Num = Output.GetVolume();
			Params->ThreadCountX = ThreadGroupCount.X * FSliceConstants::NUM_GROUP_THREADS;
			FillTensorStrideShaderParameters(Input, Params->TensorInfo, 0);
			FillTensorStrideShaderParameters(Output, Params->TensorInfo, 1);
			static_assert(NNE::FTensorShape::MaxRank <= NXRT_TENSORSTRIDEINFO_MAX_NUM_DIMENSIONS);
			check(Start.Num() == Input.GetShape().Rank());
			for (int32 i = 0; i < NXRT_TENSORSTRIDEINFO_MAX_NUM_DIMENSIONS; ++i)
			{
				if (i < Start.Num())
				{
					Params->TensorInfo[i][2] = static_cast<uint32>(Start[i]);
				}
				else
				{
					Params->TensorInfo[i][2] = 0;
				}
			}

			FSliceCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FSliceCS::FSliceNumDimensions>(Output.GetShape().Rank());

			TShaderMapRef<FSliceCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

			RDG_EVENT_SCOPE(GraphBuilder, "NNE.Operator.Hlsl.Slice");
			RDG_GPU_STAT_SCOPE(GraphBuilder, FNNEOperatorSlice);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NNE.Operator.Hlsl.Slice.Dispatch"),
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				ComputeShader,
				Params,
				ThreadGroupCount);
		}
	};

	bool ValidateSliceOperator(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputSlices)
	{
		bool bIsValid = true;

		FAttributeValidator AttributeValidator;
		AttributeValidator.AddOptional(TEXT("axes"), ENNEAttributeDataType::Int32Array);
		AttributeValidator.AddRequired(TEXT("ends"), ENNEAttributeDataType::Int32Array);
		AttributeValidator.AddRequired(TEXT("starts"), ENNEAttributeDataType::Int32Array);
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		FInputValidator InputValidator;
		InputValidator.AddSupportedType(ENNETensorDataType::Int64);
		InputValidator.AddSupportedType(ENNETensorDataType::Int32);
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddRequired();

		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	FOperatorHlsl* CreateSliceOperator()
	{
		return new FSlice();
	}

	bool RegisterSliceOperator(FOperatorRegistryHlsl& Registry)
	{
		// Note: support of a particular version is partial with respect to tensor data types (only the most typical ones are usually supported).
		Registry.OpAdd({{TEXT("Slice"), TEXT("Onnx")}, 1}, CreateSliceOperator, ValidateSliceOperator);
		return true;
	}
} // UE::NNERuntimeRDG::Private::Hlsl
