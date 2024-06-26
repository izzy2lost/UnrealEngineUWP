// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/DataInterfaces/PCGCustomKernelDataInterface.h"

#include "PCGComponent.h"
#include "PCGModule.h"
#include "PCGSettings.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterMetadataBuilder.h"
#include "ComputeFramework/ComputeMetadataBuilder.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGCustomKernelDataInterface)

const TCHAR* UPCGCustomKernelDataInterface::NumThreadsReservedName = TEXT("NumThreads");

void UPCGCustomKernelDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(FString::Printf(TEXT("Get%s"), NumThreadsReservedName))
		.AddReturnType(FShaderValueType::Get(EShaderFundamentalType::Int));

	// A convenient way to serve component bounds to all kernels. Could be pulled out into a PCG context DI in the future.
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("GetComponentBoundsMin"))
		.AddReturnType(EShaderFundamentalType::Float, 3);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("GetComponentBoundsMax"))
		.AddReturnType(EShaderFundamentalType::Float, 3);
}

BEGIN_SHADER_PARAMETER_STRUCT(FPCGKernelDataInterfaceParameters, )
	SHADER_PARAMETER(FIntVector3, NumThreads)
	SHADER_PARAMETER(FVector3f, ComponentBoundsMin)
	SHADER_PARAMETER(FVector3f, ComponentBoundsMax)
END_SHADER_PARAMETER_STRUCT()

void UPCGCustomKernelDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FPCGKernelDataInterfaceParameters>(UID);
}

void UPCGCustomKernelDataInterface::GetShaderHash(FString& InOutKey) const
{
	// UComputeGraph::BuildKernelSource hashes the result of GetHLSL()
	// Only append additional hashes here if the HLSL contains any additional includes	
}

void UPCGCustomKernelDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	const FString TypeName = FShaderValueType::Get(EShaderFundamentalType::Int, 3)->ToString();

	if (ensure(!TypeName.IsEmpty()))
	{
		TMap<FString, FStringFormatArg> TemplateArgs =
		{
			{ TEXT("DataInterfaceName"), InDataInterfaceName },
		};

		// Add uniforms.
		OutHLSL += FString::Printf(TEXT("%s %s_%s;\n"), 
			*TypeName, 
			*InDataInterfaceName, 
			NumThreadsReservedName);
			
		// Add function getters.
		OutHLSL += FString::Printf(TEXT("%s Get%s_%s()\n{\n\treturn %s_%s;\n}\n"), 
			*TypeName,
			NumThreadsReservedName,
			*InDataInterfaceName, 
			*InDataInterfaceName, 
			NumThreadsReservedName);

		// Add helpers (e.g. GetComponentBounds())
		OutHLSL += FString::Format(TEXT(
			"float3 {DataInterfaceName}_ComponentBoundsMin;\n"
			"float3 {DataInterfaceName}_ComponentBoundsMax;\n"
			"\n"
			"float3 GetComponentBoundsMin_{DataInterfaceName}()\n"
			"{\n"
			"	return {DataInterfaceName}_ComponentBoundsMin;\n"
			"}\n"
			"\n"
			"float3 GetComponentBoundsMax_{DataInterfaceName}()\n"
			"{\n"
			"	return {DataInterfaceName}_ComponentBoundsMax;\n"
			"}\n"), TemplateArgs);
	}
}

UComputeDataProvider* UPCGCustomKernelDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
	check(Settings);

	UPCGDataBinding* Binding = CastChecked<UPCGDataBinding>(InBinding);
	check(Binding->SourceComponent.IsValid() && Binding->SourceComponent.Get());

	const FBox ComponentBounds = Binding->SourceComponent.Get()->GetGridBounds();

	UPCGCustomComputeKernelDataProvider* Provider = NewObject<UPCGCustomComputeKernelDataProvider>();
	Provider->SourceComponentBounds = ComponentBounds;
	Provider->ThreadCount = Settings->ComputeKernelThreadCount(Binding);

	return Provider;
}

FComputeDataProviderRenderProxy* UPCGCustomComputeKernelDataProvider::GetRenderProxy()
{
	TArray<int32> InvocationCounts;
	int32 TotalThreadCount = 0;
	
	if (!GetInvocationThreadCounts(InvocationCounts, TotalThreadCount))
	{
		InvocationCounts.Reset();
	}

	return new FPCGCustomComputeKernelDataProviderProxy(MoveTemp(InvocationCounts), TotalThreadCount, SourceComponentBounds);
}

bool UPCGCustomComputeKernelDataProvider::GetInvocationThreadCounts(TArray<int32>& OutInvocationThreadCount, int32& OutTotalThreadCount) const
{
	OutInvocationThreadCount.Reset(1);
	OutInvocationThreadCount.Add(ThreadCount);

	OutTotalThreadCount = ThreadCount;
	
	return true;
}

bool FPCGCustomComputeKernelDataProviderProxy::IsValid(FValidationData const& InValidationData) const
{
	if (InValidationData.ParameterStructSize != sizeof(FParameters))
	{
		UE_LOG(LogPCG, Error, TEXT("Proxy invalid due to mismatching ParameterStructSize."));
		return false;
	}

	if (InvocationThreadCounts.Num() == 0)
	{
		UE_LOG(LogPCG, Error, TEXT("Proxy invalid due to empty InvocationThreadCounts."));
		return false;
	}

	return true;
}

int32 FPCGCustomComputeKernelDataProviderProxy::GetDispatchThreadCount(TArray<FIntVector>& InOutThreadCounts) const
{
	InOutThreadCounts.Reset(InvocationThreadCounts.Num());
	for (const int32 Count : InvocationThreadCounts)
	{
		InOutThreadCounts.Add({Count, 1, 1});
	}
	return InOutThreadCounts.Num();
}

void FPCGCustomComputeKernelDataProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < InDispatchData.NumInvocations; ++InvocationIndex)
	{
		int32 NumThreads;
		if (InDispatchData.bUnifiedDispatch)
		{
			NumThreads = TotalThreadCount;
		}
		else
		{
			NumThreads = InvocationThreadCounts[InvocationIndex];
		}

		FParameters& Parameters = ParameterArray[InvocationIndex];
		Parameters.NumThreads = FIntVector(NumThreads, 1, 1);

		// Set component bounds
		Parameters.ComponentBoundsMin = (FVector3f)SourceComponentBounds.Min;
		Parameters.ComponentBoundsMax = (FVector3f)SourceComponentBounds.Max;
	}
}
