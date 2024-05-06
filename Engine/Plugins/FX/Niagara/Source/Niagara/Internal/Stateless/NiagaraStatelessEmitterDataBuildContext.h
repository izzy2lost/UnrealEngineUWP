// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStatelessCommon.h"
#include "NiagaraStatelessDistribution.h"

struct FNiagaraDataSetCompiledData;
struct FNiagaraParameterBinding;
struct FNiagaraParameterBindingWithValue;
struct FNiagaraParameterStore;
namespace NiagaraStateless
{
	class FParticleSimulationContext;
	class FParticleSimulationExecData;
}

class FNiagaraStatelessEmitterDataBuildContext
{
	static constexpr uint32 StatelessDistributionFlag_Random	= 0x00000001;
	static constexpr uint32 StatelessDistributionFlag_Uniform	= 0x00000002;
	static constexpr uint32 StatelessDistributionFlag_Binding	= 0x00000004;

public:
	UE_NONCOPYABLE(FNiagaraStatelessEmitterDataBuildContext);

	FNiagaraStatelessEmitterDataBuildContext(FNiagaraDataSetCompiledData& InParticleDataSet, FNiagaraParameterStore& InRendererBindings, TArray<uint8>& InBuiltData, TArray<float>& InStaticFloatData, NiagaraStateless::FParticleSimulationExecData* InParticleExecData)
		: ParticleDataSet(InParticleDataSet)
		, RendererBindings(InRendererBindings)
		, BuiltData(InBuiltData)
		, StaticFloatData(InStaticFloatData)
		, ParticleExecData(InParticleExecData)
	{
	}

	void PreModuleBuild();

	uint32 AddStaticData(TConstArrayView<float> FloatData) const;
	uint32 AddStaticData(TConstArrayView<FVector2f> FloatData) const;
	uint32 AddStaticData(TConstArrayView<FVector3f> FloatData) const;
	uint32 AddStaticData(TConstArrayView<FVector4f> FloatData) const;
	uint32 AddStaticData(TConstArrayView<FLinearColor> FloatData) const;

	template<typename T>
	T* AllocateBuiltData() const
	{
		static_assert(TIsTrivial<T>::Value, "Only trivial types can be used for built data");

		const int32 Offset = Align(BuiltData.Num(), alignof(T));
		BuiltData.AddZeroed(Offset + sizeof(T) - BuiltData.Num());
		void* NewData = BuiltData.GetData() + Offset;
		return new(NewData) T();
	}

	template<typename T>
	T& GetTransientBuildData() const
	{
		TUniquePtr<FTransientObject>& TransientObj = TransientBuildData.FindOrAdd(T::GetName());
		if (TransientObj.IsValid() == false)
		{
			TransientObj.Reset(new TTransientObject<T>);
		}
		return *reinterpret_cast<T*>(TransientObj->GetObject());
	}

	// Adds a binding to the renderer parameter store
	// This allows you to read the parameter data inside the simulation process
	// The returned value is INDEX_NONE is the variables is index otherwise the offset in DWORDs
	int32 AddRendererBinding(const FNiagaraVariableBase& Variable) const;
	int32 AddRendererBinding(const FNiagaraParameterBinding& Binding) const;
	int32 AddRendererBinding(const FNiagaraParameterBindingWithValue& Binding) const;

	// Adds an distribution into the LUT if enabled
	template<typename TType>
	FUintVector3 AddDistribution(ENiagaraDistributionMode Mode, TConstArrayView<TType> Values, bool bEnabled) const
	{
		using namespace NiagaraStateless;

		FUintVector3 Parameters = FUintVector3::ZeroValue;
		if (bEnabled && Values.Num() > 0)
		{
			switch (Mode)
			{
				case ENiagaraDistributionMode::Binding:				checkNoEntry(); break;
				case ENiagaraDistributionMode::UniformConstant:		Parameters.X = uint32(ENiagaraStatelessBuiltDistributionFlag::Random | ENiagaraStatelessBuiltDistributionFlag::Uniform); break;
				case ENiagaraDistributionMode::NonUniformConstant:	Parameters.X = uint32(ENiagaraStatelessBuiltDistributionFlag::Random); break;
				case ENiagaraDistributionMode::UniformRange:		Parameters.X = uint32(ENiagaraStatelessBuiltDistributionFlag::Random | ENiagaraStatelessBuiltDistributionFlag::Uniform); break;
				case ENiagaraDistributionMode::NonUniformRange:		Parameters.X = uint32(ENiagaraStatelessBuiltDistributionFlag::Random); break;
				case ENiagaraDistributionMode::UniformCurve:		Parameters.X = uint32(ENiagaraStatelessBuiltDistributionFlag::Uniform); break;
				case ENiagaraDistributionMode::NonUniformCurve:		Parameters.X = 0; break;
				default:											checkNoEntry(); break;
			}

			Parameters.Y = AddStaticData(Values);
			Parameters.Z = Values.Num() - 1;
		}
		return Parameters;
	}

