// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeBasicCpuModel.h"

#include "NNE.h"
#include "NNERuntimeBasicCpu.h"

#define NNE_RUNTIME_BASIC_ENABLE_ISPC INTEL_ISPC
//#define NNE_RUNTIME_BASIC_ENABLE_ISPC 0

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
#include "NNERuntimeBasicCpu.ispc.generated.h"
#endif

namespace UE::NNE::RuntimeBasic
{
	namespace Private
	{
		namespace Serialization
		{
			//--------------------------------------------------------------------------

			static inline void Align(uint64& InOutOffset, const uint32 Alignment)
			{
				InOutOffset = ((InOutOffset + Alignment - 1) / Alignment) * Alignment;
			}

			//--------------------------------------------------------------------------

			static inline void Size(uint64& InOutOffset, const uint32& In)
			{
				Align(InOutOffset, sizeof(uint32));
				InOutOffset += sizeof(uint32);
			}

			static inline void Size(uint64& InOutOffset, const float& In)
			{
				Align(InOutOffset, sizeof(float));
				InOutOffset += sizeof(float);
			}

			static inline void Size(uint64& InOutOffset, const TConstArrayView<float> In)
			{
				Align(InOutOffset, 64);
				InOutOffset += In.Num() * sizeof(float);
			}

			static inline void Size(uint64& InOutOffset, const TConstArrayView<uint16> In)
			{
				Align(InOutOffset, 64);
				InOutOffset += In.Num() * sizeof(uint16);
			}

			static inline void Size(uint64& InOutOffset, const TSharedPtr<ILayer>& InLayer);

			//--------------------------------------------------------------------------

			static inline void Load(uint64& InOutOffset, uint32& Out, TConstArrayView<uint8> Data)
			{
				Align(InOutOffset, sizeof(uint32));
				Out = *((uint32*)&Data[InOutOffset]);
				InOutOffset += sizeof(uint32);
			}

			static inline void Load(uint64& InOutOffset, float& Out, TConstArrayView<uint8> Data)
			{
				Align(InOutOffset, sizeof(float));
				Out = *((float*)&Data[InOutOffset]);
				InOutOffset += sizeof(float);
			}

			static inline void Load(uint64& InOutOffset, TConstArrayView<float>& Out, TConstArrayView<uint8> Data, int32 Size)
			{
				Align(InOutOffset, 64);
				Out = MakeArrayView<float>((float*)&Data[InOutOffset], Size);
				InOutOffset += Size * sizeof(float);
			}

			static inline void Load(uint64& InOutOffset, TConstArrayView<uint16>& Out, TConstArrayView<uint8> Data, int32 Size)
			{
				Align(InOutOffset, 64);
				Out = MakeArrayView<uint16>((uint16*)&Data[InOutOffset], Size);
				InOutOffset += Size * sizeof(uint16);
			}

			static inline void Load(uint64& InOutOffset, TSharedPtr<ILayer>& OutLayer, TConstArrayView<uint8> Data);

			//--------------------------------------------------------------------------

			static inline void Save(uint64& InOutOffset, const uint32 In, TArrayView<uint8> Data)
			{
				Align(InOutOffset, sizeof(uint32));
				*((uint32*)&Data[InOutOffset]) = In;
				InOutOffset += sizeof(uint32);
			}

			static inline void Save(uint64& InOutOffset, const float In, TArrayView<uint8> Data)
			{
				Align(InOutOffset, sizeof(float));
				*((float*)&Data[InOutOffset]) = In;
				InOutOffset += sizeof(float);
			}

			static inline void Save(uint64& InOutOffset, TConstArrayView<float> In, TArrayView<uint8> Data)
			{
				Align(InOutOffset, 64);
				FMemory::Memcpy(&Data[InOutOffset], In.GetData(), In.Num() * sizeof(float));
				InOutOffset += In.Num() * sizeof(float);
			}

			static inline void Save(uint64& InOutOffset, TConstArrayView<uint16> In, TArrayView<uint8> Data)
			{
				Align(InOutOffset, 64);
				FMemory::Memcpy(&Data[InOutOffset], In.GetData(), In.Num() * sizeof(uint16));
				InOutOffset += In.Num() * sizeof(uint16);
			}

			static inline void Save(uint64& InOutOffset, const TSharedPtr<ILayer>& InLayer, TArrayView<uint8> Data);
		}

		//--------------------------------------------------------------------------

		static inline float Sigmoid(const float X)
		{
			return 1.0f / (1.0f + FMath::Exp(-X));
		}

		//--------------------------------------------------------------------------

		static inline void IdentityLayer(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const uint32 BatchSize,
			const uint32 InputOutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUIdentityLayer(
				Output,
				Input,
				BatchSize,
				InputOutputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < InputOutputSize; Idx++)
				{
					Output[BatchIdx * OutputStride + Idx] = Input[BatchIdx * InputStride + Idx];
				}
			}
#endif
		}

		static inline void NormalizeLayer(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const float* RESTRICT Mean,
			const float* RESTRICT Std,
			const uint32 BatchSize,
			const uint32 InputOutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUNormalizeLayer(
				Output,
				Input,
				Mean,
				Std,
				BatchSize,
				InputOutputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < InputOutputSize; Idx++)
				{
					Output[BatchIdx * OutputStride + Idx] = (Input[BatchIdx * InputStride + Idx] - Mean[Idx]) / Std[Idx];
				}
			}
#endif
		}

		static inline void DenormalizeLayer(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const float* RESTRICT Mean,
			const float* RESTRICT Std,
			const uint32 BatchSize,
			const uint32 InputOutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUDenormalizeLayer(
				Output,
				Input,
				Mean,
				Std,
				BatchSize,
				InputOutputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < InputOutputSize; Idx++)
				{
					Output[BatchIdx * OutputStride + Idx] = (Input[BatchIdx * InputStride + Idx] * Std[Idx]) + Mean[Idx];
				}
			}
#endif
		}

		static inline void LinearLayer(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const float* RESTRICT Weights,
			const float* RESTRICT Biases,
			const uint32 BatchSize,
			const uint32 InputSize,
			const uint32 OutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPULinearLayer(
				Output,
				Input,
				Weights,
				Biases,
				BatchSize,
				InputSize,
				OutputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 ColIdx = 0; ColIdx < OutputSize; ColIdx++)
				{
					Output[BatchIdx * OutputStride + ColIdx] = Biases[ColIdx];
				}
			}

			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 RowIdx = 0; RowIdx < InputSize; RowIdx++)
				{
					const float Value = Input[BatchIdx * InputStride + RowIdx];

					if (Value != 0.0)
					{
						for (uint32 ColIdx = 0; ColIdx < OutputSize; ColIdx++)
						{
							Output[BatchIdx * OutputStride + ColIdx] += Value * Weights[RowIdx * OutputSize + ColIdx];
						}
					}
				}
			}
#endif
		}

		static inline void CompressedLinearLayer(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const uint16* RESTRICT Weights,
			const float* RESTRICT WeightOffsets,
			const float* RESTRICT WeightScales,
			const float* RESTRICT Biases,
			const uint32 BatchSize,
			const uint32 InputSize,
			const uint32 OutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUCompressedLinearLayer(
				Output,
				Input,
				Weights,
				WeightOffsets,
				WeightScales,
				Biases,
				BatchSize,
				InputSize,
				OutputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 ColIdx = 0; ColIdx < OutputSize; ColIdx++)
				{
					Output[BatchIdx * OutputStride + ColIdx] = Biases[ColIdx];
				}
			}

			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 RowIdx = 0; RowIdx < InputSize; RowIdx++)
				{
					const float Value = Input[BatchIdx * InputStride + RowIdx];

					if (Value != 0.0)
					{
						const float Offset = WeightOffsets[RowIdx];
						const float Scales = WeightScales[RowIdx];

						for (uint32 ColIdx = 0; ColIdx < OutputSize; ColIdx++)
						{
							Output[BatchIdx * OutputStride + ColIdx] += Value * ((Scales * ((float)Weights[RowIdx * OutputSize + ColIdx])) + Offset);
						}
					}
				}
			}
