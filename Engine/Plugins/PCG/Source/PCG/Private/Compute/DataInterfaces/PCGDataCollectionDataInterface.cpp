// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/DataInterfaces/PCGDataCollectionDataInterface.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGModule.h"
#include "PCGSettings.h"
#include "PCGSubsystem.h"
#include "Compute/PCGDataBinding.h"
#include "Compute/Elements/PCGComputeGraphElement.h"

#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterMetadataBuilder.h"

#define LOCTEXT_NAMESPACE "PCGDataCollectionDataInterface"

void UPCGDataCollectionDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("LoadBufferInternal"))
		.AddReturnType(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("StoreBufferInternal"))
		.AddParam(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Uint);

	// Header Readers
	{
		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetNumData"))
			.AddReturnType(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetDataAddress"))
			.AddReturnType(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetDataId"))
			.AddReturnType(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetDataNumAttributes"))
			.AddReturnType(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetDataPreambleSize"))
			.AddReturnType(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetDataInfo"))
			.AddReturnType(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetDataAttributeHeadersAddress"))
			.AddReturnType(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetThreadData"))
			.AddParam(EShaderFundamentalType::Uint) // InThreadIndex
			.AddParam(EShaderFundamentalType::Uint, 0, 0, EShaderParamModifier::Out) // OutDataIndex
			.AddParam(EShaderFundamentalType::Uint, 0, 0, EShaderParamModifier::Out) // OutDataAddress
			.AddParam(EShaderFundamentalType::Uint, 0, 0, EShaderParamModifier::Out) // OutElementIndex
			.AddReturnType(EShaderFundamentalType::Bool);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetThreadData"))
			.AddParam(EShaderFundamentalType::Uint) // InThreadIndex
			.AddParam(EShaderFundamentalType::Uint, 0, 0, EShaderParamModifier::Out) // OutDataIndex
			.AddParam(EShaderFundamentalType::Uint, 0, 0, EShaderParamModifier::Out) // OutElementIndex
			.AddReturnType(EShaderFundamentalType::Bool);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetNumElements"))
			.AddReturnType(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetAttributeHeaderAddress"))
			.AddReturnType(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetAttributeIdAndStride"))
			.AddReturnType(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetAttributeId"))
			.AddReturnType(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetAttributeStride"))
			.AddReturnType(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetAttributeAddress"))
			.AddReturnType(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetAttributeAddress"))
			.AddReturnType(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);
	}

	// Header Writers
	{
		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetNumDataInternal"))
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetDataAddressInternal"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetDataIdInternal"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetDataNumAttributesInternal"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetDataPreambleSizeInternal"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetDataInfoInternal"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetAttributeIdAndStrideInternal"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetAttributeAddressInternal"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("WriteHeader"));
	}

	// Attribute Getters
	{
		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetBool"))
			.AddReturnType(EShaderFundamentalType::Bool)
			.AddParam(EShaderFundamentalType::Uint) // DataIndex
			.AddParam(EShaderFundamentalType::Uint) // ElementIndex
			.AddParam(EShaderFundamentalType::Uint); // AttributeId

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetInt"))
			.AddReturnType(EShaderFundamentalType::Int)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetFloat"))
			.AddReturnType(EShaderFundamentalType::Float)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetFloat2"))
			.AddReturnType(EShaderFundamentalType::Float, 2)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetFloat3"))
			.AddReturnType(EShaderFundamentalType::Float, 3)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetFloat4"))
			.AddReturnType(EShaderFundamentalType::Float, 4)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetRotator"))
			.AddReturnType(EShaderFundamentalType::Float, 3)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetQuat"))
			.AddReturnType(EShaderFundamentalType::Float, 4)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetTransform"))
			.AddReturnType(EShaderFundamentalType::Float, 4, 4)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);
	}

	// Attribute Setters
	{
		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetBool"))
			.AddParam(EShaderFundamentalType::Uint) // DataIndex
			.AddParam(EShaderFundamentalType::Uint) // ElementIndex
			.AddParam(EShaderFundamentalType::Uint) // AttributeId
			.AddParam(EShaderFundamentalType::Bool); // Value

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetInt"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Int);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetFloat"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetFloat2"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 2);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetFloat3"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 3);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetFloat4"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 4);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetRotator"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 3);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetQuat"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 4);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetTransform"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 4, 4);
	}

	// Point Attribute Getters
	{
		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetPosition"))
			.AddReturnType(EShaderFundamentalType::Float, 3)
			.AddParam(EShaderFundamentalType::Uint) // DataIndex
			.AddParam(EShaderFundamentalType::Uint); // ElementIndex

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetRotation"))
			.AddReturnType(EShaderFundamentalType::Float, 4)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetScale"))
			.AddReturnType(EShaderFundamentalType::Float, 3)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetBoundsMin"))
			.AddReturnType(EShaderFundamentalType::Float, 3)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetBoundsMax"))
			.AddReturnType(EShaderFundamentalType::Float, 3)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetColor"))
			.AddReturnType(EShaderFundamentalType::Float, 4)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetDensity"))
			.AddReturnType(EShaderFundamentalType::Float)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetSeed"))
			.AddReturnType(EShaderFundamentalType::Int)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetSteepness"))
			.AddReturnType(EShaderFundamentalType::Float)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("IsValid"))
			.AddReturnType(EShaderFundamentalType::Bool)
			.AddParam(EShaderFundamentalType::Uint) // DataIndex
			.AddParam(EShaderFundamentalType::Uint); // ElementIndex

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetPointTransform"))
			.AddReturnType(EShaderFundamentalType::Float, 4, 4)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);
	}

	// Point Attribute Setters
	{
		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetPosition"))
			.AddParam(EShaderFundamentalType::Uint) // DataIndex
			.AddParam(EShaderFundamentalType::Uint) // ElementIndex
			.AddParam(EShaderFundamentalType::Float, 3); // Value

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetRotation"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 4);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetScale"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 3);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetBoundsMin"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 3);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetBoundsMax"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 3);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetColor"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 4);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetDensity"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetSeed"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Int);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetSteepness"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetPointTransform"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 4, 4);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("RemovePoint"))
			.AddParam(EShaderFundamentalType::Uint) // DataIndex
			.AddParam(EShaderFundamentalType::Uint); // ElementIndex

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("InitializePoint"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);
	}
}

BEGIN_SHADER_PARAMETER_STRUCT(FPCGDataCollectionDataInterfaceParameters, )
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, DataCollectionBuffer)
END_SHADER_PARAMETER_STRUCT()

void UPCGDataCollectionDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FPCGDataCollectionDataInterfaceParameters>(UID);
}

TCHAR const* UPCGDataCollectionDataInterface::TemplateFilePath = TEXT("/Plugin/PCG/Private/PCGDataCollectionDataInterface.ush");

TCHAR const* UPCGDataCollectionDataInterface::GetShaderVirtualPath() const
{
	return TemplateFilePath;
}

void UPCGDataCollectionDataInterface::GetShaderHash(FString& InOutKey) const
{
	GetShaderFileHash(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);

	// Quaternion library
	GetShaderFileHash(TEXT("/Engine/Private/Quaternion.ush"), EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);
}

void UPCGDataCollectionDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	FString TemplateFile;
	LoadShaderSourceFile(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
	OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}

UComputeDataProvider* UPCGDataCollectionDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
	UPCGDataBinding* Binding = CastChecked<UPCGDataBinding>(InBinding);

	UPCGDataCollectionDataProvider* Provider = NewObject<UPCGDataCollectionDataProvider>();
	Provider->Binding = Binding;
	Provider->ProducerSettings = ProducerSettings;
	Provider->PinDesc = ProducerSettings->ComputeOutputPinDataDesc(OutputPinLabel, Binding);

	Provider->ReadbackMode = bRequiresReadback ? EPCGReadbackMode::GraphOutput : EPCGReadbackMode::None;

#if WITH_EDITOR
	if (Binding->SourceComponent->IsInspecting())
	{
		Provider->ReadbackMode |= EPCGReadbackMode::Inspection;
	}

	if (ProducerSettings->bDebug)
	{
		Provider->ReadbackMode |= EPCGReadbackMode::DebugVisualization;
	}
#endif

	// Use the aliased label for normal data output as this is the output from the compute graph.
	Provider->OutputPinLabelAlias = OutputPinLabelAlias;

	// The original label is needed to store inspection data.
	Provider->OutputPinLabel = OutputPinLabel;

	return Provider;
}

bool UPCGDataCollectionDataInterface::GetRequiresReadback() const
{
#if WITH_EDITOR
	// In editor DIs are always flagged as requiring readback, and decision whether to actually readback data or not
	// is made at execution time (in GetReadbackData) based on inspection state.
	return true;
#else
	return bRequiresReadback;
#endif
}

FComputeDataProviderRenderProxy* UPCGDataCollectionDataProvider::GetRenderProxy()
{
	FPCGDataCollectionDataProviderProxy* Proxy = new FPCGDataCollectionDataProviderProxy(Binding, PinDesc, ReadbackMode);

	if (ReadbackMode != EPCGReadbackMode::None)
	{
		TWeakObjectPtr<UPCGDataCollectionDataProvider> ThisWeakPtr(this);

		Proxy->AsyncReadbackCallback_RenderThread = [ThisWeakPtr](const void* InData, int InNumBytes)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGDataCollectionDataProvider::ProcessReadbackData_RenderThread);

			if (!ThisWeakPtr.IsValid() || !ThisWeakPtr.Get())
			{
				return;
			}

			UPCGDataCollectionDataProvider* ThisDataProvider = ThisWeakPtr.Get();
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
	}

	return Proxy;
}

