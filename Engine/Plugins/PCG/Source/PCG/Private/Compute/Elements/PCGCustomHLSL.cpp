// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/Elements/PCGCustomHLSL.h"

#include "PCGContext.h"
#include "PCGEdge.h"
#include "PCGModule.h"
#include "PCGPoint.h"
#include "Compute/PCGComputeCommon.h"
#include "Compute/PCGComputeGraph.h"
#include "Compute/PCGDataBinding.h"
#include "Data/PCGPointData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGCustomHLSL)

#define LOCTEXT_NAMESPACE "PCGCustomHLSLElement"

namespace PCGHLSLElement
{
	const FString KernelAttributeToken = TEXT("@");

	void ConvertObjectPathToShaderFilePath(FString& InOutPath)
	{
		// Shader compiler recognizes "/Engine/Generated/..." path as special. 
		// It doesn't validate file suffix etc.
		InOutPath = FString::Printf(TEXT("/Engine/Generated/UObject%s.ush"), *InOutPath);
		// Shader compilation result parsing will break if it finds ':' where it doesn't expect.
		InOutPath.ReplaceCharInline(TEXT(':'), TEXT('@'));
	}

	int GetElementCount(const UPCGData* InData)
	{
		if (const UPCGPointData* PointData = Cast<UPCGPointData>(InData))
		{
			return PointData->GetPoints().Num();
		}
		else if (InData)
		{
			// TODO - spline points, other things with multiple elements?
			return 1;
		}
		else
		{
			return 0;
		}
	}

	FString GetKernelAttributeKeyAsString(const FPCGKernelAttributeKey& Key)
	{
		return FString::Format(TEXT("{0}{1}"), { KernelAttributeToken, Key.Name.ToString()});
	}
}

#if WITH_EDITOR
void UPCGCustomHLSLSettings::PostLoad()
{
	Super::PostLoad();

	UpdatePinSettings();
}

void UPCGCustomHLSLSettings::PostInitProperties()
{
	Super::PostInitProperties();

	UpdatePinSettings();

	UpdateDeclarations();
}
#endif

bool UPCGCustomHLSLSettings::ShouldExecuteOnGPU() const
{
	return IsKernelValid();
}

#if WITH_EDITOR
void UPCGCustomHLSLSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Apply any pin setup before refreshing the node.
	UpdatePinSettings();

	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdateDeclarations();
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
			ProcessingElemCount += PCGHLSLElement::GetElementCount(Data.Data);
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

			// Only support Point data right now.
			ProcessingElemCount += PinDesc.ComputeDataElementCount(EPCGDataType::Point);
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
		auto GetFromFirstInput = [this, Binding]()
		{
			const UPCGPin* InputPin = GetPointProcessingInputPin();
			return GetProcessingElemCountForInputPin(InputPin, Binding);
		};

		auto GetFromSecondInput = [this, Binding]()
		{
			const UPCGPin* InputPin = GetSecondPointProcessingInputPin();
			return GetProcessingElemCountForInputPin(InputPin, Binding);
		};

		auto GetFromFirstInputXSecondInput = [this, Binding]()
		{
			const UPCGPin* FirstPin = GetPointProcessingInputPin();
			const int FirstElemCount = GetProcessingElemCountForInputPin(FirstPin, Binding);
			const UPCGPin* SecondPin = GetSecondPointProcessingInputPin();
			const int SecondElemCount = GetProcessingElemCountForInputPin(SecondPin, Binding);
			return FirstElemCount * SecondElemCount;
		};

		if (DispatchThreadCount == EPCGDispatchThreadCount::FromOutput)
		{
			const UPCGPin* OutputPin = GetFirstOutputPin();

			if (OutputPin->Properties.BufferSizeMode == EPCGPinBufferSizeMode::FixedElementCount)
			{
				ThreadCount = OutputPin->Properties.FixedBufferElementCount;
			}
			else if (OutputPin->Properties.BufferSizeMode == EPCGPinBufferSizeMode::FromFirstPin)
			{
				ThreadCount = GetFromFirstInput();
			}
			else if (OutputPin->Properties.BufferSizeMode == EPCGPinBufferSizeMode::FromSecondPin)
			{
				ThreadCount = GetFromSecondInput();
			}
			else if (OutputPin->Properties.BufferSizeMode == EPCGPinBufferSizeMode::FirstPinXSecondPin)
			{
				ThreadCount = GetFromFirstInputXSecondInput();
			}
		}
		else if (DispatchThreadCount == EPCGDispatchThreadCount::FromFirstInput)
		{
			ThreadCount = GetFromFirstInput();
		}
		else if (DispatchThreadCount == EPCGDispatchThreadCount::FromSecondInput)
		{
			ThreadCount = GetFromSecondInput();
		}
		else if (DispatchThreadCount == EPCGDispatchThreadCount::FirstInputXSecondInput)
		{
			ThreadCount = GetFromFirstInputXSecondInput();
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

	return ThreadCount;
}

