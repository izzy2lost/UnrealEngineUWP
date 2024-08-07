// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/Elements/PCGCustomHLSL.h"

#include "PCGComputeGraphElement.h"
#include "PCGContext.h"
#include "PCGComponent.h"
#include "PCGEdge.h"
#include "PCGModule.h"
#include "PCGPoint.h"
#include "PCGSubsystem.h"
#include "Compute/PCGComputeCommon.h"
#include "Compute/PCGComputeGraph.h"
#include "Compute/PCGDataBinding.h"
#include "Data/PCGPointData.h"

#include "Internationalization/Regex.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGCustomHLSL)

#define LOCTEXT_NAMESPACE "PCGCustomHLSLElement"
#define PCG_LOGGING_ENABLED (!(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING)

#if PCG_LOGGING_ENABLED
#define PCG_LOG_VALIDATION(ValidationMessage) LogGraphError(ValidationMessage);
#else
#define PCG_LOG_VALIDATION(ValidationMessage) // Log removed
#endif

namespace PCGHLSLElement
{
	/** First capture: Pin name (supports a - z, A - Z, and 0 - 9) */
	constexpr int AttributePinCaptureGroup = 1;

	/** Second capture: Function name (Get or Set) */
	constexpr int AttributeFunctionCaptureGroup = 2;

	/** Third capture: Attribute type (e.g.Int, Float, Rotator, etc.) */
	constexpr int AttributeTypeCaptureGroup = 3;

	/** Fourth capture: Attribute name (supports a-z, A-Z, 0-9, ' ', '-', '_', and '/') */
	constexpr int AttributeNameCaptureGroup = 4;

	/** Regex pattern used to detect and parse attribute function usage in kernels. */
	constexpr TCHAR AttributeFunctionPattern[] = { TEXT("([a-zA-Z0-9]+)_(Get|Set)(.*)\\(.*'([a-zA-Z0-9 -_\\/]+)'.*") };

	constexpr TCHAR AttributeFunctionGet[] = { TEXT("Get") };
	constexpr TCHAR AttributeFunctionSet[] = { TEXT("Set") };

	void ConvertObjectPathToShaderFilePath(FString& InOutPath)
	{
		// Shader compiler recognizes "/Engine/Generated/..." path as special. 
		// It doesn't validate file suffix etc.
		InOutPath = FString::Printf(TEXT("/Engine/Generated/UObject%s.ush"), *InOutPath);
		// Shader compilation result parsing will break if it finds ':' where it doesn't expect.
		InOutPath.ReplaceCharInline(TEXT(':'), TEXT('@'));
	}

	FString GetKernelAttributeKeyAsString(const FPCGKernelAttributeKey& Key)
	{
		return FString::Format(TEXT("'{0}'"), { Key.Name.ToString() });
	}

	FString GetDataTypeString(EPCGDataType Type)
	{
		const UEnum* DataTypeEnum = StaticEnum<EPCGDataType>();
		check(DataTypeEnum);

		return DataTypeEnum->GetValueOrBitfieldAsString(static_cast<int64>(Type));
	}
}

#if WITH_EDITOR
bool FPCGPinPropertiesGPU::CanEditChange(const FEditPropertyChain& PropertyChain) const
{
	if (FProperty* Property = PropertyChain.GetActiveNode()->GetValue())
	{
		if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(FPCGPinProperties, bAllowMultipleData))
		{
			return bAllowEditMultipleData;
		}
	}

	return true;
}
#endif // WITH_EDITOR

UPCGCustomHLSLSettings::UPCGCustomHLSLSettings()
{
	bExecuteOnGPU = true;
	bUseSeed = true;
}

#if WITH_EDITOR
void UPCGCustomHLSLSettings::PostLoad()
{
	Super::PostLoad();

	UpdatePinSettings();
	UpdateAttributeKeys();
}

void UPCGCustomHLSLSettings::PostInitProperties()
{
	Super::PostInitProperties();

	UpdatePinSettings();
	UpdateDeclarations();
}
#endif

TArray<FPCGPinProperties> UPCGCustomHLSLSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	Algo::Transform(OutputPins, PinProperties, [](const FPCGPinPropertiesGPU& InPropertiesGPU) { return InPropertiesGPU; });
	return PinProperties;
}

#if WITH_EDITOR
void UPCGCustomHLSLSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Apply any pin setup before refreshing the node.
	UpdatePinSettings();

	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdateDeclarations();
	UpdateAttributeKeys();
}
#endif

FPCGElementPtr UPCGCustomHLSLSettings::CreateElement() const
{
	return MakeShared<FPCGCustomHLSLElement>();
}

int UPCGCustomHLSLSettings::GetProcessingElemCountForInputPin(const UPCGPin* InputPin, const UPCGDataBinding* Binding) const
{
	check(Binding);
	const FPCGDataForGPU& DataForGPU = Binding->DataForGPU;

	// Upper bound estimate of total number of data elements expected to arrive at this pin.
	int ProcessingElemCount = 0;

	if (DataForGPU.InputPins.Contains(InputPin))
	{
		FName PinLabel = InputPin->Properties.Label;
		if (const FName* PinLabelAlias = DataForGPU.InputPinLabelAliases.Find(InputPin))
		{
			PinLabel = *PinLabelAlias;
		}

		// Data coming straight from CPU, know its size now so count it.
		TArray<FPCGTaggedData> DataForPin = DataForGPU.InputDataCollection.GetInputsByPin(PinLabel);
		for (const FPCGTaggedData& Data : DataForPin)
		{
			ProcessingElemCount += PCGComputeHelpers::GetElementCount(Data.Data);
		}
	}
	else
	{
		// Estimate (upper bound) element count by looking at incident connections.
		for (const UPCGEdge* Edge : InputPin->Edges)
		{
			// InputPin is upstream output pin.
			const UPCGPin* UpstreamOutputPin = Edge->InputPin;
			if (!UpstreamOutputPin)
			{
				continue;
			}

			const UPCGSettings* UpstreamSettings = UpstreamOutputPin->Node ? UpstreamOutputPin->Node->GetSettings() : nullptr;
			check(UpstreamSettings);

			const FPCGDataCollectionDesc PinDesc = UpstreamSettings->ComputeOutputPinDataDesc(UpstreamOutputPin, Binding);
			ProcessingElemCount += PinDesc.ComputeDataElementCount(InputPin->Properties.AllowedTypes);
		}
	}

	return ProcessingElemCount;
}

int UPCGCustomHLSLSettings::ComputeKernelThreadCount(const UPCGDataBinding* Binding) const
{
	int ThreadCount = 0;

	if (KernelType == EPCGKernelType::PointGenerator)
	{
		// Point generator has fixed thread count.
		ThreadCount = PointCount;
	}
	else if (KernelType == EPCGKernelType::PointProcessor)
	{
		// Processing volume depends on data arriving on primary pin.
		const UPCGPin* InputPin = GetPointProcessingInputPin();
		ThreadCount = GetProcessingElemCountForInputPin(InputPin, Binding);
	}
	else if (KernelType == EPCGKernelType::Custom)
	{
		if (DispatchThreadCount == EPCGDispatchThreadCount::FromFirstOutputPin)
		{
			const UPCGPin* OutputPin = GetFirstOutputPin();
			const FPCGPinPropertiesGPU* PropertiesGPU = OutputPin ? GetOutputPinPropertiesGPU(OutputPin->Properties.Label) : nullptr;
			if (PropertiesGPU)
			{
				if (PropertiesGPU->BufferSizeMode == EPCGPinBufferSizeMode::FixedElementCount)
				{
					ThreadCount = PropertiesGPU->FixedBufferElementCount;
				}
				else if (PropertiesGPU->BufferSizeMode == EPCGPinBufferSizeMode::FromFirstPin)
				{
					if (const UPCGPin* InputPin = GetFirstInputPin())
					{
						ThreadCount = GetProcessingElemCountForInputPin(InputPin, Binding);
					}
				}
				else if (PropertiesGPU->BufferSizeMode == EPCGPinBufferSizeMode::FromProductOfInputPins)
				{
					for (const FName& PinLabel : PropertiesGPU->BufferSizeInputPinLabels)
					{
						if (const UPCGPin* InputPin = GetInputPin(PinLabel))
						{
							ThreadCount = FMath::Max(ThreadCount, 1) * GetProcessingElemCountForInputPin(InputPin, Binding);
						}
					}
				}
			}
		}
		else if (DispatchThreadCount == EPCGDispatchThreadCount::FromProductOfInputPins)
		{
			for (const FName& PinLabel : ThreadCountInputPinLabels)
			{
				if (const UPCGPin* InputPin = GetInputPin(PinLabel))
				{
					ThreadCount = FMath::Max(ThreadCount, 1) * GetProcessingElemCountForInputPin(InputPin, Binding);
				}
			}
		}
		else if (DispatchThreadCount == EPCGDispatchThreadCount::Fixed)
		{
			ThreadCount = FixedThreadCount;
		}
	}
	else
	{
		checkNoEntry();
	}

	if (IsThreadCountMultiplierInUse())
	{
		ThreadCount *= ThreadCountMultiplier;
	}

	return ThreadCount;
}

