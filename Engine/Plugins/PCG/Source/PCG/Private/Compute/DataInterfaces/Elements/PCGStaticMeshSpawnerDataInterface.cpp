// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGStaticMeshSpawnerDataInterface.h"

#include "Compute/PCGComputeCommon.h"
#include "Compute/PCGDataBinding.h"
#include "Elements/PCGStaticMeshSpawner.h"

#include "GlobalRenderResources.h"
#include "RHIResources.h"
#include "ShaderCompilerCore.h"
#include "ShaderCore.h"
#include "ShaderParameterMetadataBuilder.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"

void UPCGStaticMeshSpawnerDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_GetNumAttributes"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_GetAttributeIdOffsetStride"))
		.AddReturnType(EShaderFundamentalType::Uint, 4)
		.AddParam(EShaderFundamentalType::Uint); // InAttributeIndex

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_GetNumPrimitives"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_GetPrimitiveSelectionCDF"))
		.AddReturnType(EShaderFundamentalType::Float)
		.AddParam(EShaderFundamentalType::Uint); // InPrimitiveIndex
}

BEGIN_SHADER_PARAMETER_STRUCT(FPCGStaticMeshSpawnerDataInterfaceParameters,)
	SHADER_PARAMETER(uint32, NumAttributes)
	SHADER_PARAMETER_ARRAY(FUintVector4, AttributeIdOffsetStrides, [UPCGStaticMeshSpawnerDataInterface::MAX_ATTRIBUTES])
	SHADER_PARAMETER(uint32, NumPrimitives)
	SHADER_PARAMETER_SCALAR_ARRAY(float, SelectionCDF, [PCGComputeConstants::MAX_PRIMITIVE_COMPONENTS_PER_SPAWNER])
END_SHADER_PARAMETER_STRUCT()

void UPCGStaticMeshSpawnerDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FPCGStaticMeshSpawnerDataInterfaceParameters>(UID);
}

void UPCGStaticMeshSpawnerDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
		{ TEXT("MaxAttributes"), MAX_ATTRIBUTES },
		{ TEXT("MaxPrimitives"), PCGComputeConstants::MAX_PRIMITIVE_COMPONENTS_PER_SPAWNER },
	};

	OutHLSL += FString::Format(TEXT(
		"uint {DataInterfaceName}_NumAttributes;\n"
		"uint4 {DataInterfaceName}_AttributeIdOffsetStrides[{MaxAttributes}];\n"
		"uint SMSpawner_GetNumAttributes_{DataInterfaceName}() { return {DataInterfaceName}_NumAttributes; }\n"
		"uint4 SMSpawner_GetAttributeIdOffsetStride_{DataInterfaceName}(uint InAttributeIndex) { return {DataInterfaceName}_AttributeIdOffsetStrides[InAttributeIndex]; }\n"
		"\n"
		"uint {DataInterfaceName}_NumPrimitives;\n"
		"DECLARE_SCALAR_ARRAY(float, {DataInterfaceName}_SelectionCDF, {MaxPrimitives});\n"
		"uint SMSpawner_GetNumPrimitives_{DataInterfaceName}() { return {DataInterfaceName}_NumPrimitives; }\n"
		"float SMSpawner_GetPrimitiveSelectionCDF_{DataInterfaceName}(uint InPrimitiveIndex) { return GET_SCALAR_ARRAY_ELEMENT({DataInterfaceName}_SelectionCDF, InPrimitiveIndex); }\n"
		), TemplateArgs);
}

UComputeDataProvider* UPCGStaticMeshSpawnerDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
	UPCGDataBinding* Binding = CastChecked<UPCGDataBinding>(InBinding);
	const FPCGSpawnerPrimitives* Primitives = Binding->MeshSpawnersToPrimitives.Find(Settings);
	if (!ensure(Primitives))
	{
		return nullptr;
	}
	if (!ensure(!Primitives->Primitives.IsEmpty()))
	{
		return nullptr;
	}

	TObjectPtr<UPCGStaticMeshSpawnerDataProvider> DataProvider = NewObject<UPCGStaticMeshSpawnerDataProvider>();
	DataProvider->Settings = Cast<UPCGStaticMeshSpawnerSettings>(Settings);
	DataProvider->AttributeIdOffsetStrides = Primitives->AttributeIdOffsetStrides;
	DataProvider->SelectionCDF = Primitives->SelectionCDF;

	return DataProvider;
}

FComputeDataProviderRenderProxy* UPCGStaticMeshSpawnerDataProvider::GetRenderProxy()
{
	return new FPCGStaticMeshSpawnerDataProviderProxy(AttributeIdOffsetStrides, SelectionCDF);
}

bool FPCGStaticMeshSpawnerDataProviderProxy::IsValid(FValidationData const& InValidationData) const
{
	if (InValidationData.ParameterStructSize != sizeof(FParameters))
	{
		return false;
	}

	return true;
}

void FPCGStaticMeshSpawnerDataProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		FParameters& Parameters = ParameterArray[InvocationIndex];

		Parameters.NumAttributes = AttributeIdOffsetStrides.Num();
		for (int32 Index = 0; Index < AttributeIdOffsetStrides.Num(); ++Index)
		{
			Parameters.AttributeIdOffsetStrides[Index] = AttributeIdOffsetStrides[Index];
		}

		Parameters.NumPrimitives = SelectionCDF.Num();
		for (int32 Index = 0; Index < SelectionCDF.Num(); ++Index)
		{
			GET_SCALAR_ARRAY_ELEMENT(Parameters.SelectionCDF, Index) = SelectionCDF[Index];
		}
	}
}