#endif
		}

		static inline void MultiLinearLayer(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const float* RESTRICT Weights,
			const float* RESTRICT Biases,
			const uint32 BatchSize,
			const uint32 BlockNum,
			const uint32 InputSize,
			const uint32 OutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
			// For this function ispc generates slightly less efficient code than the naive C++ implementation so we
			// don't bother calling out to the ispc version even if it is available

			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 BlockIdx = 0; BlockIdx < BlockNum; BlockIdx++)
				{
					for (uint32 ColIdx = 0; ColIdx < OutputSize; ColIdx++)
					{
						Output[BatchIdx * OutputStride + BlockIdx * OutputSize + ColIdx] = Biases[BlockIdx * OutputSize + ColIdx];
					}
				}
			}

			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 BlockIdx = 0; BlockIdx < BlockNum; BlockIdx++)
				{
					for (uint32 RowIdx = 0; RowIdx < InputSize; RowIdx++)
					{
						const float Value = Input[BatchIdx * InputStride + BlockIdx * InputSize + RowIdx];

						if (Value != 0.0)
						{
							for (uint32 ColIdx = 0; ColIdx < OutputSize; ColIdx++)
							{
								Output[BatchIdx * OutputStride + BlockIdx * OutputSize + ColIdx] += Value * Weights[BlockIdx * InputSize * OutputSize + RowIdx * OutputSize + ColIdx];
							}
						}
					}
				}
			}
		}

		static inline void ActivationReLU(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const uint32 BatchSize,
			const uint32 InputOutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUActivationReLU(
				Output,
				Input,
				BatchSize,
				InputOutputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < InputOutputSize; Idx++)
				{
					Output[BatchIdx * OutputStride + Idx] = FMath::Max(Input[BatchIdx * InputStride + Idx], 0.0f);
				}
			}
#endif
		}

		static inline void ActivationELU(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const uint32 BatchSize,
			const uint32 InputOutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUActivationELU(
				Output,
				Input,
				BatchSize,
				InputOutputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < InputOutputSize; Idx++)
				{
					const float Value = Input[BatchIdx * InputStride + Idx];
					Output[BatchIdx * OutputStride + Idx] = Value > 0.0f ? Value : FMath::InvExpApprox(-Value) - 1.0f;
				}
			}
#endif
		}

		static inline void ActivationTanH(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const uint32 BatchSize,
			const uint32 InputOutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUActivationTanH(
				Output,
				Input,
				BatchSize,
				InputOutputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < InputOutputSize; Idx++)
				{
					Output[BatchIdx * OutputStride + Idx] = FMath::Tanh(Input[BatchIdx * InputStride + Idx]);
				}
			}
#endif
		}

		static inline void ActivationPReLU(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const float* RESTRICT Alpha,
			const uint32 BatchSize,
			const uint32 InputOutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUActivationPReLU(
				Output,
				Input,
				Alpha,
				BatchSize,
				InputOutputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < InputOutputSize; Idx++)
				{
					const float Value = Input[BatchIdx * InputStride + Idx];
					Output[BatchIdx * OutputStride + Idx] = Value > 0.0f ? Value : Alpha[Idx] * Value;
				}
			}