FPCGDataCollectionDesc UPCGCustomHLSLSettings::ComputeOutputPinDataDesc(const UPCGPin* OutputPin, const UPCGDataBinding* Binding) const
{
	check(OutputPin);
	FPCGDataCollectionDesc PinDesc;

	const FPCGPinPropertiesGPU* PropertiesGPU = GetOutputPinPropertiesGPU(OutputPin->Properties.Label);
	const UPCGPin* FirstOutputPin = GetFirstOutputPin();

	// The primary output pin follows any rules prescribed by kernel type.
	if (OutputPin == FirstOutputPin && KernelType == EPCGKernelType::PointProcessor)
	{
		// First output pin passes through first input pin.
		if (const UPCGPin* PointProcessingInputPin = GetPointProcessingInputPin())
		{
			PinDesc = ComputeInputPinDataDesc(PointProcessingInputPin, Binding);
		}
	}
	else if (OutputPin == FirstOutputPin && KernelType == EPCGKernelType::PointGenerator)
	{
		// Generators always produce a single point data with known point count.
		PinDesc.DataDescs.Emplace(EPCGDataType::Point, PointCount);
	}
	else if (ensure(PropertiesGPU))
	{
		// No size set by kernel, fall back to pin settings.
		if (PropertiesGPU->BufferSizeMode == EPCGPinBufferSizeMode::FromFirstPin)
		{
			if (const UPCGPin* InputPin = GetFirstInputPin())
			{
				PinDesc = ComputeInputPinDataDesc(InputPin, Binding);
			}
		}
		else if (PropertiesGPU->BufferSizeMode == EPCGPinBufferSizeMode::FromProductOfInputPins)
		{
			int TotalElementCount = 0;

			for (const FName& PinLabel : PropertiesGPU->BufferSizeInputPinLabels)
			{
				if (const UPCGPin* InputPin = GetInputPin(PinLabel))
				{
					const int ElementCount = ComputeInputPinDataDesc(InputPin, Binding).ComputeDataElementCount(PropertiesGPU->AllowedTypes);
					TotalElementCount = FMath::Max(TotalElementCount, 1) * ElementCount;
				}
			}

			if (TotalElementCount > 0)
			{
				PinDesc.DataDescs.Emplace(PropertiesGPU->AllowedTypes, TotalElementCount);
			}
		}
		else if (PropertiesGPU->BufferSizeMode == EPCGPinBufferSizeMode::FixedElementCount)
		{
			if (ensure(PropertiesGPU->FixedBufferElementCount > 0))
			{
				const UPCGPin* InitializeFromPin = (PropertiesGPU->AllowedTypes == EPCGDataType::Param) ? GetInputPin(PropertiesGPU->InitializeFromPin) : nullptr;

				if (InitializeFromPin)
				{
					PinDesc = ComputeInputPinDataDesc(InitializeFromPin, Binding);
				}
				else
				{
					PinDesc.DataDescs.Emplace(PropertiesGPU->AllowedTypes, PropertiesGPU->FixedBufferElementCount);
				}
			}
		}
		else
		{
			checkNoEntry();
		}
	}

	const TMap<FPCGKernelAttributeKey, int32>& GlobalAttributeLookupTable = ensure(Binding && Binding->Graph) ? Binding->Graph->GetAttributeLookupTable() : TMap<FPCGKernelAttributeKey, int32>();

	for (const FPCGKernelAttributeKey& AttributeKey : KernelAttributeKeys)
	{
		// Add attributes that will be created for this pin on the GPU.
		if (const TArray<TTuple<FPCGKernelAttributeKey, bool>>* Keys = PinToAttributeKeys.Find(OutputPin->Properties.Label))
		{
			const TTuple<FPCGKernelAttributeKey, bool>* Pair = Keys->FindByPredicate([AttributeKey](const TTuple<FPCGKernelAttributeKey, bool>& Pair) { return Pair.Key == AttributeKey; });
			const bool bCreatedOnGPU = Pair && Pair->Value;

			if (bCreatedOnGPU)
			{
				for (FPCGDataDesc& DataDesc : PinDesc.DataDescs)
				{
					if (const int* Index = GlobalAttributeLookupTable.Find(AttributeKey))
					{
						const FPCGKernelAttributeDesc AttributeDesc(*Index, AttributeKey.Type, AttributeKey.Name);
						DataDesc.AttributeDescs.AddUnique(AttributeDesc);
					}
				}
			}
		}
	}

	return PinDesc;
}

#if WITH_EDITOR
EPCGChangeType UPCGCustomHLSLSettings::GetChangeTypeForProperty(const FName& InPropertyName) const
{
	EPCGChangeType ChangeType = Super::GetChangeTypeForProperty(InPropertyName);

	if (InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGCustomHLSLSettings, ShaderSource)
		|| InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGCustomHLSLSettings, ShaderFunctions))
	{
		ChangeType |= EPCGChangeType::ShaderSource;
	}

	// Any settings change to this node could change the compute graph.
	ChangeType |= EPCGChangeType::Structural;

	return ChangeType;
}
#endif

const UPCGPin* UPCGCustomHLSLSettings::GetInputPin(FName Label) const
{
	const UPCGNode* Node = Cast<UPCGNode>(GetOuter());
	return Node ? Node->GetInputPin(Label) : nullptr;
}

const UPCGPin* UPCGCustomHLSLSettings::GetOutputPin(FName Label) const
{
	const UPCGNode* Node = Cast<UPCGNode>(GetOuter());
	return Node ? Node->GetOutputPin(Label) : nullptr;
}

const UPCGPin* UPCGCustomHLSLSettings::GetFirstInputPin() const
{
	if (const UPCGNode* Node = Cast<UPCGNode>(GetOuter()))
	{
		return !Node->GetInputPins().IsEmpty() ? Node->GetInputPins()[0] : nullptr;
	}

	return nullptr;
}

const UPCGPin* UPCGCustomHLSLSettings::GetPointProcessingInputPin() const
{
	if (const UPCGNode* Node = Cast<UPCGNode>(GetOuter()))
	{
		const UPCGPin* FirstPin = !Node->GetInputPins().IsEmpty() ? Node->GetInputPins()[0] : nullptr;
		if (FirstPin && FirstPin->Properties.AllowedTypes == EPCGDataType::Point)
		{
			return FirstPin;
		}
	}

	return nullptr;
}

const UPCGPin* UPCGCustomHLSLSettings::GetFirstOutputPin() const
{
	if (const UPCGNode* Node = Cast<UPCGNode>(GetOuter()))
	{
		return !Node->GetOutputPins().IsEmpty() ? Node->GetOutputPins()[0] : nullptr;
	}

	return nullptr;
}

const UPCGPin* UPCGCustomHLSLSettings::GetFirstPointOutputPin() const
{
	if (const UPCGNode* Node = Cast<UPCGNode>(GetOuter()))
	{
		const UPCGPin* FirstPin = !Node->GetOutputPins().IsEmpty() ? Node->GetOutputPins()[0] : nullptr;
		if (FirstPin && FirstPin->Properties.AllowedTypes == EPCGDataType::Point)
		{
			return FirstPin;
		}
	}

	return nullptr;
}

