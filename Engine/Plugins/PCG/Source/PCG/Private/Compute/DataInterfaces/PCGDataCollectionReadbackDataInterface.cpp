// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/DataInterfaces/PCGDataCollectionReadbackDataInterface.h"

#include "PCGModule.h"
#include "PCGSettings.h"
#include "Compute/PCGDataBinding.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"

UComputeDataProvider* UPCGDataCollectionReadbackDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
	UPCGDataBinding* Binding = CastChecked<UPCGDataBinding>(InBinding);

	UPCGDataProviderDataCollectionReadback* Provider = NewObject<UPCGDataProviderDataCollectionReadback>();
	Provider->Binding = Binding;
	Provider->PinDesc = ProducerSettings->ComputeOutputPinDataDesc(OutputPinLabel, Binding);
	// Use the aliased label as this is the output from the compute graph.
	Provider->OutputPinLabelAlias = OutputPinLabelAlias;

	return Provider;
}

FComputeDataProviderRenderProxy* UPCGDataProviderDataCollectionReadback::GetRenderProxy()
{
	TWeakObjectPtr<UPCGDataProviderDataCollectionReadback> ThisWeakPtr(this);

	auto ProcessReadbackData_RenderThread = [ThisWeakPtr](const void* InData, int InNumBytes)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGDataProviderDataCollectionReadback::ProcessReadbackData);

		if (!ThisWeakPtr.IsValid() || !ThisWeakPtr.Get())
		{
			return;
		}

		UPCGDataProviderDataCollectionReadback* ThisDataProvider = ThisWeakPtr.Get();
		check(ThisDataProvider);

		// We should never find ourselves stomping existing data.
		check(ThisDataProvider->RawReadbackData.IsEmpty());

		if (InData && InNumBytes > 0)
		{
			// Copy the data to temp storage for the game thread to pick up.
			ThisDataProvider->RawReadbackData.SetNumUninitialized(InNumBytes);
			FMemory::Memcpy(ThisDataProvider->RawReadbackData.GetData(), InData, InNumBytes);
		}
		else
		{
			// Can happen currently if no threads dispatched.
			ThisDataProvider->RawReadbackData.Reset();
		}

		ThisDataProvider->bReadbackComplete = true;

		ThisDataProvider->OnReadbackComplete.Broadcast();
	};

	return new FPCGDataProviderDataCollectionReadbackProxy(Binding, PinDesc, ProcessReadbackData_RenderThread);
}

bool UPCGDataProviderDataCollectionReadback::ProcessReadBackData()
{
	if (!ensure(bReadbackComplete))
	{
		// This should not be called until readback has been done.
		return false;
	}

	if (RawReadbackData.IsEmpty())
	{
		// If there's no data, we leave an empty data collection and we're done.
		return true;
	}

	if (Binding.IsValid())
	{
		// TODO these are not adding in order if multiple buffers are read back? Maybe that never happens here though.
		PinDesc.UnpackDataCollection(RawReadbackData, OutputPinLabelAlias, Binding->OutputDataCollection);
	}

	RawReadbackData.Reset();

	return true;
}

FPCGDataProviderDataCollectionReadbackProxy::FPCGDataProviderDataCollectionReadbackProxy(
	TWeakObjectPtr<UPCGDataBinding> InBinding,
	const FPCGDataCollectionDesc& InPinDesc,
	FReadbackCallback InAsyncReadbackCallback_RenderThread)
	: FPCGDataCollectionDataProviderProxy(InBinding, InPinDesc)
{
	SizeBytes = InPinDesc.ComputePackedSize();
	AsyncReadbackCallback_RenderThread = InAsyncReadbackCallback_RenderThread;
}

bool FPCGDataProviderDataCollectionReadbackProxy::IsValid(FValidationData const& InValidationData) const
{
	if (SizeBytes <= 0)
	{
		UE_LOG(LogPCG, Error, TEXT("Proxy invalid due to invalid size."));
		return false;
	}

	return FPCGDataCollectionDataProviderProxy::IsValid(InValidationData);
}

void FPCGDataProviderDataCollectionReadbackProxy::AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData)
{
	check(SizeBytes > 0);

	FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), SizeBytes / sizeof(uint32));
	Desc.Usage = EBufferUsageFlags(Desc.Usage | BUF_SourceCopy);
	Buffer = GraphBuilder.CreateBuffer(Desc, TEXT("PCGDataCollectionReadbackBuffer"));
	BufferUAV = GraphBuilder.CreateUAV(Buffer);

	// Initialize with an empty data collection. The kernel may not run, for example if indirect dispatch args end up being 0. Ensure
	// there is something meaningful to readback.
	// TODO factor out to avoid doing this work each time.
	TArray<uint32> PackedDataCollection;
	PinDesc.PrepareBufferForKernelOutput(PackedDataCollection);

	GraphBuilder.QueueBufferUpload(Buffer, PackedDataCollection.GetData(), PackedDataCollection.Num() * PackedDataCollection.GetTypeSize(), ERDGInitialDataFlags::None);
}

void FPCGDataProviderDataCollectionReadbackProxy::GetReadbackData(TArray<FReadbackData>& OutReadbackData) const
{
	FReadbackData Data;
	Data.Buffer = Buffer;
	Data.NumBytes = SizeBytes;
	Data.ReadbackCallback_RenderThread = &AsyncReadbackCallback_RenderThread;

	OutReadbackData.Add(MoveTemp(Data));
}