#endif
		}

		static inline void MemoryCellUpdateMemory(
			float* RESTRICT Output,
			const float* RESTRICT RememberGate,
			const float* RESTRICT Memory,
			const float* RESTRICT Update,
			const uint32 BatchSize,
			const uint32 MemorySize,
			const uint32 OutputStride,
			const uint32 RememberGateStride,
			const uint32 MemoryStride,
			const uint32 UpdateStride)
		{
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < MemorySize; Idx++)
				{
					const float Gate = Sigmoid(RememberGate[BatchIdx * RememberGateStride + Idx]);
					const float Prev = Memory[BatchIdx * MemoryStride + Idx];
					const float Targ = FMath::Tanh(Update[BatchIdx * UpdateStride + Idx]);

					Output[BatchIdx * OutputStride + Idx] = (1.0f - Gate) * Prev + Gate * Targ;
				}
			}
		}

		static inline void MemoryCellUpdateOutput(
			float* RESTRICT Output,
			const float* RESTRICT PassthroughGate,
			const float* RESTRICT MemoryUpdate,
			const float* RESTRICT InputUpdate,
			const uint32 BatchSize,
			const uint32 OutputSize,
			const uint32 OutputStride,
			const uint32 PassthroughGateStride,
			const uint32 MemoryUpdateStride,
			const uint32 InputUpdateStride)
		{
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < OutputSize; Idx++)
				{
					const float Gate = Sigmoid(PassthroughGate[BatchIdx * PassthroughGateStride + Idx]);
					const float MemTarg = FMath::Tanh(MemoryUpdate[BatchIdx * MemoryUpdateStride + Idx]);
					const float InTarg = FMath::Tanh(InputUpdate[BatchIdx * InputUpdateStride + Idx]);

					Output[BatchIdx * OutputStride + Idx] = (1.0f - Gate) * MemTarg + Gate * InTarg;
				}
			}
		}

		//--------------------------------------------------------------------------

		/** Layer Type Id - this should match what is given in nne_runtime_basic_cpu.py  */
		enum class ELayerType : uint32
		{
			Invalid = 0,
			Sequence = 1,
			Normalize = 2,
			Denormalize = 3,
			Linear = 4,
			CompressedLinear = 5,
			MultiLinear = 6,
			ReLU = 7,
			ELU = 8,
			TanH = 9,
			PReLU = 10,
			MemoryCell = 11,
			MemoryBackbone = 12,
		};

		//--------------------------------------------------------------------------

		/**
		 * Interface for a Layer Instance - the data required for performing inference for a layer.
		 */
		struct ILayerInstance
		{
			/** Virtual destructor */
			virtual ~ILayerInstance() = default;

			/** Indicate to this layer instance what the batchsize is going to be when performing inference. */
			virtual void SetBatchSize(const uint32 BatchSize) = 0;
		};

		/**
		 * Interface for a Layer - the network parameter data required for a layer.
		 */
		struct ILayer
		{
			/** Virtual destructor */
			virtual ~ILayer() = default;

			/** Create the instance data required for this type of layer. */
			virtual TSharedPtr<ILayerInstance> MakeInstance() const { return nullptr; };

			/** Get the layer type. */
			virtual ELayerType GetLayerType() const = 0;

			/** Get the size of the input vector. */
			virtual uint32 GetInputSize() const = 0;

			/** Get the size of the output vector. */
			virtual uint32 GetOutputSize() const = 0;

			/** Compute the size required to serialize this layer by growing InOutOffset. */
			virtual void SerializationSize(uint64& InOutOffset) const = 0;

			/** Load this layer from the buffer at the given offset. */
			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) = 0;

			/** Save this layer from the buffer at the given offset. */
			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const = 0;

			/**
			 * Evaluate this layer.
			 * 
			 * Note that the input and output buffers may not match the exact dimensions expected by this layer - the
			 * size dimension can be over-allocated to allow for buffer re-use.
			 *
			 * @param Output			The output buffer of size (BatchSize, OutputStride).
			 * @param Input				The input buffer of size (BatchSize, InputStride).
			 * @param Instance			The instance data for this layer.
			 * @param BatchSize			The batchsize of the inputs and outputs
			 * @param OutputStride		The stride of the output buffer vector for each item in the batch
			 * @param InputStride		The stride of the input buffer vector for each item in the batch
			 */
			virtual void Evaluate(
				TArrayView<float> Output,
				TConstArrayView<float> Input,
				ILayerInstance* Instance,
				uint32 BatchSize,
				uint32 OutputStride,
				uint32 InputStride) = 0;
		};

		//--------------------------------------------------------------------------

		struct FSequenceLayer;

		struct FSequenceLayerInstance : public ILayerInstance
		{
			FSequenceLayerInstance(const FSequenceLayer& InSequenceLayer);

			virtual void SetBatchSize(const uint32 BatchSize) override final;

			const FSequenceLayer& SequenceLayer;
			uint32 ActivationStride = 0;
			TArray<TSharedPtr<ILayerInstance>, TInlineAllocator<32>> Instances;
			TArray<float, TInlineAllocator<512>> ActivationBufferFront;
			TArray<float, TInlineAllocator<512>> ActivationBufferBack;
		};

		//--------------------------------------------------------------------------

		struct FSequenceLayer : public ILayer
		{
			virtual TSharedPtr<ILayerInstance> MakeInstance() const { return MakeShared<FSequenceLayerInstance>(*this); };
			virtual ELayerType GetLayerType() const override final { return ELayerType::Sequence; }
			virtual uint32 GetInputSize() const override final { return Layers.Num() > 0 ? Layers[0]->GetInputSize() : 0; }
			virtual uint32 GetOutputSize() const override final { return Layers.Num() > 0 ? Layers.Last()->GetOutputSize() : 0; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, (uint32)Layers.Num());
				for (const TSharedPtr<ILayer>& Layer : Layers)
				{
					Serialization::Size(InOutOffset, Layer);
				}
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				uint32 LayerNum = 0;
				Serialization::Load(InOutOffset, LayerNum, Data);
				Layers.SetNumZeroed(LayerNum);

				for (uint32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
				{
					Serialization::Load(InOutOffset, Layers[LayerIdx], Data);
				}
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				uint32 LayerNum = Layers.Num();
				Serialization::Save(InOutOffset, LayerNum, Data);
				for (uint32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
				{
					Serialization::Save(InOutOffset, Layers[LayerIdx], Data);
				}
			}

			virtual void Evaluate(
				TArrayView<float> Output,
				TConstArrayView<float> Input,
				ILayerInstance* Instance,
				uint32 BatchSize,
				uint32 OutputStride,
				uint32 InputStride) override final
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(NNE::RuntimeBasic::Private::FSequenceLayer::Evaluate);

				const uint32 LayerNum = Layers.Num();
				check(LayerNum > 0);

				FSequenceLayerInstance* SequenceInstance = StaticCast<FSequenceLayerInstance*>(Instance);
				check(SequenceInstance);

				// If we just have one layer then evaluate layer directly without using intermediate storage

				if (LayerNum == 1)
				{
					Layers[0]->Evaluate(
						Output,
						Input,
						SequenceInstance->Instances[0].Get(),
						BatchSize,
						OutputStride,
						InputStride);

					return;
				}

				// Otherwise evaluate first layer from input into activation buffer

				Layers[0]->Evaluate(
					SequenceInstance->ActivationBufferFront,
					Input,
					SequenceInstance->Instances[0].Get(),
					BatchSize,
					SequenceInstance->ActivationStride,
					InputStride);

				// Evaluate intermediate layers using front and back buffers

				for (uint32 LayerIdx = 1; LayerIdx < LayerNum - 1; LayerIdx++)
				{
					TConstArrayView<float> LayerInput = LayerIdx % 2 == 0 ? 
						SequenceInstance->ActivationBufferBack : 
						SequenceInstance->ActivationBufferFront;

					TArrayView<float> LayerOutput = LayerIdx % 2 == 0 ? 
						SequenceInstance->ActivationBufferFront : 
						SequenceInstance->ActivationBufferBack;

					Layers[LayerIdx]->Evaluate(
						LayerOutput,
						LayerInput,
						SequenceInstance->Instances[LayerIdx].Get(),
						BatchSize,
						SequenceInstance->ActivationStride,
						SequenceInstance->ActivationStride);
				}

				// Evaluate final layer from activation buffer into output

				TConstArrayView<float> FinalLayerInput = LayerNum % 2 == 0 ? 
					SequenceInstance->ActivationBufferFront : 
					SequenceInstance->ActivationBufferBack;

				Layers.Last()->Evaluate(
					Output,
					FinalLayerInput,
					SequenceInstance->Instances.Last().Get(),
					BatchSize,
					OutputStride,
					SequenceInstance->ActivationStride);
			}

			TArray<TSharedPtr<ILayer>, TInlineAllocator<32>> Layers;
		};

		//--------------------------------------------------------------------------

		FSequenceLayerInstance::FSequenceLayerInstance(const FSequenceLayer& InSequenceLayer) 
			: SequenceLayer(InSequenceLayer)
		{
			const uint32 LayerNum = SequenceLayer.Layers.Num();
			Instances.Init(nullptr, LayerNum);

			for (uint32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
			{
				Instances[LayerIdx] = SequenceLayer.Layers[LayerIdx]->MakeInstance();
			}

			// Compute the largest intermediate size used

			ActivationStride = SequenceLayer.Layers[0]->GetInputSize();
			for (uint32 LayerIdx = 1; LayerIdx < LayerNum; LayerIdx++)
			{
				ActivationStride = FMath::Max(ActivationStride, SequenceLayer.Layers[LayerIdx]->GetOutputSize());
			}
		}

		void FSequenceLayerInstance::SetBatchSize(const uint32 BatchSize)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(NNE::RuntimeBasic::Private::FSequenceLayerInstance::SetBatchSize);

			// Propagate call to sub-layer instances

			for (const TSharedPtr<ILayerInstance>& Instance : Instances)
			{
				// Most layers don't allocate instance data and so we need to check for nullptr
				if (Instance)
				{
					Instance->SetBatchSize(BatchSize);
				}
			}

			// Allocate front and back buffers to maximum size. Don't shrink to avoid re-allocation 
			// when smaller batches are requested.

			ActivationBufferFront.SetNumUninitialized(BatchSize * ActivationStride, false);
			ActivationBufferBack.SetNumUninitialized(BatchSize * ActivationStride, false);
		}

		//--------------------------------------------------------------------------

		struct FNormalizeLayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::Normalize; }
			virtual uint32 GetInputSize() const override final { return InputOutputSize; }
			virtual uint32 GetOutputSize() const override final { return InputOutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputOutputSize);
				Serialization::Size(InOutOffset, Mean);
				Serialization::Size(InOutOffset, Std);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputOutputSize, Data);
				Serialization::Load(InOutOffset, Mean, Data, InputOutputSize);
				Serialization::Load(InOutOffset, Std, Data, InputOutputSize);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputOutputSize, Data);
				Serialization::Save(InOutOffset, Mean, Data);
				Serialization::Save(InOutOffset, Std, Data);
			}

			virtual void Evaluate(
				TArrayView<float> Output,
				TConstArrayView<float> Input,
				ILayerInstance* Instance,
				uint32 BatchSize,
				uint32 OutputStride,
				uint32 InputStride) override final
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(NNE::RuntimeBasic::Private::FNormalizeLayer::Evaluate);

				NormalizeLayer(
					Output.GetData(),
					Input.GetData(),
					Mean.GetData(),
					Std.GetData(),
					BatchSize,
					InputOutputSize,
					OutputStride,
					InputStride);
			}

			uint32 InputOutputSize = 0;
			TConstArrayView<float> Mean;
			TConstArrayView<float> Std;
		};

		//--------------------------------------------------------------------------



		struct FDenormalizeLayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::Denormalize; }
			virtual uint32 GetInputSize() const override final { return InputOutputSize; }
			virtual uint32 GetOutputSize() const override final { return InputOutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputOutputSize);
				Serialization::Size(InOutOffset, Mean);
				Serialization::Size(InOutOffset, Std);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputOutputSize, Data);
				Serialization::Load(InOutOffset, Mean, Data, InputOutputSize);
				Serialization::Load(InOutOffset, Std, Data, InputOutputSize);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputOutputSize, Data);
				Serialization::Save(InOutOffset, Mean, Data);
				Serialization::Save(InOutOffset, Std, Data);
			}

			virtual void Evaluate(
				TArrayView<float> Output,
				TConstArrayView<float> Input,
				ILayerInstance* Instance,
				uint32 BatchSize,
				uint32 OutputStride,
				uint32 InputStride) override final
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(NNE::RuntimeBasic::Private::FDenormalizeLayer::Evaluate);

				DenormalizeLayer(
					Output.GetData(),
					Input.GetData(),
					Mean.GetData(),
					Std.GetData(),
					BatchSize,
					InputOutputSize,
					OutputStride,
					InputStride);
			}

			uint32 InputOutputSize = 0;
			TConstArrayView<float> Mean;
			TConstArrayView<float> Std;
		};

		//--------------------------------------------------------------------------

		struct FLinearLayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::Linear; }
			virtual uint32 GetInputSize() const override final { return InputSize; }
			virtual uint32 GetOutputSize() const override final { return OutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputSize);
				Serialization::Size(InOutOffset, OutputSize);
				Serialization::Size(InOutOffset, Biases);
				Serialization::Size(InOutOffset, Weights);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputSize, Data);
				Serialization::Load(InOutOffset, OutputSize, Data);
				Serialization::Load(InOutOffset, Biases, Data, OutputSize);
				Serialization::Load(InOutOffset, Weights, Data, InputSize * OutputSize);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputSize, Data);
				Serialization::Save(InOutOffset, OutputSize, Data);
				Serialization::Save(InOutOffset, Biases, Data);
				Serialization::Save(InOutOffset, Weights, Data);
			}

			virtual void Evaluate(
				TArrayView<float> Output,
				TConstArrayView<float> Input,
				ILayerInstance* Instance,
				uint32 BatchSize,
				uint32 OutputStride,
				uint32 InputStride) override final
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(NNE::RuntimeBasic::Private::FLinearLayer::Evaluate);

				LinearLayer(
					Output.GetData(),
					Input.GetData(),
					Weights.GetData(),
					Biases.GetData(),
					BatchSize,
					InputSize,
					OutputSize,
					OutputStride,
					InputStride);
			}

			uint32 InputSize = 0;
			uint32 OutputSize = 0;
			TConstArrayView<float> Biases;
			TConstArrayView<float> Weights;
		};

		//--------------------------------------------------------------------------

		struct FCompressedLinearLayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::CompressedLinear; }
			virtual uint32 GetInputSize() const override final { return InputSize; }
			virtual uint32 GetOutputSize() const override final { return OutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputSize);
				Serialization::Size(InOutOffset, OutputSize);
				Serialization::Size(InOutOffset, WeightOffsets);
				Serialization::Size(InOutOffset, WeightScales);
				Serialization::Size(InOutOffset, Biases);
				Serialization::Size(InOutOffset, Weights);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputSize, Data);
				Serialization::Load(InOutOffset, OutputSize, Data);
				Serialization::Load(InOutOffset, WeightOffsets, Data, InputSize);
				Serialization::Load(InOutOffset, WeightScales, Data, InputSize);
				Serialization::Load(InOutOffset, Biases, Data, OutputSize);
				Serialization::Load(InOutOffset, Weights, Data, InputSize * OutputSize);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputSize, Data);
				Serialization::Save(InOutOffset, OutputSize, Data);
				Serialization::Save(InOutOffset, WeightOffsets, Data);
				Serialization::Save(InOutOffset, WeightScales, Data);
				Serialization::Save(InOutOffset, Biases, Data);
				Serialization::Save(InOutOffset, Weights, Data);
			}

			virtual void Evaluate(
				TArrayView<float> Output,
				TConstArrayView<float> Input,
				ILayerInstance* Instance,
				uint32 BatchSize,
				uint32 OutputStride,
				uint32 InputStride) override final
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(NNE::RuntimeBasic::Private::FCompressedLinearLayer::Evaluate);

				CompressedLinearLayer(
					Output.GetData(),
					Input.GetData(),
					Weights.GetData(),
					WeightOffsets.GetData(),
					WeightScales.GetData(),
					Biases.GetData(),
					BatchSize,
					InputSize,
					OutputSize,
					OutputStride,
					InputStride);
			}

			uint32 InputSize = 0;
			uint32 OutputSize = 0;
			TConstArrayView<float> WeightOffsets;
			TConstArrayView<float> WeightScales;
			TConstArrayView<float> Biases;
			TConstArrayView<uint16> Weights;
		};

		//--------------------------------------------------------------------------

		struct FMultiLinearLayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::MultiLinear; }
			virtual uint32 GetInputSize() const override final { return BlockNum * InputSize; }
			virtual uint32 GetOutputSize() const override final { return BlockNum * OutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, BlockNum);
				Serialization::Size(InOutOffset, InputSize);
				Serialization::Size(InOutOffset, OutputSize);
				Serialization::Size(InOutOffset, Biases);
				Serialization::Size(InOutOffset, Weights);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, BlockNum, Data);
				Serialization::Load(InOutOffset, InputSize, Data);
				Serialization::Load(InOutOffset, OutputSize, Data);
				Serialization::Load(InOutOffset, Biases, Data, BlockNum * OutputSize);
				Serialization::Load(InOutOffset, Weights, Data, BlockNum * InputSize * OutputSize);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, BlockNum, Data);
				Serialization::Save(InOutOffset, InputSize, Data);
				Serialization::Save(InOutOffset, OutputSize, Data);
				Serialization::Save(InOutOffset, Biases, Data);
				Serialization::Save(InOutOffset, Weights, Data);
			}

			virtual void Evaluate(
				TArrayView<float> Output,
				TConstArrayView<float> Input,
				ILayerInstance* Instance,
				uint32 BatchSize,
				uint32 OutputStride,
				uint32 InputStride) override final
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(NNE::RuntimeBasic::Private::FMultiLinearLayer::Evaluate);

				MultiLinearLayer(
					Output.GetData(),
					Input.GetData(),
					Weights.GetData(),
					Biases.GetData(),
					BatchSize,
					BlockNum,
					InputSize,
					OutputSize,
					OutputStride,
					InputStride);
			}

			uint32 InputSize = 0;
			uint32 OutputSize = 0;
			uint32 BlockNum = 0;
			TConstArrayView<float> Biases;
			TConstArrayView<float> Weights;
		};

		//--------------------------------------------------------------------------



		struct FReLULayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::ReLU; }
			virtual uint32 GetInputSize() const override final { return InputOutputSize; }
			virtual uint32 GetOutputSize() const override final { return InputOutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputOutputSize);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputOutputSize, Data);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputOutputSize, Data);
			}

			virtual void Evaluate(
				TArrayView<float> Output,
				TConstArrayView<float> Input,
				ILayerInstance* Instance,
				uint32 BatchSize,
				uint32 OutputStride,
				uint32 InputStride) override final
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(NNE::RuntimeBasic::Private::FReLULayer::Evaluate);

				ActivationReLU(
					Output.GetData(),
					Input.GetData(),
					BatchSize,
					InputOutputSize,
					OutputStride,
					InputStride);
			}

			uint32 InputOutputSize = 0;
		};

		//--------------------------------------------------------------------------

		struct FELULayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::ELU; }
			virtual uint32 GetInputSize() const override final { return InputOutputSize; }
			virtual uint32 GetOutputSize() const override final { return InputOutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputOutputSize);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputOutputSize, Data);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputOutputSize, Data);
			}

			virtual void Evaluate(
				TArrayView<float> Output,
				TConstArrayView<float> Input,
				ILayerInstance* Instance,
				uint32 BatchSize,
				uint32 OutputStride,
				uint32 InputStride) override final
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(NNE::RuntimeBasic::Private::FELULayer::Evaluate);

				ActivationELU(
					Output.GetData(),
					Input.GetData(),
					BatchSize,
					InputOutputSize,
					OutputStride,
					InputStride);
			}

			uint32 InputOutputSize = 0;
		};

		//--------------------------------------------------------------------------

		struct FTanHLayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::TanH; }
			virtual uint32 GetInputSize() const override final { return InputOutputSize; }
			virtual uint32 GetOutputSize() const override final { return InputOutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputOutputSize);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputOutputSize, Data);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputOutputSize, Data);
			}

			virtual void Evaluate(
				TArrayView<float> Output,
				TConstArrayView<float> Input,
				ILayerInstance* Instance,
				uint32 BatchSize,
				uint32 OutputStride,
				uint32 InputStride) override final
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(NNE::RuntimeBasic::Private::FTanHLayer::Evaluate);

				ActivationTanH(
					Output.GetData(),
					Input.GetData(),
					BatchSize,
					InputOutputSize,
					OutputStride,
					InputStride);
			}

			uint32 InputOutputSize = 0;
		};

		//--------------------------------------------------------------------------

		struct FPReLULayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::PReLU; }
			virtual uint32 GetInputSize() const override final { return InputOutputSize; }
			virtual uint32 GetOutputSize() const override final { return InputOutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputOutputSize);
				Serialization::Size(InOutOffset, Alpha);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputOutputSize, Data);
				Serialization::Load(InOutOffset, Alpha, Data, InputOutputSize);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputOutputSize, Data);
				Serialization::Save(InOutOffset, Alpha, Data);
			}

			virtual void Evaluate(
				TArrayView<float> Output,
				TConstArrayView<float> Input,
				ILayerInstance* Instance,
				uint32 BatchSize,
				uint32 OutputStride,
				uint32 InputStride) override final
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(NNE::RuntimeBasic::Private::FPReLuLayer::Evaluate);

				ActivationPReLU(
					Output.GetData(),
					Input.GetData(),
					Alpha.GetData(),
					BatchSize,
					InputOutputSize,
					OutputStride,
					InputStride);
			}

			uint32 InputOutputSize = 0;
			TConstArrayView<float> Alpha;
		};

		//--------------------------------------------------------------------------

		struct FMemoryCellLayer;

		struct FMemoryCellInstance : public ILayerInstance
		{
			FMemoryCellInstance(const FMemoryCellLayer& InMemoryCellLayer);

			virtual void SetBatchSize(const uint32 BatchSize) override final;

			const FMemoryCellLayer& MemoryCellLayer;
			TArray<float, TInlineAllocator<512>> RememberGateBuffer;
			TArray<float, TInlineAllocator<512>> MemoryUpdateBuffer;
			TArray<float, TInlineAllocator<512>> PassthroughGateBuffer;
			TArray<float, TInlineAllocator<512>> OutputMemoryUpdateBuffer;
			TArray<float, TInlineAllocator<512>> OutputInputUpdateBuffer;
		};

		//--------------------------------------------------------------------------

		struct FMemoryCellLayer : public ILayer
		{
			virtual TSharedPtr<ILayerInstance> MakeInstance() const { return MakeShared<FMemoryCellInstance>(*this); };
			virtual ELayerType GetLayerType() const override final { return ELayerType::MemoryCell; }
			virtual uint32 GetInputSize() const override final { return InputSize + MemorySize; }
			virtual uint32 GetOutputSize() const override final { return OutputSize + MemorySize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputSize);
				Serialization::Size(InOutOffset, OutputSize);
				Serialization::Size(InOutOffset, MemorySize);
				Serialization::Size(InOutOffset, BiasesWr);
				Serialization::Size(InOutOffset, WeightsWr);
				Serialization::Size(InOutOffset, BiasesWp);
				Serialization::Size(InOutOffset, WeightsWp);
				Serialization::Size(InOutOffset, BiasesWn);
				Serialization::Size(InOutOffset, WeightsWn);
				Serialization::Size(InOutOffset, BiasesWz);
				Serialization::Size(InOutOffset, WeightsWz);
				Serialization::Size(InOutOffset, BiasesWy);
				Serialization::Size(InOutOffset, WeightsWy);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputSize, Data);
				Serialization::Load(InOutOffset, OutputSize, Data);
				Serialization::Load(InOutOffset, MemorySize, Data);
				Serialization::Load(InOutOffset, BiasesWr, Data, MemorySize);
				Serialization::Load(InOutOffset, WeightsWr, Data, (InputSize + MemorySize) * MemorySize);
				Serialization::Load(InOutOffset, BiasesWp, Data, OutputSize);
				Serialization::Load(InOutOffset, WeightsWp, Data, (InputSize + MemorySize) * OutputSize);
				Serialization::Load(InOutOffset, BiasesWn, Data, MemorySize);
				Serialization::Load(InOutOffset, WeightsWn, Data, (InputSize + MemorySize) * MemorySize);
				Serialization::Load(InOutOffset, BiasesWz, Data, OutputSize);
				Serialization::Load(InOutOffset, WeightsWz, Data, (InputSize + MemorySize) * OutputSize);
				Serialization::Load(InOutOffset, BiasesWy, Data, OutputSize);
				Serialization::Load(InOutOffset, WeightsWy, Data, MemorySize * OutputSize);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputSize, Data);
				Serialization::Save(InOutOffset, OutputSize, Data);
				Serialization::Save(InOutOffset, MemorySize, Data);
				Serialization::Save(InOutOffset, BiasesWr, Data);
				Serialization::Save(InOutOffset, WeightsWr, Data);
				Serialization::Save(InOutOffset, BiasesWp, Data);
				Serialization::Save(InOutOffset, WeightsWp, Data);
				Serialization::Save(InOutOffset, BiasesWn, Data);
				Serialization::Save(InOutOffset, WeightsWn, Data);
				Serialization::Save(InOutOffset, BiasesWz, Data);
				Serialization::Save(InOutOffset, WeightsWz, Data);
				Serialization::Save(InOutOffset, BiasesWy, Data);
				Serialization::Save(InOutOffset, WeightsWy, Data);
			}

			virtual void Evaluate(
				TArrayView<float> Output,
				TConstArrayView<float> Input,
				ILayerInstance* Instance,
				uint32 BatchSize,
				uint32 OutputStride,
				uint32 InputStride) override final
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(NNE::RuntimeBasic::Private::FMemoryCellLayer::Evaluate);

				FMemoryCellInstance* MemoryCellInstance = StaticCast<FMemoryCellInstance*>(Instance);
				check(MemoryCellInstance);

				// Remember Gate

				LinearLayer(
					MemoryCellInstance->RememberGateBuffer.GetData(),
					Input.GetData(),
					WeightsWr.GetData(),
					BiasesWr.GetData(),
					BatchSize,
					InputSize + MemorySize,
					MemorySize,
					MemorySize,
					InputStride);

				// Passthrough Gate

				LinearLayer(
					MemoryCellInstance->PassthroughGateBuffer.GetData(),
					Input.GetData(),
					WeightsWp.GetData(),
					BiasesWp.GetData(),
					BatchSize,
					InputSize + MemorySize,
					OutputSize,
					OutputSize,
					InputStride);

				// Memory Update

				LinearLayer(
					MemoryCellInstance->MemoryUpdateBuffer.GetData(),
					Input.GetData(),
					WeightsWn.GetData(),
					BiasesWn.GetData(),
					BatchSize,
					InputSize + MemorySize,
					MemorySize,
					MemorySize,
					InputStride);

				// Update Memory State

				MemoryCellUpdateMemory(
					Output.GetData() + OutputSize,
					MemoryCellInstance->RememberGateBuffer.GetData(),
					Input.GetData() + InputSize,
					MemoryCellInstance->MemoryUpdateBuffer.GetData(),
					BatchSize,
					MemorySize,
					OutputStride,
					MemorySize,
					InputStride,
					MemorySize);

				// Output Input Update

				LinearLayer(
					MemoryCellInstance->OutputInputUpdateBuffer.GetData(),
					Input.GetData(),
					WeightsWz.GetData(),
					BiasesWz.GetData(),
					BatchSize,
					InputSize + MemorySize,
					OutputSize,
					OutputSize,
					InputStride);

				// Output Memory Update

				LinearLayer(
					MemoryCellInstance->OutputMemoryUpdateBuffer.GetData(),
					Output.GetData() + OutputSize,
					WeightsWy.GetData(),
					BiasesWy.GetData(),
					BatchSize,
					MemorySize,
					OutputSize,
					OutputSize,
					OutputStride);

				// Update Final Output

				MemoryCellUpdateOutput(
					Output.GetData(),
					MemoryCellInstance->PassthroughGateBuffer.GetData(),
					MemoryCellInstance->OutputMemoryUpdateBuffer.GetData(),
					MemoryCellInstance->OutputInputUpdateBuffer.GetData(),
					BatchSize,
					OutputSize,
					OutputStride,
					OutputSize,
					OutputSize,
					OutputSize);
			}

			uint32 InputSize = 0;
			uint32 OutputSize = 0;
			uint32 MemorySize = 0;
			TConstArrayView<float> BiasesWr;
			TConstArrayView<float> WeightsWr;
			TConstArrayView<float> BiasesWp;
			TConstArrayView<float> WeightsWp;
			TConstArrayView<float> BiasesWn;
			TConstArrayView<float> WeightsWn;
			TConstArrayView<float> BiasesWz;
			TConstArrayView<float> WeightsWz;
			TConstArrayView<float> BiasesWy;
			TConstArrayView<float> WeightsWy;
		};

		//--------------------------------------------------------------------------

		FMemoryCellInstance::FMemoryCellInstance(const FMemoryCellLayer& InMemoryCellLayer) 
			: MemoryCellLayer(InMemoryCellLayer) {}

		void FMemoryCellInstance::SetBatchSize(const uint32 BatchSize)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(NNE::RuntimeBasic::Private::FMemoryCellInstance::SetBatchSize);

			RememberGateBuffer.SetNumUninitialized(BatchSize * MemoryCellLayer.MemorySize, false);
			MemoryUpdateBuffer.SetNumUninitialized(BatchSize * MemoryCellLayer.MemorySize, false);
			PassthroughGateBuffer.SetNumUninitialized(BatchSize * MemoryCellLayer.OutputSize, false);
			OutputMemoryUpdateBuffer.SetNumUninitialized(BatchSize * MemoryCellLayer.OutputSize, false);
			OutputInputUpdateBuffer.SetNumUninitialized(BatchSize * MemoryCellLayer.OutputSize, false);
		}

		//--------------------------------------------------------------------------

		struct FMemoryBackboneLayer;

		struct FMemoryBackboneInstance : public ILayerInstance
		{
			FMemoryBackboneInstance(const FMemoryBackboneLayer& InMemoryBackboneLayer);

			virtual void SetBatchSize(const uint32 BatchSize) override final;

			const FMemoryBackboneLayer& MemoryBackboneLayer;
			TArray<float, TInlineAllocator<512>> CellInputBuffer;
			TArray<float, TInlineAllocator<512>> CellOutputBuffer;
			TSharedPtr<ILayerInstance> PrefixInstance;
			TSharedPtr<ILayerInstance> CellInstance;
			TSharedPtr<ILayerInstance> PostfixInstance;
		};

		//--------------------------------------------------------------------------

		struct FMemoryBackboneLayer : public ILayer
		{
			virtual TSharedPtr<ILayerInstance> MakeInstance() const { return MakeShared<FMemoryBackboneInstance>(*this); };
			virtual ELayerType GetLayerType() const override final { return ELayerType::MemoryBackbone; }
			virtual uint32 GetInputSize() const override final { return PrefixInputSize + MemorySize; }
			virtual uint32 GetOutputSize() const override final { return PostfixOutputSize + MemorySize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, MemorySize);
				Serialization::Size(InOutOffset, Cell);
				Serialization::Size(InOutOffset, Prefix);
				Serialization::Size(InOutOffset, Postfix);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, MemorySize, Data);
				Serialization::Load(InOutOffset, Cell, Data);
				Serialization::Load(InOutOffset, Prefix, Data);
				Serialization::Load(InOutOffset, Postfix, Data);

				PrefixInputSize = Prefix->GetInputSize();
				PrefixOutputSize = Prefix->GetOutputSize();
				PostfixInputSize = Postfix->GetInputSize();
				PostfixOutputSize = Postfix->GetOutputSize();
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, MemorySize, Data);
				Serialization::Save(InOutOffset, Cell, Data);
				Serialization::Save(InOutOffset, Prefix, Data);
				Serialization::Save(InOutOffset, Postfix, Data);
			}

			virtual void Evaluate(
				TArrayView<float> Output,
				TConstArrayView<float> Input,
				ILayerInstance* Instance,
				uint32 BatchSize,
				uint32 OutputStride,
				uint32 InputStride) override final
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(NNE::RuntimeBasic::Private::FMemoryBackboneLayer::Evaluate);

				FMemoryBackboneInstance* MemoryBackboneInstance = StaticCast<FMemoryBackboneInstance*>(Instance);
				check(MemoryBackboneInstance);

				// Evaluate Prefix

				Prefix->Evaluate(
					MemoryBackboneInstance->CellInputBuffer,
					Input,
					MemoryBackboneInstance->PrefixInstance.Get(),
					BatchSize,
					PrefixOutputSize + MemorySize,
					InputStride);

				// Copy Memory State to Cell Input

				IdentityLayer(
					MemoryBackboneInstance->CellInputBuffer.GetData() + PrefixOutputSize,
					Input.GetData() + PrefixInputSize,
					BatchSize,
					MemorySize,
					PrefixOutputSize + MemorySize,
					InputStride);

				// Evaluate Cell

				Cell->Evaluate(
					MemoryBackboneInstance->CellOutputBuffer,
					MemoryBackboneInstance->CellInputBuffer,
					MemoryBackboneInstance->CellInstance.Get(),
					BatchSize,
					PostfixInputSize + MemorySize,
					PrefixOutputSize + MemorySize);

				// Evaluate Postfix

				Postfix->Evaluate(
					Output,
					MemoryBackboneInstance->CellOutputBuffer,
					MemoryBackboneInstance->PostfixInstance.Get(),
					BatchSize,
					OutputStride,
					PostfixInputSize + MemorySize);

				// Copy Memory State to Output

				IdentityLayer(
					Output.GetData() + PostfixOutputSize,
					MemoryBackboneInstance->CellOutputBuffer.GetData() + PostfixInputSize,
					BatchSize,
					MemorySize,
					OutputStride,
					PostfixInputSize + MemorySize);
			}

			uint32 MemorySize = 0;
			TSharedPtr<ILayer> Cell;
			TSharedPtr<ILayer> Prefix;
			TSharedPtr<ILayer> Postfix;
			uint32 PrefixInputSize = 0;
			uint32 PrefixOutputSize = 0;
			uint32 PostfixInputSize = 0;
			uint32 PostfixOutputSize = 0;
		};

		//--------------------------------------------------------------------------

		FMemoryBackboneInstance::FMemoryBackboneInstance(const FMemoryBackboneLayer& InMemoryBackboneLayer)
			: MemoryBackboneLayer(InMemoryBackboneLayer)
		{
			CellInstance = MemoryBackboneLayer.Cell->MakeInstance();
			PrefixInstance = MemoryBackboneLayer.Prefix->MakeInstance();
			PostfixInstance = MemoryBackboneLayer.Postfix->MakeInstance();
		}

		void FMemoryBackboneInstance::SetBatchSize(const uint32 BatchSize)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(NNE::RuntimeBasic::Private::FMemoryBackboneInstance::SetBatchSize);

			if (CellInstance) { CellInstance->SetBatchSize(BatchSize); }
			if (PrefixInstance) { PrefixInstance->SetBatchSize(BatchSize); }
			if (PostfixInstance) { PostfixInstance->SetBatchSize(BatchSize); }

			CellInputBuffer.SetNumUninitialized(BatchSize * (MemoryBackboneLayer.PrefixOutputSize + MemoryBackboneLayer.MemorySize), false);
			CellOutputBuffer.SetNumUninitialized(BatchSize * (MemoryBackboneLayer.PostfixInputSize + MemoryBackboneLayer.MemorySize), false);
		}

		//--------------------------------------------------------------------------

		namespace Serialization
		{
			static inline void Size(uint64& InOutOffset, const TSharedPtr<ILayer>& InLayer)
			{
				Serialization::Size(InOutOffset, (uint32)InLayer->GetLayerType());
				InLayer->SerializationSize(InOutOffset);
			}

			static inline void Load(uint64& InOutOffset, TSharedPtr<ILayer>& OutLayer, TConstArrayView<uint8> Data)
			{
				uint32 LayerTypeId = (uint32)ELayerType::Invalid;
				Serialization::Load(InOutOffset, LayerTypeId, Data);

				checkf((ELayerType)LayerTypeId != ELayerType::Invalid, TEXT("Invalid Layer"));

				if (OutLayer == nullptr || OutLayer->GetLayerType() != (ELayerType)LayerTypeId)
				{
					switch ((ELayerType)LayerTypeId)
					{
					case ELayerType::Sequence: OutLayer = MakeShared<FSequenceLayer>(); break;
					case ELayerType::Normalize: OutLayer = MakeShared<FNormalizeLayer>(); break;
					case ELayerType::Denormalize: OutLayer = MakeShared<FDenormalizeLayer>(); break;
					case ELayerType::Linear: OutLayer = MakeShared<FLinearLayer>(); break;
					case ELayerType::CompressedLinear: OutLayer = MakeShared<FCompressedLinearLayer>(); break;
					case ELayerType::MultiLinear: OutLayer = MakeShared<FMultiLinearLayer>(); break;
					case ELayerType::ReLU: OutLayer = MakeShared<FReLULayer>(); break;
					case ELayerType::ELU: OutLayer = MakeShared<FELULayer>(); break;
					case ELayerType::TanH: OutLayer = MakeShared<FTanHLayer>(); break;
					case ELayerType::PReLU: OutLayer = MakeShared<FPReLULayer>(); break;
					case ELayerType::MemoryCell: OutLayer = MakeShared<FMemoryCellLayer>(); break;
					case ELayerType::MemoryBackbone: OutLayer = MakeShared<FMemoryBackboneLayer>(); break;
					default: checkf(false, TEXT("Unknown Layer Id %i"), LayerTypeId);
					}
				}

				OutLayer->SerializationLoad(InOutOffset, Data);
			}

			static inline void Save(uint64& InOutOffset, const TSharedPtr<ILayer>& InLayer, TArrayView<uint8> Data)
			{
				Serialization::Save(InOutOffset, (uint32)InLayer->GetLayerType(), Data);
				InLayer->SerializationSave(InOutOffset, Data);
			}
		}
	}

	//--------------------------------------------------------------------------

	FModelInstanceCPU::FModelInstanceCPU(const TSharedPtr<FModelCPU>& InModel)
		: Model(InModel)
		, InputTensorDesc(FTensorDesc::Make(TEXT("Input"), FSymbolicTensorShape::Make({ -1, -1 }), ENNETensorDataType::Float))
		, OutputTensorDesc(FTensorDesc::Make(TEXT("Output"), FSymbolicTensorShape::Make({ -1, -1 }), ENNETensorDataType::Float))
		, Instance(Model->Layer->MakeInstance())
	{}

	int FModelInstanceCPU::SetInputTensorShapes(TConstArrayView<FTensorShape> InInputShapes)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(NNE::RuntimeBasic::FModelInstanceCPU::SetInputTensorShapes);

		if (!ensureMsgf(InInputShapes.Num() == 1, TEXT("Basic CPU Inference only supports single input tensor.")))
		{
			return -1;
		}

		const FTensorShape& InputShape = InInputShapes[0];

		if (!ensureMsgf(InputShape.Rank() == 2, TEXT("Basic CPU Inference only supports rank 2 input tensors.")))
		{
			return -1;
		}

		const uint32 InputBatchSize = InputShape.GetData()[0];
		const uint32 InputInputSize = InputShape.GetData()[1];
		const uint32 ModelInputSize = Model->Layer->GetInputSize();
		const uint32 ModelOutputSize = Model->Layer->GetOutputSize();

		if (!ensureMsgf(InputInputSize == ModelInputSize, TEXT("Input tensor shape does not match model input size. Got %i, expected %i."), InputInputSize, ModelInputSize))
		{
			return -1;
		}

		BatchSize = InputBatchSize;
		InputSize = ModelInputSize;
		OutputSize = ModelOutputSize;

		InputTensorShape = FTensorShape::Make({ BatchSize, InputSize });
		OutputTensorShape = FTensorShape::Make({ BatchSize, OutputSize });

		if (Instance)
		{
			Instance->SetBatchSize(BatchSize);
		}

		return 0;
	}

	int FModelInstanceCPU::RunSync(TConstArrayView<FTensorBindingCPU> InInputBindings, TConstArrayView<FTensorBindingCPU> InOutputBindings)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(NNE::RuntimeBasic::FModelInstanceCPU::RunSync);

		if (!ensureMsgf(BatchSize > 0, TEXT("SetInputTensorShapes must be run before RunSync")))
		{
			return -1;
		}

		if (!ensureMsgf(InInputBindings.Num() == 1, TEXT("Basic CPU Inference only supports single input tensor.")))
		{
			return -1;
		}

		if (!ensureMsgf(InOutputBindings.Num() == 1, TEXT("Basic CPU Inference only supports single output tensor.")))
		{
			return -1;
		}

		if (!ensureMsgf(InInputBindings[0].SizeInBytes == BatchSize * InputSize * sizeof(float), TEXT("Incorrect Input Tensor Size")))
		{
			return -1;
		}

		if (!ensureMsgf(InOutputBindings[0].SizeInBytes == BatchSize * OutputSize * sizeof(float), TEXT("Incorrect Output Tensor Size")))
		{
			return -1;
		}

		Model->Layer->Evaluate(
			TArrayView<float>((float*)InOutputBindings[0].Data, BatchSize * OutputSize),
			TConstArrayView<float>((const float*)InInputBindings[0].Data, BatchSize * InputSize),
			Instance.Get(),
			BatchSize,
			OutputSize,
			InputSize);

		return 0;
	}


	//--------------------------------------------------------------------------

	uint32 FModelCPU::ModelMagicNumber = 0x0BA51C01;
	uint32 FModelCPU::ModelVersionNumber = 1;

	TSharedPtr<IModelInstanceCPU> FModelCPU::CreateModelInstanceCPU()
	{
		return MakeShared<FModelInstanceCPU>(WeakThis.Pin());
	}

	void FModelCPU::SerializationSize(uint64& InOutOffset) const
	{
		checkf(InOutOffset % 64 == 0,
			TEXT("Model must be aligned to 64 bytes because there must be no padding before magic number."));

		Private::Serialization::Size(InOutOffset, ModelMagicNumber);
		Private::Serialization::Size(InOutOffset, ModelVersionNumber);
		Private::Serialization::Size(InOutOffset, Layer);
	}

	bool FModelCPU::SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(NNE::RuntimeBasic::FModelCPU::SerializationLoad);

		checkf(InOutOffset % 64 == 0, 
			TEXT("Model must be aligned to 64 bytes because there must be no padding before magic number."));

		uint32 Magic = INDEX_NONE;
		Private::Serialization::Load(InOutOffset, Magic, Data);
		if (Magic != ModelMagicNumber)
		{
			UE_LOG(LogNNE, Error, TEXT("Invalid Magic Number %i"), Magic);
			return false;
		}

		uint32 Version = INDEX_NONE;
		Private::Serialization::Load(InOutOffset, Version, Data);
		if (Version != ModelVersionNumber)
		{
			UE_LOG(LogNNE, Error, TEXT("Unsupported Version Number %i"), Version);
			return false;
		}

		Private::Serialization::Load(InOutOffset, Layer, Data);

		return true;
	}

	void FModelCPU::SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(NNE::RuntimeBasic::FModelCPU::SerializationSave);

		checkf(InOutOffset % 64 == 0,
			TEXT("Model must be aligned to 64 bytes because there must be no padding before magic number."));

		Private::Serialization::Save(InOutOffset, ModelMagicNumber, Data);
		Private::Serialization::Save(InOutOffset, ModelVersionNumber, Data);
		Private::Serialization::Save(InOutOffset, Layer, Data);
	}

	//--------------------------------------------------------------------------

	FSequentialModelBuilder::FSequentialModelBuilder()
	{
		Model = MakeShared<FModelCPU>();
		Model->Layer = MakeShared<Private::FSequenceLayer>();
	}

	void FSequentialModelBuilder::AddLinear(uint32 InputSize, uint32 OutputSize, TConstArrayView<float> Weights, TConstArrayView<float> Biases)
	{
		Private::FSequenceLayer* Sequence = StaticCast<Private::FSequenceLayer*>(Model->Layer.Get());

		if (Sequence->Layers.Num() > 0)
		{
			check(Sequence->Layers.Last()->GetOutputSize() == InputSize);
		}

		check(Weights.Num() == InputSize * OutputSize);
		check(Biases.Num() == OutputSize);

		const TSharedPtr<Private::FLinearLayer> LinearLayer = MakeShared<Private::FLinearLayer>();
		LinearLayer->InputSize = InputSize;
		LinearLayer->OutputSize = OutputSize;
		LinearLayer->Biases = Biases;
		LinearLayer->Weights = Weights;

		Sequence->Layers.Emplace(LinearLayer);
	}

	void FSequentialModelBuilder::AddMultiLinear(uint32 InputSize, uint32 OutputSize, uint32 BlockNum, TConstArrayView<float> Weights, TConstArrayView<float> Biases)
	{
		Private::FSequenceLayer* Sequence = StaticCast<Private::FSequenceLayer*>(Model->Layer.Get());

		if (Sequence->Layers.Num() > 0)
		{
			check(Sequence->Layers.Last()->GetOutputSize() == InputSize * BlockNum);
		}

		check(Weights.Num() == InputSize * OutputSize * BlockNum);
		check(Biases.Num() == OutputSize * BlockNum);

		const TSharedPtr<Private::FMultiLinearLayer> MultiLinearLayer = MakeShared<Private::FMultiLinearLayer>();
		MultiLinearLayer->InputSize = InputSize;
		MultiLinearLayer->OutputSize = OutputSize;
		MultiLinearLayer->BlockNum = BlockNum;
		MultiLinearLayer->Biases = Biases;
		MultiLinearLayer->Weights = Weights;

		Sequence->Layers.Emplace(MultiLinearLayer);
	}

	void FSequentialModelBuilder::AddReLU()
	{
		Private::FSequenceLayer* Sequence = StaticCast<Private::FSequenceLayer*>(Model->Layer.Get());

		checkf(Sequence->Layers.Num() > 0, TEXT("Cannot add activation on initial layer because size is unknown"));

		const TSharedPtr<Private::FReLULayer> ActivationLayer = MakeShared<Private::FReLULayer>();
		ActivationLayer->InputOutputSize = Sequence->Layers.Last()->GetOutputSize();
		Sequence->Layers.Emplace(ActivationLayer);
	}

	void FSequentialModelBuilder::AddELU()
	{
		Private::FSequenceLayer* Sequence = StaticCast<Private::FSequenceLayer*>(Model->Layer.Get());

		checkf(Sequence->Layers.Num() > 0, TEXT("Cannot add activation on initial layer because size is unknown"));

		const TSharedPtr<Private::FELULayer> ActivationLayer = MakeShared<Private::FELULayer>();
		ActivationLayer->InputOutputSize = Sequence->Layers.Last()->GetOutputSize();
		Sequence->Layers.Emplace(ActivationLayer);
	}

	void FSequentialModelBuilder::AddTanH()
	{
		Private::FSequenceLayer* Sequence = StaticCast<Private::FSequenceLayer*>(Model->Layer.Get());

		checkf(Sequence->Layers.Num() > 0, TEXT("Cannot add activation on initial layer because size is unknown"));

		const TSharedPtr<Private::FTanHLayer> ActivationLayer = MakeShared<Private::FTanHLayer>();
		ActivationLayer->InputOutputSize = Sequence->Layers.Last()->GetOutputSize();
		Sequence->Layers.Emplace(ActivationLayer);
	}

	void FSequentialModelBuilder::AddPReLU(TConstArrayView<float> Alpha)
	{
		Private::FSequenceLayer* Sequence = StaticCast<Private::FSequenceLayer*>(Model->Layer.Get());

		checkf(Sequence->Layers.Num() > 0, TEXT("Cannot add activation on initial layer because size is unknown"));
		check(Alpha.Num() == Sequence->Layers.Last()->GetOutputSize());

		const TSharedPtr<Private::FPReLULayer> ActivationLayer = MakeShared<Private::FPReLULayer>();
		ActivationLayer->InputOutputSize = Sequence->Layers.Last()->GetOutputSize();
		ActivationLayer->Alpha = Alpha;

		Sequence->Layers.Emplace(ActivationLayer);
	}

	uint64 FSequentialModelBuilder::GetWriteByteNum() const
	{
		uint64 Offset = 0;
		Model->SerializationSize(Offset);
		return Offset;
	}

	void FSequentialModelBuilder::WriteAndReset(TArrayView<uint8> OutBytes)
	{
		check((uint64)OutBytes.Num() >= GetWriteByteNum());

		// Zero to ensure any padding due to alignment is always zero
		FMemory::Memzero(OutBytes.GetData(), OutBytes.Num());

		uint64 Offset = 0;
		Model->SerializationSave(Offset, OutBytes);

		Private::FSequenceLayer* Sequence = StaticCast<Private::FSequenceLayer*>(Model->Layer.Get());
		Sequence->Layers.Empty();
	}

	//--------------------------------------------------------------------------

	FMemoryBackboneModelBuilder::FMemoryBackboneModelBuilder()
	{
		Model = MakeShared<FModelCPU>();
	}

	void FMemoryBackboneModelBuilder::BuildEmptyModel(
		const uint32 InputNum,
		const uint32 OutputNum,
		const uint32 MemoryNum,
		const uint32 HiddenUnitNum,
		const uint32 PrefixLayerNum,
		const uint32 PostfixLayerNum)
	{
		// Allocate enough zeros for every weight matrix used

		const uint32 MaxZeroNumPrefix = FMath::Max(InputNum * HiddenUnitNum, HiddenUnitNum * HiddenUnitNum);
		const uint32 MaxZeroNumPostfix = FMath::Max(HiddenUnitNum * HiddenUnitNum, OutputNum * HiddenUnitNum);
		const uint32 MaxZeroNumCell = FMath::Max((HiddenUnitNum + MemoryNum) * MemoryNum, (HiddenUnitNum + MemoryNum) * HiddenUnitNum);

		ZerosData.Init(0.0f, FMath::Max(FMath::Max(MaxZeroNumPrefix, MaxZeroNumPostfix), MaxZeroNumCell));
		TConstArrayView<float> ZerosView = ZerosData;

		// Create Cell Layer

		TSharedRef<Private::FMemoryCellLayer> CellLayer = MakeShared<Private::FMemoryCellLayer>();
		CellLayer->InputSize = HiddenUnitNum;
		CellLayer->MemorySize = MemoryNum;
		CellLayer->OutputSize = HiddenUnitNum;
		CellLayer->BiasesWr = ZerosView.Slice(0, MemoryNum);
		CellLayer->WeightsWr = ZerosView.Slice(0, (HiddenUnitNum + MemoryNum) * MemoryNum);
		CellLayer->BiasesWp = ZerosView.Slice(0, HiddenUnitNum);
		CellLayer->WeightsWp = ZerosView.Slice(0, (HiddenUnitNum + MemoryNum) * HiddenUnitNum);
		CellLayer->BiasesWn = ZerosView.Slice(0, MemoryNum);
		CellLayer->WeightsWn = ZerosView.Slice(0, (HiddenUnitNum + MemoryNum) * MemoryNum);
		CellLayer->BiasesWz = ZerosView.Slice(0, HiddenUnitNum);
		CellLayer->WeightsWz = ZerosView.Slice(0, (HiddenUnitNum + MemoryNum) * HiddenUnitNum);
		CellLayer->BiasesWy = ZerosView.Slice(0, HiddenUnitNum);
		CellLayer->WeightsWy = ZerosView.Slice(0, MemoryNum * HiddenUnitNum);

		// Create Prefix Network

		check(PrefixLayerNum >= 1);

		TSharedRef<Private::FSequenceLayer> PrefixLayer = MakeShared<Private::FSequenceLayer>();
		PrefixLayer->Layers.Reserve(PrefixLayerNum * 2);

		for (uint32 LayerIdx = 0; LayerIdx < PrefixLayerNum; LayerIdx++)
		{
			const TSharedPtr<Private::FLinearLayer> LinearLayer = MakeShared<Private::FLinearLayer>();
			LinearLayer->InputSize = LayerIdx == 0 ? InputNum : HiddenUnitNum;
			LinearLayer->OutputSize = HiddenUnitNum;
			LinearLayer->Biases = ZerosView.Slice(0, LinearLayer->OutputSize);
			LinearLayer->Weights = ZerosView.Slice(0, LinearLayer->InputSize * LinearLayer->OutputSize);

			PrefixLayer->Layers.Emplace(LinearLayer);

			const TSharedPtr<Private::FELULayer> ActivationLayer = MakeShared<Private::FELULayer>();
			ActivationLayer->InputOutputSize = LinearLayer->OutputSize;

			PrefixLayer->Layers.Emplace(ActivationLayer);
		}

		// Create Postfix Network

		check(PostfixLayerNum >= 1);

		TSharedRef<Private::FSequenceLayer> PostfixLayer = MakeShared<Private::FSequenceLayer>();
		PostfixLayer->Layers.Reserve(PostfixLayerNum * 2 - 1);

		for (uint32 LayerIdx = 0; LayerIdx < PostfixLayerNum; LayerIdx++)
		{
			const TSharedPtr<Private::FLinearLayer> LinearLayer = MakeShared<Private::FLinearLayer>();
			LinearLayer->InputSize = HiddenUnitNum;
			LinearLayer->OutputSize = LayerIdx + 1 == PostfixLayerNum ? OutputNum : HiddenUnitNum;
			LinearLayer->Biases = ZerosView.Slice(0, LinearLayer->OutputSize);
			LinearLayer->Weights = ZerosView.Slice(0, LinearLayer->InputSize * LinearLayer->OutputSize);

			PostfixLayer->Layers.Emplace(LinearLayer);

			if (LayerIdx + 1 != PostfixLayerNum)
			{
				const TSharedPtr<Private::FELULayer> ActivationLayer = MakeShared<Private::FELULayer>();
				ActivationLayer->InputOutputSize = LinearLayer->OutputSize;

				PostfixLayer->Layers.Emplace(ActivationLayer);
			}
		}

		// Create Backbone

		TSharedRef<Private::FMemoryBackboneLayer> BackboneLayer = MakeShared<Private::FMemoryBackboneLayer>();
		BackboneLayer->MemorySize = MemoryNum;
		BackboneLayer->Cell = CellLayer;
		BackboneLayer->Prefix = PrefixLayer;
		BackboneLayer->Postfix = PostfixLayer;

		Model->Layer = BackboneLayer;
	}

	uint64 FMemoryBackboneModelBuilder::GetWriteByteNum() const
	{
		checkf(Model->Layer, TEXT("Model not built."));
		uint64 Offset = 0;
		Model->SerializationSize(Offset);
		return Offset;
	}

	void FMemoryBackboneModelBuilder::WriteAndReset(TArrayView<uint8> OutBytes)
	{
		checkf(Model->Layer, TEXT("Model not built."));
		check((uint64)OutBytes.Num() >= GetWriteByteNum());

		// Zero to ensure any padding due to alignment is always zero
		FMemory::Memzero(OutBytes.GetData(), OutBytes.Num());

		uint64 Offset = 0;
		Model->SerializationSave(Offset, OutBytes);

		Model->Layer.Reset();
		ZerosData.Empty();
	}

	//--------------------------------------------------------------------------

} // namespace UE::NNE::RuntimeBasic

#undef NNE_RUNTIME_BASIC_ENABLE_ISPC