const FPCGPinPropertiesGPU* UPCGCustomHLSLSettings::GetOutputPinPropertiesGPU(const FName& InPinLabel) const
{
	return OutputPins.FindByPredicate([InPinLabel](const FPCGPinPropertiesGPU& InProperties)
	{
		return InProperties.Label == InPinLabel;
	});
}

#if WITH_EDITOR
void UPCGCustomHLSLSettings::UpdateDeclarations()
{
	// Reference: UOptimusNode_CustomComputeKernel::UpdatePreamble
	InputDeclarations.Reset();
	OutputDeclarations.Reset();

	// Add constants category
	{
		if (KernelType == EPCGKernelType::PointGenerator)
		{
			const UPCGPin* PointProcessingOutputPin = GetFirstPointOutputPin();
			InputDeclarations += FString::Format(
				TEXT("// Constants\n")
				TEXT("uint PointCount = {0};\n")
				TEXT("\n"),
				{ PointCount });
		}
	}

	// Add resource indexing category
	{
		InputDeclarations += TEXT("// Resource Indexing\n");
		InputDeclarations += TEXT("uint ThreadIndex;\n");

		if (KernelType == EPCGKernelType::PointProcessor)
		{
			const UPCGPin* PointProcessingInputPin = GetPointProcessingInputPin();
			const UPCGPin* PointProcessingOutputPin = GetFirstPointOutputPin();

			if (PointProcessingInputPin && PointProcessingOutputPin)
			{
				InputDeclarations += FString::Format(
					TEXT("uint {0}_DataIndex;\n")
					TEXT("uint {1}_DataIndex;\n"),
					{ PointProcessingInputPin->Properties.Label.ToString(),  PointProcessingOutputPin->Properties.Label.ToString() });
			}
		}
		else if (KernelType == EPCGKernelType::PointGenerator)
		{
			if (const UPCGPin* PointProcessingOutputPin = GetFirstPointOutputPin())
			{
				InputDeclarations += FString::Format(
					TEXT("uint {0}_DataIndex;\n"),
					{ PointProcessingOutputPin->Properties.Label.ToString() });
			}
		}
		else if (KernelType == EPCGKernelType::Custom)
		{
			auto EmitGetThreadElement = [&InInputDeclarations = InputDeclarations](const FPCGPinProperties& Properties)
			{
				InInputDeclarations += FString::Format(TEXT(
					"// Returns false if thread has no data to operate on.\n"
					"bool {0}_GetThreadData(uint ThreadIndex, out uint OutDataIndex, out uint OutElementIndex);\n"
					"bool {0}_GetThreadData(uint ThreadIndex, out uint OutDataIndex, out uint OutDataAddress, out uint OutElementIndex);\n"),
					{ Properties.Label.ToString() });
			};

			for (const FPCGPinProperties& Properties : InputPinProperties())
			{
				EmitGetThreadElement(Properties);
			}

			for (const FPCGPinProperties& Properties : OutputPinProperties())
			{
				EmitGetThreadElement(Properties);
			}
		}
		else
		{
			checkNoEntry();
		}

		InputDeclarations += TEXT("uint ElementIndex;\n");
		InputDeclarations += TEXT("int3 GetNumThreads();\n");
	}

	// Add debug category
	{
		if (bPrintShaderDebugValues)
		{
			InputDeclarations += FString::Format(
				TEXT("\n// Debug\n")
				TEXT("void WriteDebugValue(uint Index, float Value); // Index in [0, {0}] (set from 'Debug Buffer Size' property)\n"),
				{ DebugBufferSize - 1 });
		}
	}

	// Per-pin input category
	{
		TArray<FString> DataPins;
		TArray<FString> PointDataPins;
		TArray<FString> SplineDataPins;
		TArray<FString> LandscapeDataPins;
		TArray<FString> TextureDataPins;
		TArray<FString> RawBufferDataPins;

		for (const FPCGPinProperties& Pin : InputPinProperties())
		{
			DataPins.Add(Pin.Label.ToString());

			if (!!(Pin.AllowedTypes & EPCGDataType::Point))
			{
				PointDataPins.Add(Pin.Label.ToString());
			}

			if (!!(Pin.AllowedTypes & EPCGDataType::Spline))
			{
				SplineDataPins.Add(Pin.Label.ToString());
			}

			if (!!(Pin.AllowedTypes & EPCGDataType::Landscape))
			{
				LandscapeDataPins.Add(Pin.Label.ToString());
			}

			if (!!(Pin.AllowedTypes & EPCGDataType::Texture))
			{
				TextureDataPins.Add(Pin.Label.ToString());
			}
		}

		InputDeclarations += TEXT("\n### HELPER FUNCTIONS ###\n");
		InputDeclarations += TEXT("\nfloat3 GetComponentBoundsMin(); // World-space\n");
		InputDeclarations += TEXT("float3 GetComponentBoundsMax();\n");
		InputDeclarations += TEXT("uint GetSeed();\n");

		InputDeclarations += TEXT("\nfloat FRand(inout uint Seed); // Returns random float between 0 and 1.\n");
		InputDeclarations += TEXT("uint ComputeSeed(uint A, uint B);\n");
		InputDeclarations += TEXT("uint ComputeSeed(uint A, uint B, uint C);\n");
		InputDeclarations += TEXT("uint ComputeSeedFromPosition(float3 Position);\n");

		InputDeclarations += TEXT("\nfloat3 CreateGrid2D(int ElementIndex, int NumPoints, float3 Min, float3 Max);\n");
		InputDeclarations += TEXT("float3 CreateGrid2D(int ElementIndex, int NumPoints, int NumRows, float3 Min, float3 Max);\n");
		InputDeclarations += TEXT("float3 CreateGrid3D(int ElementIndex, int NumPoints, float3 Min, float3 Max);\n");
		InputDeclarations += TEXT("float3 CreateGrid3D(int ElementIndex, int NumPoints, int NumRows, int NumCols, float3 Min, float3 Max);\n");

		if (!DataPins.IsEmpty())
		{
			InputDeclarations += TEXT("\n### DATA FUNCTIONS ###\n");
			InputDeclarations += TEXT("\n// Valid pins: ") + FString::Join(DataPins, TEXT(", ")) + TEXT("\n");
			InputDeclarations += TEXT("// Valid types: bool, int, float, float2, float3, float4, Rotator (float3), Quat (float4), Transform (float4x4)\n");

			InputDeclarations += TEXT("\nuint <pin>_GetNumData();\n");
			InputDeclarations += TEXT("uint <pin>_GetNumElements();\n");
			InputDeclarations += TEXT("<type> <pin>_Get<type>(uint DataIndex, uint ElementIndex, uint AttributeId);\n");
			InputDeclarations += TEXT("<type> <pin>_Get<type>(uint DataIndex, uint ElementIndex, 'AttributeName');\n");
		}

		if (!PointDataPins.IsEmpty())
		{
			InputDeclarations += TEXT("\n### POINT DATA FUNCTIONS ###\n");
			InputDeclarations += TEXT("\n// Valid pins: ") + FString::Join(PointDataPins, TEXT(", ")) + TEXT("\n");

			InputDeclarations += TEXT("\nuint <pin>_GetNumPoints(uint DataIndex);\n");
			InputDeclarations += TEXT("float3 <pin>_GetPosition(uint DataIndex, uint ElementIndex);\n");
			InputDeclarations += TEXT("float4 <pin>_GetRotation(uint DataIndex, uint ElementIndex);\n");
			InputDeclarations += TEXT("float3 <pin>_GetScale(uint DataIndex, uint ElementIndex);\n");
			InputDeclarations += TEXT("float3 <pin>_GetBoundsMin(uint DataIndex, uint ElementIndex);\n");
			InputDeclarations += TEXT("float3 <pin>_GetBoundsMax(uint DataIndex, uint ElementIndex);\n");
			InputDeclarations += TEXT("float3 <pin>_GetColor(uint DataIndex, uint ElementIndex);\n");
			InputDeclarations += TEXT("float <pin>_GetDensity(uint DataIndex, uint ElementIndex);\n");
			InputDeclarations += TEXT("int <pin>_GetSeed(uint DataIndex, uint ElementIndex);\n");
			InputDeclarations += TEXT("float <pin>_GetSteepness(uint DataIndex, uint ElementIndex);\n");
			InputDeclarations += TEXT("float4x4 <pin>_GetPointTransform(uint DataIndex, uint ElementIndex);\n");
			InputDeclarations += TEXT("bool <pin>_IsValid(uint DataIndex, uint ElementIndex);\n");
		}

		if (!LandscapeDataPins.IsEmpty())
		{
			InputDeclarations += TEXT("\n### LANDSCAPE DATA FUNCTIONS ###\n");
			InputDeclarations += TEXT("\n// Valid pins: ") + FString::Join(LandscapeDataPins, TEXT(", ")) + TEXT("\n");

			InputDeclarations += TEXT("\nfloat <pin>_GetHeight(float3 WorldPos);\n");
			InputDeclarations += TEXT("float3 <pin>_GetNormal(float3 WorldPos);\n");
		}

		if (!TextureDataPins.IsEmpty())
		{
			InputDeclarations += TEXT("\n### TEXTURE DATA FUNCTIONS ###\n");
			InputDeclarations += TEXT("\n// Valid pins: ") + FString::Join(TextureDataPins, TEXT(", ")) + TEXT("\n");

			InputDeclarations += TEXT("\nfloat2 <pin>_GetTexCoords(float2 WorldPos, float2 Min, float2 Max);\n");
			InputDeclarations += TEXT("float4 <pin>_Sample(float2 TexCoords);\n");
		}

		if (!RawBufferDataPins.IsEmpty())
		{
			InputDeclarations += TEXT("\n### BYTE ADDRESS BUFFER DATA FUNCTIONS ###\n");
			InputDeclarations += TEXT("\n// Valid pins: ") + FString::Join(RawBufferDataPins, TEXT(", ")) + TEXT("\n");

			InputDeclarations += TEXT("\nuint <pin>_ReadNumValues();\n");
			InputDeclarations += TEXT("uint <pin>_ReadValue(uint Index);\n");
		}
	}

	// Per-pin output category
	{
		TArray<FString> DataPins;
		TArray<FString> PointDataPins;
		TArray<FString> SplineDataPins;
		TArray<FString> RawBufferDataPins;

		for (const FPCGPinProperties& Pin : OutputPinProperties())
		{
			DataPins.Add(Pin.Label.ToString());

			if (!!(Pin.AllowedTypes & EPCGDataType::Point))
			{
				PointDataPins.Add(Pin.Label.ToString());
			}

			if (!!(Pin.AllowedTypes & EPCGDataType::Spline))
			{
				SplineDataPins.Add(Pin.Label.ToString());
			}
		}

		if (!DataPins.IsEmpty())
		{
			OutputDeclarations += TEXT("### DATA FUNCTIONS ###\n");
			OutputDeclarations += TEXT("\n// Valid pins: ") + FString::Join(DataPins, TEXT(", ")) + TEXT("\n");
			OutputDeclarations += TEXT("// Valid types: bool, int, float, float2, float3, float4, Rotator (float3), Quat (float4), Transform (float4x4)\n");

			OutputDeclarations += TEXT("\nvoid <pin>_Set<type>(uint DataIndex, uint ElementIndex, uint AttributeId, <type> Value);\n");
			OutputDeclarations += TEXT("void <pin>_Set<type>(uint DataIndex, uint ElementIndex, 'AttributeName', <type> Value);\n");
		}

		if (!PointDataPins.IsEmpty())
		{
			OutputDeclarations += TEXT("\n### POINT DATA FUNCTIONS ###\n");
			OutputDeclarations += TEXT("\n// Valid pins: ") + FString::Join(PointDataPins, TEXT(", ")) + TEXT("\n");

			OutputDeclarations += TEXT("\nvoid <pin>_SetPosition(uint DataIndex, uint ElementIndex, float3 Position);\n");
			OutputDeclarations += TEXT("void <pin>_SetRotation(uint DataIndex, uint ElementIndex, float4 Rotation);\n");
			OutputDeclarations += TEXT("void <pin>_SetScale(uint DataIndex, uint ElementIndex, float3 Scale);\n");
			OutputDeclarations += TEXT("void <pin>_SetBoundsMin(uint DataIndex, uint ElementIndex, float3 BoundsMin);\n");
			OutputDeclarations += TEXT("void <pin>_SetBoundsMax(uint DataIndex, uint ElementIndex, float3 BoundsMax);\n");
			OutputDeclarations += TEXT("void <pin>_SetColor(uint DataIndex, uint ElementIndex, float4 Color);\n");
			OutputDeclarations += TEXT("void <pin>_SetDensity(uint DataIndex, uint ElementIndex, float Density);\n");
			OutputDeclarations += TEXT("void <pin>_SetSeed(uint DataIndex, uint ElementIndex, int Seed);\n");
			OutputDeclarations += TEXT("void <pin>_SetSeedFromPosition(uint DataIndex, uint ElementIndex, float3 Position);\n");
			OutputDeclarations += TEXT("void <pin>_SetSteepness(uint DataIndex, uint ElementIndex, float Steepness);\n");
			OutputDeclarations += TEXT("void <pin>_SetPointTransform(uint DataIndex, uint ElementIndex, float4x4 Transform);\n");
			OutputDeclarations += TEXT("bool <pin>_RemovePoint(uint DataIndex, uint ElementIndex);\n");
		}

		if (!RawBufferDataPins.IsEmpty())
		{
			OutputDeclarations += TEXT("\n### BYTE ADDRESS BUFFER DATA FUNCTIONS ###\n\n");
			OutputDeclarations += TEXT("// Valid pins: ") + FString::Join(RawBufferDataPins, TEXT(", ")) + TEXT("\n");

			OutputDeclarations += TEXT("\nuint <pin>_WriteValue(uint Index, uint Value);\n");
		}
	}

	if (!OutputDeclarations.IsEmpty())
	{
		// Remove final newline as a small UI improvement.
		OutputDeclarations = OutputDeclarations.LeftChop(1);
	}
}

