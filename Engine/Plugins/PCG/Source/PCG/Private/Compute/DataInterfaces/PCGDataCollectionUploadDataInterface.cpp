// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/DataInterfaces/PCGDataCollectionUploadDataInterface.h"

#include "PCGEdge.h"
#include "PCGModule.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "Compute/PCGComputeGraph.h"
#include "Compute/PCGDataBinding.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"

UComputeDataProvider* UPCGDataCollectionUploadDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
	UPCGDataBinding* Binding = CastChecked<UPCGDataBinding>(InBinding);

	UPCGDataProviderDataCollectionUpload* Provider = NewObject<UPCGDataProviderDataCollectionUpload>();
	Provider->Binding = Binding;

	// Pick the data items from input data collection using any of the compute graph element virtual input pin labels.
	// TODO: Ideally we could call ProducerSettings->ComputeOutputPinDataDesc() but some settings do not have associated nodes/pins.
	check(!DownstreamInputPinLabelAliases.IsEmpty());
	Provider->PinDesc = FPCGDataCollectionDesc::BuildFromInputDataCollectionAndInputPinLabel(
		Binding->DataForGPU.InputDataCollection,
		DownstreamInputPinLabelAliases[0],
		Binding->Graph->GetAttributeLookupTable());

	Provider->DownstreamInputPinLabels = DownstreamInputPinLabelAliases;

	return Provider;
}

FComputeDataProviderRenderProxy* UPCGDataProviderDataCollectionUpload::GetRenderProxy()
{
	return new FPCGDataProviderDataCollectionUploadProxy(Binding, PinDesc, DownstreamInputPinLabels);
}

FPCGDataProviderDataCollectionUploadProxy::FPCGDataProviderDataCollectionUploadProxy(
	TWeakObjectPtr<UPCGDataBinding> InBinding,
	const FPCGDataCollectionDesc& InPinDesc,
	const TArray<FName>& InDownstreamInputPinLabels)
	: FPCGDataCollectionDataProviderProxy(InBinding, InPinDesc, EPCGReadbackMode::None)
{
	DownstreamInputPinLabels = InDownstreamInputPinLabels;
}

void FPCGDataProviderDataCollectionUploadProxy::AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData)
{
	if (!ensure(Binding.IsValid()))
	{
		return;
	}

	TArray<uint32> PackedDataCollection;

	// Use any downstream input pin label to grab data from the collection.
	check(!DownstreamInputPinLabels.IsEmpty());
	PinDesc.PackDataCollection(Binding->DataForGPU.InputDataCollection, DownstreamInputPinLabels[0], PackedDataCollection);
	
	const FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), PackedDataCollection.Num());
	Buffer = GraphBuilder.CreateBuffer(Desc, TEXT("PCGDataCollectionUploadBuffer"));
	BufferUAV = GraphBuilder.CreateUAV(Buffer);

	GraphBuilder.QueueBufferUpload(Buffer, PackedDataCollection.GetData(), PackedDataCollection.Num() * PackedDataCollection.GetTypeSize(), ERDGInitialDataFlags::None);
}