bool UPCGDataCollectionDataProvider::ProcessReadBackData(FPCGComputeGraphContext* InContext)
{
	check(InContext);
	check(ProducerSettings);

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

	FPCGDataCollection DataFromGPU;
	const EPCGUnpackDataCollectionResult Result = PinDesc.UnpackDataCollection(RawReadbackData, OutputPinLabelAlias, DataFromGPU);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
	if (Result == EPCGUnpackDataCollectionResult::DataMismatch)
	{
		const FText DataMismatchWarningText = FText::Format(
			LOCTEXT("UnpackDataCollectionDataMismatch", "Mismatch in expected data while unpacking GPU data collection on pin '{0}'. Static analysis does not match the received data. Data collection will be ignored."),
			FText::FromName(OutputPinLabel));

#if WITH_EDITOR
		// InContext will be for the compute graph element (injected during compilation). The code below logs against the original
		// node so that the log will be visible on the graph.
		if (ensure(InContext->SourceComponent.IsValid() && InContext->SourceComponent.Get()))
		{
			if (UPCGSubsystem* Subsystem = InContext->SourceComponent->GetSubsystem())
			{
				FPCGStack StackWithNode = InContext->Stack ? *InContext->Stack : FPCGStack();
				StackWithNode.PushFrame(ProducerSettings->GetOuter());
				Subsystem->GetNodeVisualLogsMutable().Log(StackWithNode, ELogVerbosity::Warning, DataMismatchWarningText);
			}
		}
		else
#endif
		{
			PCGE_LOG_C(Warning, LogOnly, InContext, DataMismatchWarningText);
		}
	}
#endif

	RawReadbackData.Reset();

	// Store data in output collection.
	if (!!(ReadbackMode & EPCGReadbackMode::GraphOutput) && ensure(Binding.IsValid()))
	{
		Binding->OutputDataCollection.TaggedData.Append(DataFromGPU.TaggedData);
	}

	// Store data for Inspection.
#if WITH_EDITOR
	if (!!(ReadbackMode & EPCGReadbackMode::Inspection))
	{
		const UPCGNode* Node = Cast<UPCGNode>(ProducerSettings->GetOuter());
		UPCGComponent* Component = InContext->SourceComponent.Get();
		if (Component && InContext->Stack && Node)
		{
			// Virtual pin labels confuse inspection. Apply the original output label before storing.
			for (FPCGTaggedData& Data : DataFromGPU.TaggedData)
			{
				Data.Pin = OutputPinLabel;
			}

			// Required by inspection code.
			DataFromGPU.ComputeCrcs(/*bFullDataCrc=*/false);

			// Input data not yet supported.
			Component->StoreInspectionData(InContext->Stack, Node, /*InTimer=*/nullptr, /*InInputData=*/{}, DataFromGPU, /*bUsedCache*/false);
		}
	}

	if (!!(ReadbackMode & EPCGReadbackMode::DebugVisualization))
	{
		const FPCGDataVisualizationRegistry& DataVisRegistry = FPCGModule::GetConstPCGDataVisualizationRegistry();

		for (const FPCGTaggedData& Output : DataFromGPU.TaggedData)
		{
			if (!Output.Data)
			{
				continue;
			}

			if (const IPCGDataVisualization* DataVis = DataVisRegistry.GetDataVisualization(Output.Data->GetClass()))
			{
				DataVis->ExecuteDebugDisplay(InContext, ProducerSettings, Output.Data, InContext->GetTargetActor(nullptr));
			}
		}
	}
#endif // WITH_EDITOR

	return true;
}