void UPCGCustomHLSLSettings::UpdatePinSettings()
{
	// Setup input pins.
	for (int PinIndex = 0; PinIndex < InputPins.Num(); ++PinIndex)
	{
		FPCGPinProperties& Properties = InputPins[PinIndex];

		// Type Any is not allowed, default to Point
		if (Properties.AllowedTypes == EPCGDataType::Any)
		{
			Properties.AllowedTypes = EPCGDataType::Point;
		}

		if (!!(Properties.AllowedTypes & EPCGDataType::Landscape)
			|| !!(Properties.AllowedTypes & EPCGDataType::Texture)
			|| !!(Properties.AllowedTypes & EPCGDataType::Param))
		{
			// Don't allow multiple data on this pin because we do not support a dynamic number of textures/landscapes bound to a
			// compute kernel.
			// Also disallow multi-data for Attribute Sets, since we require attributes to be uniform on a pin, thus having different
			// attribute sets wouldn't work as expected.
			Properties.bAllowMultipleData = false;
			Properties.bAllowEditMultipleData = false;
		}
		else
		{
			Properties.bAllowEditMultipleData = true;
		}

		// TODO: We have work to do to allow dynamic merging of data. Also we will likely inject Gather
		// nodes on the CPU side so that merging is handled CPU side where possible.
		Properties.SetAllowMultipleConnections(false);
		Properties.bAllowEditMultipleConnections = false;
	}

	// Setup output pins.
	for (int PinIndex = 0; PinIndex < OutputPins.Num(); ++PinIndex)
	{
		FPCGPinPropertiesGPU& Properties = OutputPins[PinIndex];

		// Type Any is not allowed, default to Point
		if (Properties.AllowedTypes == EPCGDataType::Any)
		{
			Properties.AllowedTypes = EPCGDataType::Point;
		}

		// Primary pin settings driven by kernel (if not custom kernel type).
		const bool bPinCanBeSized = PinIndex > 0 || KernelType == EPCGKernelType::Custom;
		const bool bDataCanBeSized = Properties.AllowedTypes == EPCGDataType::Point;
		Properties.bDisplayBufferSizeSettings = bPinCanBeSized && bDataCanBeSized;

		// Output pins should always allow multiple connections.
		// TODO this could be hoisted up somewhere in the future.
		Properties.bAllowEditMultipleConnections = false;

		if (!!(Properties.AllowedTypes & EPCGDataType::Param))
		{
			Properties.bAllowMultipleData = false;
			Properties.bAllowEditMultipleData = false;
			Properties.bAllowEditInitializationPin = true;
			Properties.BufferSizeMode = EPCGPinBufferSizeMode::FixedElementCount;
		}
		else
		{
			Properties.bAllowEditMultipleData = true;
			Properties.bAllowEditInitializationPin = false;
		}
	}
}

