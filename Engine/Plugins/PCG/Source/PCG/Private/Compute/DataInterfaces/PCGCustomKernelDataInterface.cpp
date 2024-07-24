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

void UPCGCustomKernelDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("GetNumThreads"))
		.AddReturnType(FShaderValueType::Get(EShaderFundamentalType::Int, 3));

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("GetSeed"))
		.AddReturnType(FShaderValueType::Get(EShaderFundamentalType::Uint));

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
	SHADER_PARAMETER(uint32, Seed)
	SHADER_PARAMETER(FVector3f, ComponentBoundsMin)
	SHADER_PARAMETER(FVector3f, ComponentBoundsMax)
END_SHADER_PARAMETER_STRUCT()

void UPCGCustomKernelDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FPCGKernelDataInterfaceParameters>(UID);
}

void UPCGCustomKernelDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	OutHLSL += FString::Format(TEXT(
		"int3 {DataInterfaceName}_NumThreads;\n"
		"uint {DataInterfaceName}_Seed;\n"
		"float3 {DataInterfaceName}_ComponentBoundsMin;\n"
		"float3 {DataInterfaceName}_ComponentBoundsMax;\n"
		"\n"
		"int3 GetNumThreads_{DataInterfaceName}()\n{\n\treturn {DataInterfaceName}_NumThreads;\n}\n\n"
		"uint GetSeed_{DataInterfaceName}()\n{\n\treturn {DataInterfaceName}_Seed;\n}\n\n"
		"float3 GetComponentBoundsMin_{DataInterfaceName}()\n{\n\treturn {DataInterfaceName}_ComponentBoundsMin;\n}\n\n"
		"float3 GetComponentBoundsMax_{DataInterfaceName}()\n{\n\treturn {DataInterfaceName}_ComponentBoundsMax;\n}\n\n"),
		TemplateArgs);
}

UComputeDataProvider* UPCGCustomKernelDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
	check(Settings);

	UPCGDataBinding* Binding = CastChecked<UPCGDataBinding>(InBinding);
	check(Binding->SourceComponent.IsValid() && Binding->SourceComponent.Get());

	UPCGCustomComputeKernelDataProvider* Provider = NewObject<UPCGCustomComputeKernelDataProvider>();
	Provider->ThreadCount = Settings->ComputeKernelThreadCount(Binding);
	Provider->Seed = static_cast<uint32>(Settings->GetSeed(Binding->SourceComponent.Get()));
	Provider->SourceComponentBounds = Binding->SourceComponent.Get()->GetGridBounds();

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

	return new FPCGCustomComputeKernelDataProviderProxy(MoveTemp(InvocationCounts), TotalThreadCount, Seed, SourceComponentBounds);
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
		FParameters& Parameters = ParameterArray[InvocationIndex];

		// Thread count
		Parameters.NumThreads.X = InDispatchData.bUnifiedDispatch ? TotalThreadCount : InvocationThreadCounts[InvocationIndex];
		Parameters.NumThreads.Y = Parameters.NumThreads.Z = 1;

		// Seed for the node
		Parameters.Seed = Seed;

		// Set component bounds
		Parameters.ComponentBoundsMin = (FVector3f)SourceComponentBounds.Min;
		Parameters.ComponentBoundsMax = (FVector3f)SourceComponentBounds.Max;
	}
}