FPCGDataCollectionDataProviderProxy::FPCGDataCollectionDataProviderProxy(
	TWeakObjectPtr<UPCGDataBinding> InBinding,
	const FPCGDataCollectionDesc& InPinDesc,
	EPCGReadbackMode InReadbackMode)
	: ReadbackMode(InReadbackMode)
	, Binding(InBinding)
	, PinDesc(InPinDesc)
{
	SizeBytes = PinDesc.ComputePackedSize();
}

bool FPCGDataCollectionDataProviderProxy::IsValid(FValidationData const& InValidationData) const
{
	if (InValidationData.ParameterStructSize != sizeof(FParameters))
	{
		return false;
	}

	if (!Binding.IsValid())
	{
		UE_LOG(LogPCG, Error, TEXT("Proxy invalid due to missing data binding."));
		return false;
	}

	if (SizeBytes <= 0)
	{
		UE_LOG(LogPCG, Error, TEXT("Proxy invalid due to invalid buffer size."));
		return false;
	}

	return true;
}

void FPCGDataCollectionDataProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	check(BufferUAV);

	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		FParameters& Parameters = ParameterArray[InvocationIndex];
		Parameters.DataCollectionBuffer = BufferUAV;
	}
}

void FPCGDataCollectionDataProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData)
{
	check(SizeBytes > 0);

	// Initialize with an empty data collection. The kernel may not run, for example if indirect dispatch args end up being 0. Ensure
	// there is something meaningful to readback.
	// TODO could have a statically-allocated resource rather than allocating & uploading here.
	TArray<uint32> PackedDataCollection;
	PinDesc.PrepareBufferForKernelOutput(PackedDataCollection);

	FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(PackedDataCollection.GetTypeSize(), PackedDataCollection.Num());
	Buffer = GraphBuilder.CreateBuffer(Desc, TEXT("PCGDataCollectionBuffer"));
	BufferUAV = GraphBuilder.CreateUAV(Buffer);

	GraphBuilder.QueueBufferUpload(Buffer, PackedDataCollection.GetData(), PackedDataCollection.Num() * PackedDataCollection.GetTypeSize(), ERDGInitialDataFlags::None);
}

void FPCGDataCollectionDataProviderProxy::GetReadbackData(TArray<FReadbackData>& OutReadbackData) const
{
	if (ReadbackMode != EPCGReadbackMode::None)
	{
		FReadbackData Data;
		Data.Buffer = Buffer;
		Data.NumBytes = SizeBytes;
		Data.ReadbackCallback_RenderThread = &AsyncReadbackCallback_RenderThread;

		OutReadbackData.Add(MoveTemp(Data));
	}
}

#undef LOCTEXT_NAMESPACE