TArray<FPCGKernelAttributeKey> UPCGCustomHLSLSettings::GetKernelAttributeKeys() const
{
	TArray<FPCGKernelAttributeKey> Keys;

	const FString SourceToParse = ShaderFunctions + ShaderSource;

	int NumExistingAttributes = PCGComputeConstants::NUM_RESERVED_ATTRS;
	int AttributeSourceIndex = SourceToParse.Find(PCGHLSLElement::KernelAttributeToken);

	while (AttributeSourceIndex != INDEX_NONE)
	{
		const int GetStringIndex = SourceToParse.Find(TEXT("Get"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, AttributeSourceIndex);
		const int SetStringIndex = SourceToParse.Find(TEXT("Set"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, AttributeSourceIndex);
		const int StartTypeStringIndex = FMath::Max(GetStringIndex, SetStringIndex) + 3;
		const int EndTypeStringIndex = SourceToParse.Find(TEXT("("), ESearchCase::CaseSensitive, ESearchDir::FromStart, StartTypeStringIndex);

		const FString TypeString = (StartTypeStringIndex != INDEX_NONE || EndTypeStringIndex != INDEX_NONE)
			? SourceToParse.Mid(StartTypeStringIndex, EndTypeStringIndex - StartTypeStringIndex)
			: TEXT("");

		if (TypeString.IsEmpty())
		{
			UE_LOG(LogPCG, Error, TEXT("Read invalid attribute type in shader source.")); 
		}

		EPCGKernelAttributeType Type = EPCGKernelAttributeType::Bool;

		if (TypeString == TEXT("Bool"))
		{
			Type = EPCGKernelAttributeType::Bool;
		}
		else if (TypeString == TEXT("Int"))
		{
			Type = EPCGKernelAttributeType::Int;
		}
		else if (TypeString == TEXT("Float"))
		{
			Type = EPCGKernelAttributeType::Float;
		}
		else if (TypeString == TEXT("Float2"))
		{
			Type = EPCGKernelAttributeType::Float2;
		}
		else if (TypeString == TEXT("Float3"))
		{
			Type = EPCGKernelAttributeType::Float3;
		}
		else if (TypeString == TEXT("Float4"))
		{
			Type = EPCGKernelAttributeType::Float4;
		}
		else if (TypeString == TEXT("Rotator"))
		{
			Type = EPCGKernelAttributeType::Rotator;
		}
		else if (TypeString == TEXT("Quat"))
		{
			Type = EPCGKernelAttributeType::Quat;
		}
		else if (TypeString == TEXT("Transform"))
		{
			Type = EPCGKernelAttributeType::Transform;
		}
		else
		{
			UE_LOG(LogPCG, Error, TEXT("Read invalid attribute type in shader source."));
		}

		FString AttrNameString = "";

		// TODO: Copy the full attribute name at once with AttrNameString.Mid()
		for (int I = AttributeSourceIndex + 1; I < SourceToParse.Len(); ++I)
		{
			const char Char = SourceToParse[I];

			if (isalnum(Char))
			{
				AttrNameString += Char;
			}
			else
			{
				break;
			}
		}

		Keys.AddUnique({ Type, FName(*AttrNameString) });

		AttributeSourceIndex = SourceToParse.Find(PCGHLSLElement::KernelAttributeToken, ESearchCase::IgnoreCase, ESearchDir::FromStart, AttributeSourceIndex + 1);
	}

	return Keys;
}

FPCGDataCollectionDesc UPCGCustomHLSLSettings::ComputeOutputPinDataDesc(const UPCGPin* OutputPin, const UPCGDataBinding* Binding) const
{
	check(OutputPin);
	FPCGDataCollectionDesc PinDesc;

	// The primary output pin follows any rules prescribed by kernel type.
	if (OutputPin == GetFirstOutputPin())
	{
		if (KernelType == EPCGKernelType::PointProcessor)
		{
			// First output pin passes through first input pin.
			if (const UPCGPin* PointProcessingInputPin = GetPointProcessingInputPin())
			{
				PinDesc = ComputeInputPinDataDesc(PointProcessingInputPin, Binding);
			}

			return PinDesc;
		}
		else if (KernelType == EPCGKernelType::PointGenerator)
		{
			// Generators always produce a single point data with known point count.
			PinDesc.DataDescs.Emplace(EPCGDataType::Point, PointCount);

			// TODO: Until support for 'Create Attributes' exists, PointGenerators cannot manipulate attributes.
			return PinDesc;
		}
	}

	// No size set by kernel, fall back to pin settings.
	if (OutputPin->Properties.BufferSizeMode == EPCGPinBufferSizeMode::FromFirstPin)
	{
		if (const UPCGPin* PointProcessingInputPin = GetPointProcessingInputPin())
		{
			PinDesc = ComputeInputPinDataDesc(PointProcessingInputPin, Binding);
		}
	}
	else if (OutputPin->Properties.BufferSizeMode == EPCGPinBufferSizeMode::FromSecondPin)
	{
		if (const UPCGPin* PointProcessingInputPin = GetSecondPointProcessingInputPin())
		{
			PinDesc = ComputeInputPinDataDesc(PointProcessingInputPin, Binding);
		}
	}
	else if (OutputPin->Properties.BufferSizeMode == EPCGPinBufferSizeMode::FirstPinXSecondPin)
	{
		const UPCGPin* FirstPin = GetPointProcessingInputPin();
		const UPCGPin* SecondPin = GetSecondPointProcessingInputPin();

		if (FirstPin && SecondPin)
		{
			const FPCGDataCollectionDesc FirstPinDesc = ComputeInputPinDataDesc(FirstPin, Binding);
			const FPCGDataCollectionDesc SecondPinDesc = ComputeInputPinDataDesc(SecondPin, Binding);

			int TotalSourceElementCount = 0;
			for (const FPCGDataDesc& DataDesc : FirstPinDesc.DataDescs)
			{
				if (DataDesc.Type == EPCGDataType::Point)
				{
					TotalSourceElementCount += DataDesc.ElementCount;
				}
			}

			int TotalOutputElementCount = 0;
			for (const FPCGDataDesc& DataDesc : SecondPinDesc.DataDescs)
			{
				if (DataDesc.Type == EPCGDataType::Point)
				{
					TotalOutputElementCount += DataDesc.ElementCount * TotalSourceElementCount;
				}
			}

			PinDesc.DataDescs.Emplace(EPCGDataType::Point, TotalOutputElementCount);
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

const UPCGPin* UPCGCustomHLSLSettings::GetSecondPointProcessingInputPin() const
{
	if (const UPCGNode* Node = Cast<UPCGNode>(GetOuter()))
	{
		bool bEncounteredPointPin = false;
		for (const UPCGPin* Pin : Node->GetInputPins())
		{
			if (Pin->Properties.AllowedTypes == EPCGDataType::Point)
			{
				if (bEncounteredPointPin)
				{
					return Pin;
				}

				bEncounteredPointPin = true;
			}
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
					"// (DataIndex, DataAddress, ElementIndex). Returns -1 if invalid!\n"
					"uint3 {0}_GetThreadData(uint ThreadIndex);\n"),
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
		InputDeclarations += TEXT("uint GetNumThreads();");
	}

	// Add debug category
	{
		if (bPrintShaderDebugValues)
		{
			InputDeclarations += FString::Format(
				TEXT("\n\n")
				TEXT("// Debug\n")
				TEXT("void WriteDebugValue(uint Index, float Value); // Index in [0, {0}] (set from 'Debug Buffer Size' property)"),
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

		InputDeclarations += TEXT("\n\n### HELPER FUNCTIONS ###\n\n");
		InputDeclarations += TEXT("float3 GetComponentBoundsMin(); // World-space\n");
		InputDeclarations += TEXT("float3 GetComponentBoundsMax();");

		InputDeclarations += TEXT("\n\n");
		InputDeclarations += TEXT("float3 CreateGrid2D(int ElementIndex, int NumPoints, float3 Min, float3 Max);\n");
		InputDeclarations += TEXT("float3 CreateGrid2D(int ElementIndex, int NumPoints, int NumRows, float3 Min, float3 Max);\n");
		InputDeclarations += TEXT("float3 CreateGrid3D(int ElementIndex, int NumPoints, float3 Min, float3 Max);\n");
		InputDeclarations += TEXT("float3 CreateGrid3D(int ElementIndex, int NumPoints, int NumRows, int NumCols, float3 Min, float3 Max);");

		if (!DataPins.IsEmpty())
		{
			InputDeclarations += TEXT("\n\n### DATA FUNCTIONS ###\n\n");
			InputDeclarations += TEXT("// Valid pins: ") + FString::Join(DataPins, TEXT(", ")) + TEXT("\n");
			InputDeclarations += TEXT("// Valid types: bool, int, float, float2, float3, float4, Rotator (float3), Quat (float4), Transform (float4x4)\n");
			InputDeclarations += TEXT("\n");
			InputDeclarations += TEXT("uint <pin>_GetNumData();\n");
			InputDeclarations += TEXT("uint <pin>_GetNumElements();\n");
			InputDeclarations += TEXT("<type> <pin>_Get<type>(uint DataIndex, uint AttributeId, uint ElementIndex);");
		}

		if (!PointDataPins.IsEmpty())
		{
			InputDeclarations += TEXT("\n\n### POINT DATA FUNCTIONS ###\n\n");
			InputDeclarations += TEXT("// Valid pins: ") + FString::Join(PointDataPins, TEXT(", ")) + TEXT("\n");
			InputDeclarations += TEXT("\n");
			InputDeclarations += TEXT("uint <pin>_GetNumPoints(uint DataIndex);\n");
			InputDeclarations += TEXT("float3 <pin>_GetPosition(uint DataIndex, uint ElementIndex);\n");
			InputDeclarations += TEXT("float4 <pin>_GetRotation(uint DataIndex, uint ElementIndex);\n");
			InputDeclarations += TEXT("float3 <pin>_GetScale(uint DataIndex, uint ElementIndex);\n");
			InputDeclarations += TEXT("float3 <pin>_GetBoundsMin(uint DataIndex, uint ElementIndex);\n");
			InputDeclarations += TEXT("float3 <pin>_GetBoundsMax(uint DataIndex, uint ElementIndex);\n");
			InputDeclarations += TEXT("float3 <pin>_GetColor(uint DataIndex, uint ElementIndex);\n");
			InputDeclarations += TEXT("float <pin>_GetDensity(uint DataIndex, uint ElementIndex);\n");
			InputDeclarations += TEXT("int <pin>_GetSeed(uint DataIndex, uint ElementIndex);\n");
			InputDeclarations += TEXT("float <pin>_GetSteepness(uint DataIndex, uint ElementIndex);\n");
			InputDeclarations += TEXT("float4x4 <pin>_GetPointTransform(uint DataIndex, uint ElementIndex);");
		}

		if (!LandscapeDataPins.IsEmpty())
		{
			InputDeclarations += TEXT("\n\n### LANDSCAPE DATA FUNCTIONS ###\n\n");
			InputDeclarations += TEXT("// Valid pins: ") + FString::Join(LandscapeDataPins, TEXT(", ")) + TEXT("\n");
			InputDeclarations += TEXT("\n");
			InputDeclarations += TEXT("float <pin>_GetHeight(float3 WorldPos);\n");
			InputDeclarations += TEXT("float3 <pin>_GetNormal(float3 WorldPos);");
		}

		if (!TextureDataPins.IsEmpty())
		{
			InputDeclarations += TEXT("\n\n### TEXTURE DATA FUNCTIONS ###\n\n");
			InputDeclarations += TEXT("// Valid pins: ") + FString::Join(TextureDataPins, TEXT(", ")) + TEXT("\n");
			InputDeclarations += TEXT("\n");
			InputDeclarations += TEXT("float2 <pin>_GetTexCoords(float2 WorldPos, float2 Min, float2 Max);\n");
			InputDeclarations += TEXT("float4 <pin>_Sample(float2 TexCoords);");
		}

		if (!RawBufferDataPins.IsEmpty())
		{
			InputDeclarations += TEXT("\n\n### BYTE ADDRESS BUFFER DATA FUNCTIONS ###\n\n");
			InputDeclarations += TEXT("// Valid pins: ") + FString::Join(RawBufferDataPins, TEXT(", ")) + TEXT("\n");
			InputDeclarations += TEXT("\n");
			InputDeclarations += TEXT("uint <pin>_ReadNumValues();\n");
			InputDeclarations += TEXT("uint <pin>_ReadValue(uint Index);");
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
			OutputDeclarations += TEXT("### DATA FUNCTIONS ###\n\n");
			OutputDeclarations += TEXT("// Valid pins: ") + FString::Join(DataPins, TEXT(", ")) + TEXT("\n");
			OutputDeclarations += TEXT("// Valid types: bool, int, float, float2, float3, float4, Rotator (float3), Quat (float4), Transform (float4x4)\n");
			OutputDeclarations += TEXT("\n");
			OutputDeclarations += TEXT("void <pin>_Set<type>(uint DataIndex, uint AttributeId, uint ElementIndex, <type> Value);");
		}

		if (!PointDataPins.IsEmpty())
		{
			OutputDeclarations += TEXT("\n\n### POINT DATA FUNCTIONS ###\n\n");
			OutputDeclarations += TEXT("// Valid pins: ") + FString::Join(PointDataPins, TEXT(", ")) + TEXT("\n");
			OutputDeclarations += TEXT("\n");
			OutputDeclarations += TEXT("void <pin>_SetPosition(uint DataIndex, uint ElementIndex, float3 Position);\n");
			OutputDeclarations += TEXT("void <pin>_SetRotation(uint DataIndex, uint ElementIndex, float4 Rotation);\n");
			OutputDeclarations += TEXT("void <pin>_SetScale(uint DataIndex, uint ElementIndex, float3 Scale);\n");
			OutputDeclarations += TEXT("void <pin>_SetBoundsMin(uint DataIndex, uint ElementIndex, float3 BoundsMin);\n");
			OutputDeclarations += TEXT("void <pin>_SetBoundsMax(uint DataIndex, uint ElementIndex, float3 BoundsMax);\n");
			OutputDeclarations += TEXT("void <pin>_SetColor(uint DataIndex, uint ElementIndex, float4 Color);\n");
			OutputDeclarations += TEXT("void <pin>_SetDensity(uint DataIndex, uint ElementIndex, float Density);\n");
			OutputDeclarations += TEXT("void <pin>_SetSeed(uint DataIndex, uint ElementIndex, int Seed);\n");
			OutputDeclarations += TEXT("void <pin>_SetSeedFromPosition(uint DataIndex, uint ElementIndex, float3 Position);\n");
			OutputDeclarations += TEXT("void <pin>_SetSteepness(uint DataIndex, uint ElementIndex, float Steepness);\n");
			OutputDeclarations += TEXT("void <pin>_SetPointTransform(uint DataIndex, uint ElementIndex, float4x4 Transform);");
		}

		if (!RawBufferDataPins.IsEmpty())
		{
			OutputDeclarations += TEXT("\n\n### BYTE ADDRESS BUFFER DATA FUNCTIONS ###\n\n");
			OutputDeclarations += TEXT("// Valid pins: ") + FString::Join(RawBufferDataPins, TEXT(", ")) + TEXT("\n");
			OutputDeclarations += TEXT("\n");
			OutputDeclarations += TEXT("uint <pin>_WriteValue(uint Index, uint Value);");
		}
	}
}

void UPCGCustomHLSLSettings::UpdatePinSettings()
{
	for (int PinIndex = 0; PinIndex < InputPins.Num(); ++PinIndex)
	{
		FPCGPinProperties& Properties = InputPins[PinIndex];
		Properties.bDisplayBufferSizeSettings = false;

		// Type Any is not allowed, default to Point
		if (Properties.AllowedTypes == EPCGDataType::Any)
		{
			Properties.AllowedTypes = EPCGDataType::Point;
		}

		if (!!(Properties.AllowedTypes & EPCGDataType::Landscape)
			|| !!(Properties.AllowedTypes & EPCGDataType::Texture))
		{
			// Don't allow multiple data on this pin because we do not support a dynamic number of textures/landscapes bound to a
			// compute kernel.
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

	for (int PinIndex = 0; PinIndex < OutputPins.Num(); ++PinIndex)
	{
		FPCGPinProperties& Properties = OutputPins[PinIndex];

		// Type Any is not allowed, default to Point
		if (Properties.AllowedTypes == EPCGDataType::Any)
		{
			Properties.AllowedTypes = EPCGDataType::Point;
		}

		// Primary pin settings driven by kernel (if not custom kernel type).
		Properties.bDisplayBufferSizeSettings = PinIndex > 0 || KernelType == EPCGKernelType::Custom;

		// Output pins should always allow multiple connections.
		// TODO this could be hoisted up somewhere in the future.
		Properties.bAllowEditMultipleConnections = false;

		// Node is free to output any number of data.
		Properties.bAllowEditMultipleData = false;
	}
}
#endif

bool UPCGCustomHLSLSettings::IsKernelValid(FPCGContext* InContext) const
{
	if (OutputPinProperties().IsEmpty())
	{
		if (InContext)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, LOCTEXT("NoOutputs", "Kernels must have at least one output."));
		}

		return false;
	}

	for (const FPCGPinProperties& Properties : InputPinProperties())
	{
		if (Properties.AllowedTypes == EPCGDataType::Any)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(LOCTEXT("InvalidAnyInput", "Custom kernels do not support inputs of type Any, found on pin {0}."), FText::FromName(Properties.Label)));
			return false;
		}
	}

	for (const FPCGPinProperties& Properties : OutputPinProperties())
	{
		if (Properties.AllowedTypes == EPCGDataType::Any)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(LOCTEXT("InvalidAnyOutput", "Custom kernels do not support outputs of type Any, found on pin {0}."), FText::FromName(Properties.Label)));
			return false;
		}

		if (!!(Properties.AllowedTypes & EPCGDataType::Landscape))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(LOCTEXT("InvalidLSOutput", "Custom kernels do not support outputs of type Landscape, found on pin {0}."), FText::FromName(Properties.Label)));
			return false;
		}

		if (!!(Properties.AllowedTypes & EPCGDataType::Texture))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(LOCTEXT("InvalidTextureOutput", "Custom kernels do not support outputs of type Texture, found on pin {0}."), FText::FromName(Properties.Label)));
			return false;
		}
	}

	if (KernelType == EPCGKernelType::PointProcessor)
	{
		if (!GetPointProcessingInputPin())
		{
			if (InContext)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, LOCTEXT("InvalidPPInput", "Point processing kernel requires a first input pin of type point."));
			}

			return false;
		}

		if (!GetFirstPointOutputPin())
		{
			if (InContext)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, LOCTEXT("InvalidPPInput", "Point processing kernel requires a first output pin of type point."));
			}

			return false;
		}
	}

	auto CheckPinLabels = [InContext](const TArray<FPCGPinProperties>& Pins)
	{
		TSet<FName> EncounteredLabels;
		for (const FPCGPinProperties& Pin : Pins)
		{
			if (EncounteredLabels.Contains(Pin.Label))
			{
				if (InContext)
				{
					PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(LOCTEXT("DuplicatedPinLabels", "Duplicate pin label '{0}', all labels must be unique."), FText::FromName(Pin.Label)));
				}

				return false;
			}
		}

		return true;
	};

	if (!CheckPinLabels(InputPinProperties()) || !CheckPinLabels(OutputPinProperties()))
	{
		return false;
	}

	return true;
}