void UPCGCustomHLSLSettings::UpdateAttributeKeys()
{
	KernelAttributeKeys.Reset();
	PinToAttributeKeys.Reset();

	const FString Source = ShaderFunctions + ShaderSource;
	FRegexMatcher ModuleMatcher(FRegexPattern(PCGHLSLElement::AttributeFunctionPattern), Source);

	while (ModuleMatcher.FindNext())
	{
		const FString PinStr = ModuleMatcher.GetCaptureGroup(PCGHLSLElement::AttributePinCaptureGroup);
		const FString FuncStr = ModuleMatcher.GetCaptureGroup(PCGHLSLElement::AttributeFunctionCaptureGroup);
		const FString TypeStr = ModuleMatcher.GetCaptureGroup(PCGHLSLElement::AttributeTypeCaptureGroup);
		const FString NameStr = ModuleMatcher.GetCaptureGroup(PCGHLSLElement::AttributeNameCaptureGroup);

		const int LineStartIndex = ModuleMatcher.GetMatchBeginning();
		int CurrentSourceIndex = Source.Find("\n", ESearchCase::IgnoreCase, ESearchDir::FromStart, 0);
		int LineNumber = 0;

		while (CurrentSourceIndex < LineStartIndex && CurrentSourceIndex != INDEX_NONE)
		{
			++LineNumber;
			CurrentSourceIndex = Source.Find("\n", ESearchCase::IgnoreCase, ESearchDir::FromStart, CurrentSourceIndex + 1);
		}

		if (PinStr.IsEmpty() || FuncStr.IsEmpty() || TypeStr.IsEmpty() || NameStr.IsEmpty())
		{
			UE_LOG(LogPCG, Error, TEXT("Invalid attribute usage in shader source, line %d."), LineNumber);
			continue;
		}

		const UEnum* AttributeTypeEnum = StaticEnum<EPCGKernelAttributeType>();
		check(AttributeTypeEnum);

		const int64 AttributeType = AttributeTypeEnum->GetValueByName(FName(*TypeStr));

		if (AttributeType == INDEX_NONE)
		{
			UE_LOG(LogPCG, Error, TEXT("Invalid attribute type in shader source, line %d."), LineNumber);
			continue;
		}

		// Add the attribute if it hasn't already been referenced.
		FPCGKernelAttributeKey Key;
		Key.Type = static_cast<EPCGKernelAttributeType>(AttributeType);
		Key.Name = FName(*NameStr);
		KernelAttributeKeys.AddUnique(Key);

		// Add an entry mapping this pin to the referenced attribute, if the entry doesn't already exist.
		TArray<TTuple<FPCGKernelAttributeKey, bool>>& Keys = PinToAttributeKeys.FindOrAdd(FName(*PinStr));
		Keys.AddUnique(MakeTuple(Key, false));
	}

	// Process each output pin for any new attributes they want to create.
	for (const FPCGPinPropertiesGPU& OutputPin : OutputPins)
	{
		for (const FPCGKernelAttributeKey& Key : OutputPin.CreatedKernelAttributeKeys)
		{
			KernelAttributeKeys.AddUnique(Key);

			TArray<TTuple<FPCGKernelAttributeKey, bool>>& Keys = PinToAttributeKeys.FindOrAdd(OutputPin.Label);
			TTuple<FPCGKernelAttributeKey, bool>* Pair = Keys.FindByPredicate([Key](const TTuple<FPCGKernelAttributeKey, bool>& Pair) { return Pair.Key == Key; });

			// Mark as created on GPU
			if (Pair)
			{
				Pair->Value = true;
			}
			else
			{
				Keys.Add(MakeTuple(Key, /*bCreatedOnGPU=*/true));
			}
		}
	}
}
#endif

