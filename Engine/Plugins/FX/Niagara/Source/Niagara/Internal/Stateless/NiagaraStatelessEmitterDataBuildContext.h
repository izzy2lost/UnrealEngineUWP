// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStatelessCommon.h"
#include "NiagaraStatelessDistribution.h"

struct FNiagaraParameterBinding;
struct FNiagaraParameterBindingWithValue;
struct FNiagaraParameterStore;

class FNiagaraStatelessEmitterDataBuildContext
{
public:
	UE_NONCOPYABLE(FNiagaraStatelessEmitterDataBuildContext);

	FNiagaraStatelessEmitterDataBuildContext(FNiagaraParameterStore& InRendererBindings, TArray<uint8>& InBuiltData, TArray<float>& InStaticFloatData)
		: RendererBindings(InRendererBindings)
		, BuiltData(InBuiltData)
		, StaticFloatData(InStaticFloatData)
	{
	}

	uint32 AddStaticData(TConstArrayView<float> FloatData);
	uint32 AddStaticData(TConstArrayView<FVector2f> FloatData);
	uint32 AddStaticData(TConstArrayView<FVector3f> FloatData);
	uint32 AddStaticData(TConstArrayView<FVector4f> FloatData);
	uint32 AddStaticData(TConstArrayView<FLinearColor> FloatData);

	template<typename T>
	T* AllocateBuiltData()
	{
		static_assert(TIsTrivial<T>::Value, "Only trivial types can be used for built data");

		const int32 Align = BuiltData.Num() % alignof(T);
		const int32 Offset = BuiltData.AddZeroed(sizeof(T) + Align);
		void* NewData = BuiltData.GetData() + Offset + Align;
		return new(NewData) T();
	}

	template<typename T>
	T& GetTransientBuildData()
	{
		TUniquePtr<FTransientObject>& TransientObj = TransientBuildData.FindOrAdd(T::GetName());
		if (TransientObj.IsValid() == false)
		{
			TransientObj.Reset(new TTransientObject<T>);
		}
		return *reinterpret_cast<T*>(TransientObj->GetObject());
	}

	int32 AddRendererBinding(const FNiagaraParameterBinding& Binding);
	int32 AddRendererBinding(const FNiagaraParameterBindingWithValue& Binding);	

	// Adds an distribution into the LUT if enabled
	template<typename TType>
	FUintVector3 AddDistribution(ENiagaraDistributionMode Mode, TConstArrayView<TType> Values, bool bEnabled)
	{
		FUintVector3 Parameters = FUintVector3::ZeroValue;
		if (bEnabled && Values.Num() > 0)
		{
			constexpr uint32 StatelessDistributionFlag_Random = 0x00000001;
			constexpr uint32 StatelessDistributionFlag_Uniform = 0x00000002;

			switch (Mode)
			{
				case ENiagaraDistributionMode::UniformConstant:		Parameters.X = StatelessDistributionFlag_Random | StatelessDistributionFlag_Uniform; break;
				case ENiagaraDistributionMode::NonUniformConstant:	Parameters.X = StatelessDistributionFlag_Random; break;
				case ENiagaraDistributionMode::UniformRange:		Parameters.X = StatelessDistributionFlag_Random | StatelessDistributionFlag_Uniform; break;
				case ENiagaraDistributionMode::NonUniformRange:		Parameters.X = StatelessDistributionFlag_Random; break;
				case ENiagaraDistributionMode::UniformCurve:		Parameters.X = StatelessDistributionFlag_Uniform; break;
				case ENiagaraDistributionMode::NonUniformCurve:		Parameters.X = 0; break;
				default:											checkNoEntry(); break;
			}

			Parameters.Y = AddStaticData(Values);
			reinterpret_cast<float&>(Parameters.Z) = Values.Num() - 1;
		}
		return Parameters;
	}

	// Adds a distribution into the LUT if enabled and returns the packed information to send to the shader
	template<typename TDistribution>
	FUintVector3 AddDistribution(const TDistribution& Distribution, bool bEnabled)
	{
		return AddDistribution(Distribution.Mode, MakeArrayView(Distribution.Values), bEnabled);
	}

private:
	FNiagaraParameterStore& RendererBindings;
	TArray<uint8>&			BuiltData;
	TArray<float>&			StaticFloatData;

	struct FTransientObject
	{
		virtual ~FTransientObject() = default;
		virtual void* GetObject() = 0;
	};

	template <typename T>
	struct TTransientObject final : FTransientObject
	{
		template <typename... TArgs>
		FORCEINLINE TTransientObject(TArgs&&... Args) : TheObject(Forward<TArgs&&>(Args)...) {}
		virtual void* GetObject() { return &TheObject; }

		T TheObject;
	};

	TMap<FName, TUniquePtr<FTransientObject>>	TransientBuildData;
};
