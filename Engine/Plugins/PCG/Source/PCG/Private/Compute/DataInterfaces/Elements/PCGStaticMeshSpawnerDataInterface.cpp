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
		.SetName(TEXT("SMSpawner_GetSelectorAttributeId"))
		.AddReturnType(EShaderFundamentalType::Uint); // Attribute id to get mesh path string key from, or invalid if we should use CDF instead.

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_GetNumAttributes"))
		.AddReturnType(EShaderFundamentalType::Uint); // Num attributes

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_GetNumPrimitives"))
		.AddReturnType(EShaderFundamentalType::Uint); // Num primitives

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_GetAttributeIdOffsetStride"))
		.AddReturnType(EShaderFundamentalType::Uint, 4)
		.AddParam(EShaderFundamentalType::Uint); // InAttributeIndex

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_GetPrimitiveIndexFromStringKey"))
		.AddReturnType(EShaderFundamentalType::Uint) // Primitive index
		.AddParam(EShaderFundamentalType::Uint); // InMeshPathStringKey

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_GetPrimitiveSelectionCDF"))
		.AddReturnType(EShaderFundamentalType::Float) // CDF value
		.AddParam(EShaderFundamentalType::Uint); // InPrimitiveIndex
}

BEGIN_SHADER_PARAMETER_STRUCT(FPCGStaticMeshSpawnerDataInterfaceParameters,)
	SHADER_PARAMETER_ARRAY(FUintVector4, AttributeIdOffsetStrides, [UPCGStaticMeshSpawnerDataInterface::MAX_ATTRIBUTES])
	SHADER_PARAMETER_ARRAY(FUintVector4, PrimitiveStringKeys, [PCGComputeConstants::MAX_PRIMITIVE_COMPONENTS_PER_SPAWNER])
	SHADER_PARAMETER_SCALAR_ARRAY(float, SelectionCDF, [PCGComputeConstants::MAX_PRIMITIVE_COMPONENTS_PER_SPAWNER])
	SHADER_PARAMETER(uint32, NumAttributes)
	SHADER_PARAMETER(uint32, NumPrimitives)
	SHADER_PARAMETER(uint32, SelectorAttributeId)
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
		"uint {DataInterfaceName}_SelectorAttributeId;\n"
		"uint SMSpawner_GetSelectorAttributeId_{DataInterfaceName}() { return {DataInterfaceName}_SelectorAttributeId; }\n"
		"\n"
		"uint {DataInterfaceName}_NumAttributes;\n"
		"uint4 {DataInterfaceName}_AttributeIdOffsetStrides[{MaxAttributes}];\n"
		"uint SMSpawner_GetNumAttributes_{DataInterfaceName}() { return {DataInterfaceName}_NumAttributes; }\n"
		"uint4 SMSpawner_GetAttributeIdOffsetStride_{DataInterfaceName}(uint InAttributeIndex) { return {DataInterfaceName}_AttributeIdOffsetStrides[InAttributeIndex]; }\n"
		"\n"
		"uint {DataInterfaceName}_NumPrimitives;\n"
		"uint4 {DataInterfaceName}_PrimitiveStringKeys[{MaxPrimitives}];"
		"DECLARE_SCALAR_ARRAY(float, {DataInterfaceName}_SelectionCDF, {MaxPrimitives});\n"
		"uint SMSpawner_GetNumPrimitives_{DataInterfaceName}() { return {DataInterfaceName}_NumPrimitives; }\n"
		"\n"
		"uint SMSpawner_GetPrimitiveIndexFromStringKey_{DataInterfaceName}(uint InMeshPathStringKey)\n"
		"{\n"
		"	for (uint Index = 0; Index < {DataInterfaceName}_NumPrimitives; ++Index)\n"
		"	{\n"
		"		if ({DataInterfaceName}_PrimitiveStringKeys[Index][0] == InMeshPathStringKey)\n"
		"		{\n"
		"			return Index;\n"
		"		}\n"
		"	}\n"
		"	\n"
		"	return (uint)-1;\n"
		"}\n"
		"\n"
		"float SMSpawner_GetPrimitiveSelectionCDF_{DataInterfaceName}(uint InPrimitiveIndex) { return GET_SCALAR_ARRAY_ELEMENT({DataInterfaceName}_SelectionCDF, InPrimitiveIndex); }\n"
		"\n"
		), TemplateArgs);
}

UComputeDataProvider* UPCGStaticMeshSpawnerDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGStaticMeshSpawnerDataInterface::CreateDataProvider);
	UPCGDataBinding* Binding = CastChecked<UPCGDataBinding>(InBinding);

	TObjectPtr<UPCGStaticMeshSpawnerDataProvider> DataProvider = NewObject<UPCGStaticMeshSpawnerDataProvider>();
	DataProvider->Settings = Cast<UPCGStaticMeshSpawnerSettings>(Settings);
	
	// If there were 0 input points for this execution, we will not have created any primitives, so check for null.
	if (const FPCGSpawnerPrimitives* Primitives = Binding->MeshSpawnersToPrimitives.Find(Settings))
	{
		DataProvider->AttributeIdOffsetStrides = Primitives->AttributeIdOffsetStrides;
		DataProvider->SelectionCDF = Primitives->SelectionCDF;
		DataProvider->SelectorAttributeId = Primitives->SelectorAttributeId;
		DataProvider->PrimitiveStringKeys = Primitives->PrimitiveStringKeys;
	}

	return DataProvider;
}

FComputeDataProviderRenderProxy* UPCGStaticMeshSpawnerDataProvider::GetRenderProxy()
{
	return new FPCGStaticMeshSpawnerDataProviderProxy(AttributeIdOffsetStrides, SelectorAttributeId, PrimitiveStringKeys, SelectionCDF);
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
		Parameters.NumPrimitives = SelectionCDF.Num();
		Parameters.SelectorAttributeId = (uint32)SelectorAttributeId;

		for (int32 Index = 0; Index < AttributeIdOffsetStrides.Num(); ++Index)
		{
			Parameters.AttributeIdOffsetStrides[Index] = AttributeIdOffsetStrides[Index];
		}

		for (int32 Index = 0; Index < PrimitiveStringKeys.Num(); ++Index)
		{
			Parameters.PrimitiveStringKeys[Index] = FUintVector4(PrimitiveStringKeys[Index], /*Unused*/0, /*Unused*/0, /*Unused*/0);
		}

		for (int32 Index = 0; Index < SelectionCDF.Num(); ++Index)
		{
			GET_SCALAR_ARRAY_ELEMENT(Parameters.SelectionCDF, Index) = SelectionCDF[Index];
		}
	}
}