bool UPCGCustomHLSLSettings::IsKernelValid(FPCGContext* InContext, bool bQuiet) const
{
	auto LogGraphError = [InContext, bQuiet, This=this](const FText& InText)
	{
		if (!bQuiet)
		{
#if WITH_EDITOR
			if (InContext && ensure(InContext->SourceComponent.IsValid() && InContext->SourceComponent.Get()))
			{
				if (UPCGSubsystem* Subsystem = InContext->SourceComponent->GetSubsystem())
				{
					FPCGStack StackWithNode = InContext->Stack ? *InContext->Stack : FPCGStack();
					StackWithNode.PushFrame(This->GetOuter());

					Subsystem->GetNodeVisualLogsMutable().Log(StackWithNode, ELogVerbosity::Error, InText);
				}
			}
#endif

			PCGE_LOG_C(Error, LogOnly, InContext, InText);
		}
	};

	if (OutputPins.IsEmpty())
	{
		PCG_LOG_VALIDATION(LOCTEXT("NoOutputs", "Custom HLSL nodes must have at least one output."));
		return false;
	}

	auto CheckPinLabel = [InContext, &InPins = InputPins, OutPins = OutputPinProperties(), &LogGraphError](FName PinLabel)
	{
		if (PinLabel == NAME_None)
		{
			PCG_LOG_VALIDATION(LOCTEXT("InvalidPinLabelNone", "Pin label 'None' is not a valid pin label."));
			return false;
		}

		bool bFoundPinLabel = false;

		auto IsAlreadyFound = [InContext, &LogGraphError, PinLabel, &bFoundPinLabel](const FPCGPinProperties PinProps)
		{
			if (PinProps.Label == PinLabel)
			{
				if (bFoundPinLabel)
				{
					PCG_LOG_VALIDATION(FText::Format(LOCTEXT("DuplicatedPinLabels", "Duplicate pin label '{0}', all labels must be unique."), FText::FromName(PinLabel)));
					return true;
				}

				bFoundPinLabel = true;
			}

			return false;
		};

		for (const FPCGPinProperties& PinProps : InPins)
		{
			if (IsAlreadyFound(PinProps))
			{
				return false;
			}
		}

		for (const FPCGPinProperties& PinProps : OutPins)
		{
			if (IsAlreadyFound(PinProps))
			{
				return false;
			}
		}

		return true;
	};

	// Validate input pins
	bool bIsFirstInputPin = true;
	for (const FPCGPinProperties& Properties : InputPins)
	{
		if (!CheckPinLabel(Properties.Label))
		{
			return false;
		}

		if (bIsFirstInputPin && KernelType == EPCGKernelType::PointProcessor)
		{
			if (Properties.AllowedTypes != EPCGDataType::Point)
			{
				PCG_LOG_VALIDATION(FText::Format(
					LOCTEXT("InvalidNonPointPrimaryInput", "'Point Processor' nodes require primary input pin to be of type 'Point', but found '{0}'."),
					FText::FromString(PCGHLSLElement::GetDataTypeString(Properties.AllowedTypes))));

				return false;
			}
		}

		if (!PCGComputeHelpers::IsTypeAllowedAsInput(Properties.AllowedTypes))
		{
			PCG_LOG_VALIDATION(FText::Format(
				LOCTEXT("InvalidInputType", "Unsupported input type '{0}', found on pin '{1}'."),
				FText::FromString(PCGHLSLElement::GetDataTypeString(Properties.AllowedTypes)),
				FText::FromName(Properties.Label)));

			return false;
		}

		bIsFirstInputPin = false;
	}

	// Validate output pins
	bool bIsFirstOutputPin = true;
	for (const FPCGPinPropertiesGPU& Properties : OutputPins)
	{
		if (!CheckPinLabel(Properties.Label))
		{
			return false;
		}

		const bool bPinIsDefinedByKernel = bIsFirstOutputPin && (KernelType == EPCGKernelType::PointGenerator || KernelType == EPCGKernelType::PointProcessor);

		if (bPinIsDefinedByKernel)
		{
			if (Properties.AllowedTypes != EPCGDataType::Point)
			{
				PCG_LOG_VALIDATION(FText::Format(
					LOCTEXT("InvalidNonPointPrimaryOutput", "'Point Processor' and 'Point Generator' nodes require primary output pin to be of type 'Point', but found '{0}'."),
					FText::FromString(PCGHLSLElement::GetDataTypeString(Properties.AllowedTypes))));

				return false;
			}
		}

		if (!PCGComputeHelpers::IsTypeAllowedAsOutput(Properties.AllowedTypes))
		{
			PCG_LOG_VALIDATION(FText::Format(
				LOCTEXT("InvalidOutputType", "Unsupported output type '{0}', found on pin '{1}'."),
				FText::FromString(PCGHLSLElement::GetDataTypeString(Properties.AllowedTypes)),
				FText::FromName(Properties.Label)));

			return false;
		}

		if (!bPinIsDefinedByKernel)
		{
			if (Properties.BufferSizeMode == EPCGPinBufferSizeMode::FixedElementCount)
			{
				if (Properties.FixedBufferElementCount <= 0)
				{
					PCG_LOG_VALIDATION(FText::Format(
						LOCTEXT("InvalidFixedBufferSize", "Fixed GPU buffer size on '{0}' was invalid (%d)."),
						FText::FromName(Properties.Label),
						Properties.FixedBufferElementCount));

					return false;
				}

				if (Properties.AllowedTypes == EPCGDataType::Param && Properties.InitializeFromPin != NAME_None && !GetInputPin(Properties.InitializeFromPin))
				{
					PCG_LOG_VALIDATION(FText::Format(
						LOCTEXT("InvalidInitFromPin", "Tried to initialize attribute set pin '{0}' from non-existent pin '{1}'. Must reference a valid input pin or be 'None'."),
						FText::FromName(Properties.Label),
						FText::FromName(Properties.InitializeFromPin)));

					return false;
				}
			}
			else if (Properties.BufferSizeMode == EPCGPinBufferSizeMode::FromFirstPin)
			{
				if (InputPins.IsEmpty())
				{
					PCG_LOG_VALIDATION(FText::Format(
						LOCTEXT("InvalidBufferSizeNoInputPin", "GPU buffer size for pin '{0}' could not be computed as there are no input pins."),
						FText::FromName(Properties.Label)));

					return false;
				}

				if (!GetFirstInputPin())
				{
					PCG_LOG_VALIDATION(FText::Format(
						LOCTEXT("MissingPrimaryInputPin", "GPU buffer size for pin '{0}' could not be computed, because it refers to the primary input pin, which does not exist."),
						FText::FromName(Properties.Label)));

					return false;
				}
			}
			else if (Properties.BufferSizeMode == EPCGPinBufferSizeMode::FromProductOfInputPins)
			{
				if (InputPins.IsEmpty())
				{
					PCG_LOG_VALIDATION(FText::Format(
						LOCTEXT("InvalidBufferSizeNoInputPins", "GPU buffer size for pin '{0}' could not be computed as there are no input pins on this node."),
						FText::FromName(Properties.Label)));

					return false;
				}

				if (Properties.BufferSizeInputPinLabels.IsEmpty())
				{
					PCG_LOG_VALIDATION(FText::Format(
						LOCTEXT("InvalidBufferSizeNoBufferPins", "GPU buffer size for pin '{0}' could not be computed as input pins are specified in the pin settings."),
						FText::FromName(Properties.Label)));

					return false;
				}

				for (const FName& Label : Properties.BufferSizeInputPinLabels)
				{
					if (!GetInputPin(Label))
					{
						PCG_LOG_VALIDATION(FText::Format(
							LOCTEXT("MissingBufferSizePin", "GPU buffer size for pin '{0}' could not be computed. Invalid pin specified in Input Pins array: '{1}'."),
							FText::FromName(Properties.Label),
							FText::FromName(Label)));

						return false;
					}
				}
			}
		}

		bIsFirstOutputPin = false;
	}

	if (KernelType == EPCGKernelType::Custom && DispatchThreadCount == EPCGDispatchThreadCount::FromProductOfInputPins)
	{
		if (ThreadCountInputPinLabels.IsEmpty())
		{
			PCG_LOG_VALIDATION(LOCTEXT("MissingThreadCountPins", "Dispatch thread count is based on input pins but no labels have been set in Input Pins array."));
			return false;
		}

		for (const FName& Label : ThreadCountInputPinLabels)
		{
			if (!GetInputPin(Label))
			{
				PCG_LOG_VALIDATION(FText::Format(LOCTEXT("MissingThreadCountPin", "Invalid pin specified in Input Pins array: '{0}'."), FText::FromName(Label)));
				return false;
			}
		}
	}

	if (IsThreadCountMultiplierInUse())
	{
		if (ThreadCountMultiplier < 1)
		{
			PCG_LOG_VALIDATION(FText::Format(LOCTEXT("InvalidThreadCountMultiplier", "Thread Count Multiplier has invalid value ({0}). Must be greater than 0."), ThreadCountMultiplier));
			return false;
		}
	}

	// Validate attributes
	for (const FPCGKernelAttributeKey& AttributeKey : KernelAttributeKeys)
	{
		if (AttributeKey.Name == NAME_None)
		{
			PCG_LOG_VALIDATION(LOCTEXT("InvalidAttributeNameNone", "'None' is not a valid GPU attribute name, check the 'Attributes to Create' array on your pins."));
			return false;
		}
	}

	if (InContext)
	{
		FText* ErrorTextPtr = nullptr;
#if PCG_LOGGING_ENABLED
		FText ErrorText;
		ErrorTextPtr = &ErrorText;
#endif

		if (!AreKernelAttributesValid(InContext, ErrorTextPtr))
		{
			if (ErrorTextPtr)
			{
				PCG_LOG_VALIDATION(*ErrorTextPtr);
			}
			return false;
		}
	}

	return true;
}

