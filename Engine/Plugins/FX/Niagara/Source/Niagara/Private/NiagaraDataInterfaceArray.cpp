// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceArray.h"
#include "NiagaraCompileHashVisitor.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraSystemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraDataInterfaceArray)

#define LOCTEXT_NAMESPACE "UNiagaraDataInterfaceArray"

UNiagaraDataInterfaceArray::UNiagaraDataInterfaceArray(FObjectInitializer const& ObjectInitializer)
{
}

void UNiagaraDataInterfaceArray::PostInitProperties()
{
	Super::PostInitProperties();
	
	if (HasAnyFlags(RF_ClassDefaultObject) && (GetClass() != UNiagaraDataInterfaceArray::StaticClass()))
	{
		ENiagaraTypeRegistryFlags Flags = ENiagaraTypeRegistryFlags::AllowAnyVariable | ENiagaraTypeRegistryFlags::AllowParameter;
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
	}

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		MarkRenderDataDirty();
	}
}

void UNiagaraDataInterfaceArray::PostLoad()
{
	Super::PostLoad();
	MarkRenderDataDirty();
}

#if WITH_EDITOR
void UNiagaraDataInterfaceArray::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

bool UNiagaraDataInterfaceArray::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}
	UNiagaraDataInterfaceArray* OtherTyped = CastChecked<UNiagaraDataInterfaceArray>(Destination);
	OtherTyped->MaxElements = MaxElements;
	OtherTyped->GpuSyncMode = GpuSyncMode;
	return GetProxyAs<INDIArrayProxyBase>()->CopyToInternal(OtherTyped->GetProxyAs<INDIArrayProxyBase>());
}

bool UNiagaraDataInterfaceArray::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}

	const UNiagaraDataInterfaceArray* OtherTyped = CastChecked<UNiagaraDataInterfaceArray>(Other);
	if ((OtherTyped->MaxElements != MaxElements) ||
		(OtherTyped->GpuSyncMode != GpuSyncMode))
	{
		return false;
	}

	return GetProxyAs<INDIArrayProxyBase>()->Equals(OtherTyped->GetProxyAs<INDIArrayProxyBase>());
}

#if WITH_EDITORONLY_DATA
bool UNiagaraDataInterfaceArray::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	bool bSuccess = Super::AppendCompileHash(InVisitor);
	bSuccess &= InVisitor->UpdateShaderParameters<INDIArrayProxyBase::FShaderParameters>();
	bSuccess &= GetProxyAs<INDIArrayProxyBase>()->AppendCompileHash(InVisitor);
	return bSuccess;
}
#endif

void UNiagaraDataInterfaceArray::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
	ShaderParametersBuilder.AddNestedStruct<INDIArrayProxyBase::FShaderParameters>();
}

void UNiagaraDataInterfaceArray::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
	INDIArrayProxyBase& ArrayProxy = Context.GetProxy<INDIArrayProxyBase>();
	INDIArrayProxyBase::FShaderParameters* ShaderParameters = Context.GetParameterNestedStruct<INDIArrayProxyBase::FShaderParameters>();

	ArrayProxy.SetShaderParameters(ShaderParameters, Context.GetSystemInstanceID());
}

UObject* UNiagaraDataInterfaceArray::SimCacheBeginWrite(UObject* SimCache, FNiagaraSystemInstance* NiagaraSystemInstance, const void* OptionalPerInstanceData, FNiagaraSimCacheFeedbackContext& FeedbackContext) const
{
	UNDIArraySimCacheData* CacheData = NewObject<UNDIArraySimCacheData>(SimCache);
	return CacheData;
}

bool UNiagaraDataInterfaceArray::SimCacheWriteFrame(UObject* StorageObject, int FrameIndex, FNiagaraSystemInstance* SystemInstance, const void* OptionalPerInstanceData, FNiagaraSimCacheFeedbackContext& FeedbackContext) const
{
	check(OptionalPerInstanceData && StorageObject);
	UNDIArraySimCacheData* CacheData = CastChecked<UNDIArraySimCacheData>(StorageObject);

	const INDIArrayProxyBase* ArrayProxy = GetProxyAs<INDIArrayProxyBase>();
	return ArrayProxy->SimCacheWriteFrame(CacheData, FrameIndex, SystemInstance);
}

bool UNiagaraDataInterfaceArray::SimCacheReadFrame(UObject* StorageObject, int FrameA, int FrameB, float Interp, FNiagaraSystemInstance* SystemInstance, void* OptionalPerInstanceData)
{
	check(OptionalPerInstanceData && StorageObject);
	UNDIArraySimCacheData* CacheData = CastChecked<UNDIArraySimCacheData>(StorageObject);

	INDIArrayProxyBase* ArrayProxy = GetProxyAs<INDIArrayProxyBase>();
	return ArrayProxy->SimCacheReadFrame(CacheData, FrameA, SystemInstance);
}

FString UNiagaraDataInterfaceArray::SimCacheVisualizerRead(UNDIArraySimCacheData* CacheData, FNDIArraySimCacheDataFrame& FrameData, int Element) const
{
	const INDIArrayProxyBase* ArrayProxy = GetProxyAs<INDIArrayProxyBase>();
	return ArrayProxy->SimCacheVisualizerRead(CacheData, FrameData, Element);
}

int32 UNDIArraySimCacheData::FindOrAddData(TConstArrayView<uint8> ArrayData)
{
	const int32 Last = BufferData.Num() - ArrayData.Num();
	for (int32 i = 0; i <= Last; ++i)
	{
		if (FMemory::Memcmp(BufferData.GetData() + i, ArrayData.GetData(), ArrayData.Num()) == 0)
		{
			return i;
		}
	}

	int32 NewOffset = BufferData.Num();
	BufferData.AddUninitialized(ArrayData.Num());
	FMemory::Memcpy(BufferData.GetData() + NewOffset, ArrayData.GetData(), ArrayData.Num());
	return NewOffset;
}

#undef LOCTEXT_NAMESPACE