	// Adds a distribution into the LUT if enabled and returns the packed information to send to the shader
	template<typename TDistribution>
	FUintVector3 AddDistribution(const TDistribution& Distribution, bool bEnabled = true) const
	{
		FUintVector3 Parameters = FUintVector3::ZeroValue;
		if ( bEnabled )
		{
			if (Distribution.Mode == ENiagaraDistributionMode::Binding)
			{
				const int32 ParameterOffset = AddRendererBinding(Distribution.ParameterBinding);
				if (ParameterOffset >= 0)
				{
					Parameters.X = StatelessDistributionFlag_Binding;
					Parameters.Y = ParameterOffset;
					Parameters.Z = 1.0f;
				}
			}
			else
			{
				Parameters = AddDistribution(Distribution.Mode, MakeArrayView(Distribution.Values), bEnabled);
			}
		}
		return Parameters;
	}

	template<typename TRange, typename TDistribution, typename TDefaultValue>
	TRange ConvertDistributionToRangeHelper(const TDistribution& Distribution, const TDefaultValue& DefaultValue) const
	{
		TRange Range(DefaultValue);
		if (Distribution.Mode == ENiagaraDistributionMode::Binding)
		{
			Range.ParameterOffset = AddRendererBinding(Distribution.ParameterBinding);
		}
		else
		{
			Range = Distribution.CalculateRange(DefaultValue);
		}
		return Range;
	}

	FNiagaraStatelessRangeFloat   ConvertDistributionToRange(const FNiagaraDistributionFloat& Distribution, float DefaultValue) const { return ConvertDistributionToRangeHelper<FNiagaraStatelessRangeFloat>(Distribution, DefaultValue); }
	FNiagaraStatelessRangeVector2 ConvertDistributionToRange(const FNiagaraDistributionVector2& Distribution, const FVector2f& DefaultValue) const { return ConvertDistributionToRangeHelper<FNiagaraStatelessRangeVector2>(Distribution, DefaultValue); }
	FNiagaraStatelessRangeVector3 ConvertDistributionToRange(const FNiagaraDistributionVector3& Distribution, const FVector3f& DefaultValue) const { return ConvertDistributionToRangeHelper<FNiagaraStatelessRangeVector3>(Distribution, DefaultValue); }
	FNiagaraStatelessRangeColor   ConvertDistributionToRange(const FNiagaraDistributionColor& Distribution, const FLinearColor& DefaultValue) const { return ConvertDistributionToRangeHelper<FNiagaraStatelessRangeColor>(Distribution, DefaultValue); }

	FNiagaraStatelessRangeFloat   ConvertDistributionToRange(const FNiagaraDistributionRangeFloat& Distribution, float DefaultValue) const { return ConvertDistributionToRangeHelper<FNiagaraStatelessRangeFloat>(Distribution, DefaultValue); }
	FNiagaraStatelessRangeVector2 ConvertDistributionToRange(const FNiagaraDistributionRangeVector2& Distribution, const FVector2f& DefaultValue) const { return ConvertDistributionToRangeHelper<FNiagaraStatelessRangeVector2>(Distribution, DefaultValue); }
	FNiagaraStatelessRangeVector3 ConvertDistributionToRange(const FNiagaraDistributionRangeVector3& Distribution, const FVector3f& DefaultValue) const { return ConvertDistributionToRangeHelper<FNiagaraStatelessRangeVector3>(Distribution, DefaultValue); }
	FNiagaraStatelessRangeColor   ConvertDistributionToRange(const FNiagaraDistributionRangeColor& Distribution, const FLinearColor& DefaultValue) const { return ConvertDistributionToRangeHelper<FNiagaraStatelessRangeColor>(Distribution, DefaultValue); }
	FNiagaraStatelessRangeInt     ConvertDistributionToRange(const FNiagaraDistributionRangeInt& Distribution, int32 DefaultValue) const { return ConvertDistributionToRangeHelper<FNiagaraStatelessRangeInt>(Distribution, DefaultValue); }

	void AddParticleSimulationExecSimulate(TFunction<void(const NiagaraStateless::FParticleSimulationContext&)> Func) const;

	int32 FindParticleVariableIndex(const FNiagaraVariableBase& Variable) const;

private:
	FNiagaraDataSetCompiledData&					ParticleDataSet;
	FNiagaraParameterStore&							RendererBindings;
	TArray<uint8>&									BuiltData;
	TArray<float>&									StaticFloatData;
	NiagaraStateless::FParticleSimulationExecData*	ParticleExecData = nullptr;

	int32											ModuleBuiltDataOffset = 0;

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

	mutable TMap<FName, TUniquePtr<FTransientObject>>	TransientBuildData;
};