bool UPCGCustomHLSLSettings::AreKernelAttributesValid(FPCGContext* InContext, FText* OutErrorText) const
{
	// The context can either be a compute graph element context (if the compute graph was successfully created), otherwise
	// it will be the original CPU node context. We need the former to run the following validation.
	if (!InContext || !InContext->IsComputeContext())
	{
		return true;
	}

	const FPCGComputeGraphContext* Context = static_cast<FPCGComputeGraphContext*>(InContext);
	const UPCGDataBinding* DataBinding = Context->DataBinding.Get();
	const UPCGNode* Node = Cast<UPCGNode>(GetOuter());

	if (DataBinding && Node)
	{
		const TArray<TObjectPtr<UPCGPin>>& InPins = Node->GetInputPins();
		const TArray<TObjectPtr<UPCGPin>>& OutPins = Node->GetOutputPins();

		TMap<FName, FPCGDataCollectionDesc> InputPinDescs;
		TMap<FName, FPCGDataCollectionDesc> OutputPinDescs;

		for (const UPCGPin* InputPin : InPins)
		{
			check(InputPin);
			InputPinDescs.Add(InputPin->Properties.Label, ComputeInputPinDataDesc(InputPin, DataBinding));
		}

		for (const UPCGPin* OutputPin : OutPins)
		{
			check(OutputPin);
			OutputPinDescs.Add(OutputPin->Properties.Label, ComputeOutputPinDataDesc(OutputPin, DataBinding));
		}

		const FString Source = ShaderFunctions + ShaderSource;
		FRegexMatcher ModuleMatcher(FRegexPattern(PCGHLSLElement::AttributeFunctionPattern), Source);

		while (ModuleMatcher.FindNext())
		{
			const FString PinStr = ModuleMatcher.GetCaptureGroup(PCGHLSLElement::AttributePinCaptureGroup);
			const FString FuncStr = ModuleMatcher.GetCaptureGroup(PCGHLSLElement::AttributeFunctionCaptureGroup);
			const FString TypeStr = ModuleMatcher.GetCaptureGroup(PCGHLSLElement::AttributeTypeCaptureGroup);
			const FString NameStr = ModuleMatcher.GetCaptureGroup(PCGHLSLElement::AttributeNameCaptureGroup);

			const FName PinName = FName(*PinStr);
			const FPCGDataCollectionDesc* PinDesc = nullptr;

			auto ConstructFunctionText = [&PinStr, &FuncStr, &TypeStr]()
			{
				return FText::FromString(PinStr + TEXT("_") + FuncStr + TypeStr);
			};

			if (FuncStr == PCGHLSLElement::AttributeFunctionSet)
			{
				PinDesc = OutputPinDescs.Find(FName(*PinStr));

				if (!PinDesc && InputPinDescs.Find(PinName))
				{
#if PCG_LOGGING_ENABLED
					if (OutErrorText)
					{
						*OutErrorText = FText::Format(
							LOCTEXT("InvalidSetAttributeUsage", "Tried to call attribute function '{0}' on read-only input pin '{1}'."),
							ConstructFunctionText(),
							FText::FromName(PinName));
					}
#endif

					return false;
				}
			}
			else if (ensure(FuncStr == PCGHLSLElement::AttributeFunctionGet))
			{
				PinDesc = InputPinDescs.Find(FName(*PinStr));
			}

			if (!PinDesc)
			{
#if PCG_LOGGING_ENABLED
				if (OutErrorText)
				{
					*OutErrorText = FText::Format(
						LOCTEXT("InvalidAttributePinName", "Tried to call attribute function '{0}' on non-existent pin '{1}'."),
						ConstructFunctionText(),
						FText::FromName(PinName));
				}
#endif

				return false;
			}

			const UEnum* AttributeTypeEnum = StaticEnum<EPCGKernelAttributeType>();
			check(AttributeTypeEnum);

			const int64 AttributeType = AttributeTypeEnum->GetValueByName(FName(*TypeStr));

			if (AttributeType == INDEX_NONE)
			{
#if PCG_LOGGING_ENABLED
				if (OutErrorText)
				{
					*OutErrorText = FText::Format(
						LOCTEXT("InvalidAttributePinType", "Tried to call attribute function '{0}' on non-existent type '{1}'."),
						ConstructFunctionText(),
						FText::FromString(TypeStr));
				}
#endif

				return false;
			}

			const FPCGKernelAttributeDesc* AttrDesc = nullptr;
			const FName AttrName = FName(*NameStr);

			if (!PinDesc->DataDescs.IsEmpty())
			{
				// Note: This assumes attributes are the same on all data on a pin, which is true for now
				const FPCGDataDesc& DataDesc = PinDesc->DataDescs[0];
				AttrDesc = DataDesc.AttributeDescs.FindByPredicate([AttrName](const FPCGKernelAttributeDesc& Desc) { return Desc.Name == AttrName; });
			}

			if (!AttrDesc)
			{
#if PCG_LOGGING_ENABLED
				if (OutErrorText)
				{
					*OutErrorText = FText::Format(
						LOCTEXT("InvalidAttributeName", "Tried to call attribute function '{0}' on attribute '{1}' which does not exist."),
						ConstructFunctionText(),
						FText::FromName(AttrName));
				}
#endif

				return false;
			}

			if (AttrDesc->Type != static_cast<EPCGKernelAttributeType>(AttributeType))
			{
#if PCG_LOGGING_ENABLED
				if (OutErrorText)
				{
					const FString ActualTypeStr = AttributeTypeEnum->GetNameStringByIndex(static_cast<int64>(AttrDesc->Type));

					*OutErrorText = FText::Format(
						LOCTEXT("AttributeTypeMismatch", "Type mismatch for call to attribute function '{0}' on attribute '{1}'. Expected '{2}' but received '{3}'."),
						ConstructFunctionText(),
						FText::FromName(AttrName),
						FText::FromString(TypeStr),
						FText::FromString(ActualTypeStr));
				}
#endif

				return false;
			}
		}
	}

	return true;
}