FString UPCGCustomHLSLSettings::GetCookedKernelSource(const TMap<FPCGKernelAttributeKey, int32>& GlobalAttributeLookupTable) const
{
	const FIntVector GroupSize = GetThreadGroupSize();

	// FIXME: Create source range mappings so that we can go from error location to our source.
	FString Source = (TEXT("    // User kernel\n") + ShaderSource);
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
	// TODO: Currently done from all threads. Only write header from global index 0? Or write sections from data local index 0?
	FString HeaderWriters;

	auto EmitHeaderWriterFromInputPin = [&HeaderWriters](const FPCGPinProperties& InOutputPinProps, const UPCGPin* InFromPin)
	{
		HeaderWriters += FString::Format(TEXT(
			"    if (ThreadIndex >= GetNumThreads()) return;\n"
			"    \n"
			"    // Copy data collection header from pin {0} to pin {1}\n"
			"    {\n"
			"        const uint NumData = {0}_GetNumData();\n"
			"        {1}_WriteNumData(NumData);\n"
			"        \n"
			"        for (uint DataIndex = 0; DataIndex < NumData; ++DataIndex)\n"
			"        {\n"
			"            const uint DataAddress = {0}_ReadDataAddress(DataIndex);\n"
			"            const uint Id = {0}_ReadDataId(DataAddress);\n"
			"            const uint NumAttributes = {0}_ReadDataNumAttributes(DataAddress);\n"
			"            const uint PreambleSizeBytes = {0}_ReadDataPreambleSize(DataAddress);\n"
			"            const uint NumElements = {0}_ReadDataInfo(DataAddress);\n"
			"            \n"
			"            {1}_WriteDataAddress(DataIndex, DataAddress);\n"
			"            {1}_WriteDataId(DataAddress, Id);\n"
			"            {1}_WriteDataNumAttributes(DataAddress, NumAttributes);\n"
			"            {1}_WriteDataPreambleSize(DataAddress, PreambleSizeBytes);\n"
			"            {1}_WriteDataInfo(DataAddress, NumElements);\n"
			"            \n"
			"            const uint AttributeHeadersAddress = {0}_ReadDataAttributeHeadersAddress(DataAddress);\n"
			"            const uint PCG_MAX_NUM_ATTRIBUTES = {2};\n"
			"            \n"
			"            for (uint AttributeIndex = 0; AttributeIndex < PCG_MAX_NUM_ATTRIBUTES; ++AttributeIndex)\n"
			"            {\n"
			"                const uint AttributeHeaderAddress = {0}_ReadAttributeHeaderAddress(AttributeHeadersAddress, AttributeIndex);\n"
			"                const uint AttributeIdAndStride = {0}_ReadAttributeIdAndStride(AttributeHeaderAddress);\n"
			"                const uint AttributeAddress = {0}_ReadAttributeAddress(AttributeHeaderAddress);\n"
			"                \n"
			"                {1}_WriteAttributeIdAndStride(AttributeHeaderAddress, AttributeIdAndStride);\n"
			"                {1}_WriteAttributeAddress(AttributeHeaderAddress, AttributeAddress);\n"
			"            }\n"
			"        }\n"
			"    }\n"),
			{ InFromPin->Properties.Label.ToString(), InOutputPinProps.Label.ToString(), PCGComputeConstants::MAX_NUM_ATTRS });
	};

	if (KernelType == EPCGKernelType::PointProcessor || KernelType == EPCGKernelType::Custom)
	{
		const UPCGPin* FirstPin = (KernelType == EPCGKernelType::PointProcessor)
			? GetPointProcessingInputPin()
			: CastChecked<UPCGNode>(GetOuter())->GetPassThroughInputPin();

		const UPCGPin* SecondPin = GetSecondPointProcessingInputPin();

		// Initialize all output headers.
		for (const FPCGPinProperties& PinProps : OutputPinProperties())
		{
			// Only support Point pins for now.
			if (PinProps.AllowedTypes != EPCGDataType::Point)
			{
				continue;
			}

			if (PinProps.BufferSizeMode == EPCGPinBufferSizeMode::FromFirstPin && FirstPin)
			{
				EmitHeaderWriterFromInputPin(PinProps, FirstPin);
			}
			else if (PinProps.BufferSizeMode == EPCGPinBufferSizeMode::FromSecondPin && SecondPin)
			{
				EmitHeaderWriterFromInputPin(PinProps, SecondPin);
			}
		}
	}
	else if (KernelType == EPCGKernelType::PointGenerator)
	{
		if (const UPCGNode* Node = Cast<UPCGNode>(GetOuter()))
		{
			const UPCGPin* FirstPin = Node->GetPassThroughInputPin();
			const UPCGPin* SecondPin = GetSecondPointProcessingInputPin();
			const UPCGPin* PrimaryOutputPin = GetFirstPointOutputPin();
			
			auto EmitPointGenHeader = [&HeaderWriters, InPointCount = PointCount](const FPCGPinProperties& InOutputPinProps)
			{
				HeaderWriters += FString::Format(TEXT(
					"    if (ThreadIndex >= GetNumThreads()) return;\n"
					"    \n"
					"    // Write data collection header for pin {0}\n"
					"    {0}_WriteSinglePointDataCollectionHeader({1});\n"),
					{ InOutputPinProps.Label.ToString(), InPointCount });
			};

			for (const UPCGPin* OutputPin : Node->GetOutputPins())
			{
				if (!OutputPin || OutputPin->Properties.AllowedTypes != EPCGDataType::Point)
				{
					// Point only for now.
					continue;
				}

				const FPCGPinProperties& PinProps = OutputPin->Properties;

				if (OutputPin == PrimaryOutputPin)
				{
					EmitPointGenHeader(PinProps);
				}
				else if (PinProps.BufferSizeMode == EPCGPinBufferSizeMode::FromFirstPin && FirstPin)
				{
					EmitHeaderWriterFromInputPin(PinProps, FirstPin);
				}
				else if (PinProps.BufferSizeMode == EPCGPinBufferSizeMode::FromSecondPin && SecondPin)
				{
					EmitHeaderWriterFromInputPin(PinProps, SecondPin);
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
	
	if (KernelType == EPCGKernelType::PointProcessor)
	{
		const UPCGPin* InputPin = GetPointProcessingInputPin();
		const UPCGPin* OutputPin = GetFirstPointOutputPin();

		if (InputPin && OutputPin)
		{
			KernelSpecificPreamble += FString::Format(TEXT(
				"    const uint3 {0}_ThreadInfo = {0}_GetThreadData(ThreadIndex);\n"
				"    const uint {0}_DataIndex = {0}_ThreadInfo[0];\n"
				"    const uint {0}_DataAddress = {0}_ThreadInfo[1];\n"
				"    const uint ElementIndex = {0}_ThreadInfo[2]; // Assumption - element index identical in input and output data.\n"),
				{ InputPin->Properties.Label.ToString() });

			KernelSpecificPreamble += FString::Format(TEXT(
				"    const uint3 {0}_ThreadInfo = {0}_GetThreadData(ThreadIndex);\n"
				"    const uint {0}_DataIndex = {0}_ThreadInfo[0];\n"
				"    const uint {0}_DataAddress = {0}_ThreadInfo[1];\n"),
				{ OutputPin->Properties.Label.ToString() });

			// Automatically copy all attributes
			KernelSpecificPreamble += FString::Format(TEXT(
				"\n"
				"    // Loop over all attribute headers, if the address is non-zero, then copy it from pin {0} to pin {1}.\n"
				"    {\n"
				"        const uint HeadersAddress = {0}_ReadDataAttributeHeadersAddress({0}_DataAddress);\n"
				"        const uint NumAttributes = {0}_ReadDataNumAttributes({0}_DataAddress);\n"
				"        uint NumAttributesProcessed = 0;\n"
				"\n"
				"        for (int AttributeIndex = 0; AttributeIndex < 128; ++AttributeIndex)\n"
				"        {\n"
				"            const uint HeaderAddress = {0}_ReadAttributeHeaderAddress(HeadersAddress, AttributeIndex);\n"
				"            const uint Stride = {0}_ReadAttributeStride(HeaderAddress);\n"
				"            const uint Address = {0}_ReadAttributeAddress(HeaderAddress);\n"
				"\n"
				"            if (Address != 0)\n"
				"            {\n"
				"                const uint BaseElementAddress = Address + ElementIndex * Stride;\n"
				"\n"
				"                for (int I = 0; I < Stride; I += 4)\n"
				"                {\n"
				"                    const uint ElementAddress = BaseElementAddress + I;\n"
				"                    {1}_StoreBuffer(ElementAddress, {0}_LoadBuffer(ElementAddress));\n"
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
		KernelSpecificPreamble += FString::Format(TEXT(
			"    const uint PointCount = {0};\n"),
			{ PointCount });

		if (const UPCGPin* OutputPin = GetFirstPointOutputPin())
		{
			KernelSpecificPreamble += FString::Format(TEXT(
				"    const uint3 {0}_ThreadInfo = {0}_GetThreadData(ThreadIndex);\n"
				"    const uint {0}_DataIndex = {0}_ThreadInfo[0];\n"
				"    const uint {0}_DataAddress = {0}_ThreadInfo[1];\n"
				"    const uint ElementIndex = {0}_ThreadInfo[2];\n"),
				{ OutputPin->Properties.Label.ToString() });

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
			"#line 1 \"%s\"\n" // ShaderPathName
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
			"    const uint ThreadIndex = %s;\n" // UnWrappedDispatchThreadId
			"%s\n" // HeaderWriters
			"%s\n" // KernelSpecificPreamble
			"#line 1 \"%s\"\n\n" // ShaderPathName
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

#undef LOCTEXT_NAMESPACE
