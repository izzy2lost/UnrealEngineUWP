// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"

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
		const int32 Align = BuiltData.Num() % alignof(T);
		const int32 Offset = BuiltData.AddZeroed(sizeof(T) + Align);
		return reinterpret_cast<T*>(BuiltData.GetData() + Offset + Align);
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
	//NiagaraStateless::FPhysicsBuildData& PhysicsBuildData = BuildContext.GetTransientBuildData<NiagaraStateless::FPhysicsBuildData>();

	int32 AddRendererBinding(const FNiagaraParameterBinding& Binding);
	int32 AddRendererBinding(const FNiagaraParameterBindingWithValue& Binding);	

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