FString UPCGCustomHLSLSettings::GetCookedKernelSource(const TMap<FPCGKernelAttributeKey, int32>& GlobalAttributeLookupTable) const
{
	const FIntVector GroupSize = GetThreadGroupSize();

	// FIXME: Create source range mappings so that we can go from error location to our source.
	FString Source = ShaderSource;
	FString Functions = (TEXT("// User kernel functions\n") + ShaderFunctions);

#if PLATFORM_WINDOWS
	// Remove old-school stuff.
	Source.ReplaceInline(TEXT("\r"), TEXT(""));
	Functions.ReplaceInline(TEXT("\r"), TEXT(""));
#endif

	Source.ReplaceInline(TEXT("\n"), TEXT("\n    ")); // Properly indent kernel source

	for (const TPair<FPCGKernelAttributeKey, int32>& Pair : GlobalAttributeLookupTable)
	{
		const FString SourceDefinition = PCGHLSLElement::GetKernelAttributeKeyAsString(Pair.Key);
		const FString AttributeIndexAsString = FString::FromInt(Pair.Value);

		Source.ReplaceInline(*SourceDefinition, *AttributeIndexAsString);
		Functions.ReplaceInline(*SourceDefinition, *AttributeIndexAsString);
	}

	FString ShaderPathName = GetPathName();
	PCGHLSLElement::ConvertObjectPathToShaderFilePath(ShaderPathName);

	const bool bHasKernelKeyword = Source.Contains(TEXT("KERNEL"), ESearchCase::CaseSensitive);

	FString Includes;
	{
		// Add with caution: Pulling in external includes has the danger that 1) we may pull in more than we expect
		// if they include additional things in the future, and 2) if their functions change it could break user's
		// kernel source. The latter may be mitigated by branching our own PCG version of these if needed in the future.

		// TODO bring these in via additional sources so that their contents get hashed. Also these could generate declarations.
		Includes += TEXT("#include \"/Engine/Private/ComputeShaderUtils.ush\"\n");
		Includes += TEXT("#include \"/Engine/Private/Quaternion.ush\"\n");
		Includes += TEXT("#include \"/Plugin/PCG/Private/PCGShaderUtils.ush\"\n");
	}

	const FString KernelFunc = FString::Printf(
		TEXT("[numthreads(%d, %d, %d)]\nvoid %s(uint3 GroupId : SV_GroupID, uint GroupIndex : SV_GroupIndex)"),
		GroupSize.X, GroupSize.Y, GroupSize.Z, *GetKernelEntryPoint());

	const FString UnWrappedDispatchThreadId = FString::Printf(
		TEXT("GetUnWrappedDispatchThreadId(GroupId, GroupIndex, %d)"),
		GroupSize.X * GroupSize.Y * GroupSize.Z
	);

	// Header writers initialize PCG data collection format headers in output buffers.
	FString HeaderWriters;

	auto EmitHeaderWriterSingleData = [&HeaderWriters](const FPCGPinProperties& InOutputPinProps)
	{
		HeaderWriters += FString::Format(TEXT(
			"    // Signal kernel executed by setting data count from first thread. Rest of header was already set up by the CPU.\n"
			"    if (GroupIndex == 0) {0}_SetNumDataInternal(1);\n"
			"    AllMemoryBarrier();\n"),
			{ InOutputPinProps.Label.ToString() });
	};

	auto EmitHeaderWriterFromInputPin = [&HeaderWriters](const FPCGPinProperties& InOutputPinProps, const UPCGPin* InFromPin)
	{
		check(InFromPin);
		HeaderWriters += FString::Format(TEXT(
			"    // Signal kernel executed by copying data count from pin {0} to pin {1} from first thread. Rest of header was already set up by the CPU.\n"
			"    if (GroupIndex == 0) {1}_SetNumDataInternal({0}_GetNumData());\n"
			"    AllMemoryBarrier();\n"),
			{ InFromPin->Properties.Label.ToString(), InOutputPinProps.Label.ToString() });
	};

	if (KernelType == EPCGKernelType::PointProcessor || KernelType == EPCGKernelType::Custom)
	{
		const UPCGPin* FirstPin = (KernelType == EPCGKernelType::PointProcessor)
			? GetPointProcessingInputPin()
			: CastChecked<UPCGNode>(GetOuter())->GetPassThroughInputPin();

		// Initialize all output headers.
		for (const FPCGPinPropertiesGPU& PinProps : OutputPins)
		{
			if (PinProps.BufferSizeMode == EPCGPinBufferSizeMode::FromFirstPin && FirstPin)
			{
				EmitHeaderWriterFromInputPin(PinProps, FirstPin);
			}
			else if (PinProps.BufferSizeMode == EPCGPinBufferSizeMode::FixedElementCount)
			{
				const UPCGPin* InitFromPin = (PinProps.AllowedTypes == EPCGDataType::Param) ? GetInputPin(PinProps.InitializeFromPin) : nullptr;

				if (InitFromPin)
				{
					EmitHeaderWriterFromInputPin(PinProps, InitFromPin);
				}
				else
				{
					EmitHeaderWriterSingleData(PinProps);
				}
			}
			else if (PinProps.BufferSizeMode == EPCGPinBufferSizeMode::FromProductOfInputPins)
			{
				// TODO: FromProductOfInputPins always produces a single point data for now, make it more flexible?
				EmitHeaderWriterSingleData(PinProps);
			}
		}
	}
	else if (KernelType == EPCGKernelType::PointGenerator)
	{
		if (const UPCGNode* Node = Cast<UPCGNode>(GetOuter()))
		{
			const UPCGPin* FirstPin = Node->GetPassThroughInputPin();
			const UPCGPin* PrimaryOutputPin = GetFirstPointOutputPin();

			for (const UPCGPin* OutputPin : Node->GetOutputPins())
			{
				if (!OutputPin || !PCGComputeHelpers::IsTypeAllowedInDataCollection(OutputPin->Properties.AllowedTypes))
				{
					continue;
				}

				const FPCGPinPropertiesGPU* PinPropsGPU = GetOutputPinPropertiesGPU(OutputPin->Properties.Label);
				if (!ensure(PinPropsGPU))
				{
					continue;
				}

				const FPCGPinProperties& PinProps = OutputPin->Properties;

				if (OutputPin == PrimaryOutputPin)
				{
					EmitHeaderWriterSingleData(PinProps);
				}
				else if (PinPropsGPU->BufferSizeMode == EPCGPinBufferSizeMode::FromFirstPin && FirstPin)
				{
					EmitHeaderWriterFromInputPin(PinProps, FirstPin);
				}
				else if (PinPropsGPU->AllowedTypes == EPCGDataType::Param)
				{
					if (const UPCGPin* InitFromPin = GetInputPin(PinPropsGPU->InitializeFromPin))
					{
						EmitHeaderWriterFromInputPin(PinProps, InitFromPin);
					}
					else
					{
						EmitHeaderWriterSingleData(PinProps);
					}
				}
			}
		}
	}
	else
	{
		checkNoEntry();
	}

	// Per-kernel-type preamble. Set up shader inputs and initialize output data.
	FString KernelSpecificPreamble = TEXT("    // Kernel preamble\n");
	
	auto AddThreadInfoForPin = [&KernelSpecificPreamble](FName PinLabel)
	{
		KernelSpecificPreamble += FString::Format(TEXT(
			"    uint {0}_DataIndex;\n"
			"    uint {0}_DataAddress;\n"
			"    if (!{0}_GetThreadData(ThreadIndex, {0}_DataIndex, {0}_DataAddress, ElementIndex)) return;\n"),
			{ PinLabel.ToString() });
	};

	if (KernelType == EPCGKernelType::PointProcessor)
	{
		const UPCGPin* InputPin = GetPointProcessingInputPin();
		const UPCGPin* OutputPin = GetFirstPointOutputPin();

		if (InputPin && OutputPin)
		{
			KernelSpecificPreamble += TEXT("    uint ElementIndex; // Assumption - element index identical in input and output data.\n");

			AddThreadInfoForPin(InputPin->Properties.Label);
			AddThreadInfoForPin(OutputPin->Properties.Label);

			// If input point is invalid, mark output point as invalid and abort.
			KernelSpecificPreamble += FString::Format(TEXT(
				"    if (!{0}_IsValid({0}_DataIndex, ElementIndex))\n"
				"    {\n"
				"        {1}_RemovePoint({1}_DataIndex, ElementIndex);\n"
				"        return;\n"
				"    }\n"),
				{ InputPin->Properties.Label.ToString(), OutputPin->Properties.Label.ToString() });

			// Automatically copy value of all attributes for this element.
			KernelSpecificPreamble += FString::Format(TEXT(
				"\n"
				"    // Loop over all attribute headers, if the address is non-zero, then copy it from pin {0} to pin {1}.\n"
				"    {\n"
				"        const uint HeadersAddress = {0}_GetDataAttributeHeadersAddress({0}_DataAddress);\n"
				"        const uint NumAttributes = {0}_GetDataNumAttributes({0}_DataAddress);\n"
				"        uint NumAttributesProcessed = 0;\n"
				"\n"
				"        for (int AttributeIndex = 0; AttributeIndex < 128; ++AttributeIndex)\n"
				"        {\n"
				"            const uint HeaderAddress = {0}_GetAttributeHeaderAddress(HeadersAddress, AttributeIndex);\n"
				"            const uint Stride = {0}_GetAttributeStride(HeaderAddress);\n"
				"            const uint Address = {0}_GetAttributeAddress(HeaderAddress);\n"
				"\n"
				"            if (Address != 0)\n"
				"            {\n"
				"                const uint BaseElementAddress = Address + ElementIndex * Stride;\n"
				"\n"
				"                for (int I = 0; I < Stride; I += 4)\n"
				"                {\n"
				"                    const uint ElementAddress = BaseElementAddress + I;\n"
				"                    {1}_StoreBufferInternal(ElementAddress, {0}_LoadBufferInternal(ElementAddress));\n"
				"                }\n"
				"\n"
				"                if (++NumAttributesProcessed >= NumAttributes) break; // We can early-out when we've looked at all the possible attributes\n"
				"            }\n"
				"        }\n"
				"    }\n"),
				{ InputPin->Properties.Label.ToString(), OutputPin->Properties.Label.ToString() });
		}
	}
	else if (KernelType == EPCGKernelType::PointGenerator)
	{
		KernelSpecificPreamble += FString::Format(TEXT("    const uint PointCount = {0};\n"), { PointCount });

		if (const UPCGPin* OutputPin = GetFirstPointOutputPin())
		{
			KernelSpecificPreamble += TEXT("uint ElementIndex; // Assumption - element index identical in input and output data.\n");

			AddThreadInfoForPin(OutputPin->Properties.Label);

			KernelSpecificPreamble += FString::Format(TEXT(
				"    \n"
				"    // Initialize all values to defaults for output pin {0}\n"
				"    {0}_InitializePoint({0}_DataIndex, ElementIndex);\n"),
				{ OutputPin->Properties.Label.ToString() });
		}
	}

	FString Result;

	if (bHasKernelKeyword)
	{
		Source.ReplaceInline(TEXT("KERNEL"), TEXT("void __kernel_func(uint ThreadIndex)"), ESearchCase::CaseSensitive);

		Result = FString::Printf(TEXT(
			"#line 0 \"%s\"\n" // ShaderPathName
			"%s\n" // Includes
			"%s\n" // Functions
			"%s\n" // Source
			"%s { __kernel_func(%s); }\n"), // KernelFunc, UnWrappedDispatchThreadId
			*ShaderPathName, *Includes, *Functions, *Source, *KernelFunc, *UnWrappedDispatchThreadId);
	}
	else
	{
		Result = FString::Printf(TEXT(
			"%s\n\n" // Includes
			"%s\n\n" // Functions
			"%s\n" // KernelFunc
			"{\n"
			"	const uint ThreadIndex = %s;\n" // UnWrappedDispatchThreadId
			"	if (ThreadIndex >= GetNumThreads().x) return;\n"
			"%s\n" // HeaderWriters
			"%s\n" // KernelSpecificPreamble
			"#line 0 \"%s\"\n" // ShaderPathName
			"%s\n" // Source
			"}\n"),
			*Includes, *Functions, *KernelFunc, *UnWrappedDispatchThreadId, *HeaderWriters, *KernelSpecificPreamble, *ShaderPathName, *Source);
	}

	if (bDumpCookedHLSL)
	{
		UE_LOG(LogPCG, Log, TEXT("Cooked HLSL:\n%s\n"), *Result);
	}

	return Result;
}

bool FPCGCustomHLSLElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	const UPCGCustomHLSLSettings* Settings = Context->GetInputSettings<UPCGCustomHLSLSettings>();
	check(Settings);

	Settings->IsKernelValid(Context, /*bQuiet=*/false);

	return true;
}

#undef PCG_LOG_VALIDATION
#undef PCG_LOGGING_ENABLED
#undef LOCTEXT_NAMESPACE
