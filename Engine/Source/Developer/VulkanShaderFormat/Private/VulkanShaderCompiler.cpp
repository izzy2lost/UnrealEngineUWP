// Copyright Epic Games, Inc. All Rights Reserved.
// .

#include "RHIShaderFormatDefinitions.inl"
#include "ShaderCompilerCommon.h"
#include "ShaderCompilerDefinitions.h"
#include "ShaderParameterParser.h"
#include "ShaderPreprocessTypes.h"
#include "SpirvReflectCommon.h"
#include "VulkanCommon.h"

#include "VulkanThirdParty.h"
#include "VulkanBackend.h"
#include "VulkanShaderResources.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "SpirVShaderCompiler.inl"


inline bool IsVulkanShaderFormat(FName ShaderFormat)
{
	return ShaderFormat == NAME_VULKAN_ES3_1_ANDROID
		|| ShaderFormat == NAME_VULKAN_ES3_1
		|| ShaderFormat == NAME_VULKAN_SM5
		|| ShaderFormat == NAME_VULKAN_SM6
		|| ShaderFormat == NAME_VULKAN_SM5_ANDROID;
}

inline bool IsAndroidShaderFormat(FName ShaderFormat)
{
	return ShaderFormat == NAME_VULKAN_ES3_1_ANDROID
		|| ShaderFormat == NAME_VULKAN_SM5_ANDROID;
}


enum class EVulkanShaderVersion
{
	ES3_1,
	ES3_1_ANDROID,
	SM5,
	SM5_ANDROID,
	SM6,
	Invalid,
};


DEFINE_LOG_CATEGORY_STATIC(LogVulkanShaderCompiler, Log, All); 


class FVulkanShaderCompilerInternalState : public FSpirvShaderCompilerInternalState
{
	EVulkanShaderVersion FormatToVersion(FName Format)
	{
		if (Format == NAME_VULKAN_ES3_1)
		{
			return EVulkanShaderVersion::ES3_1;
		}
		else if (Format == NAME_VULKAN_ES3_1_ANDROID)
		{
			return EVulkanShaderVersion::ES3_1_ANDROID;
		}
		else if (Format == NAME_VULKAN_SM5_ANDROID)
		{
			return EVulkanShaderVersion::SM5_ANDROID;
		}
		else if (Format == NAME_VULKAN_SM5)
		{
			return EVulkanShaderVersion::SM5;
		}
		else if (Format == NAME_VULKAN_SM6)
		{
			return EVulkanShaderVersion::SM6;
		}
		else
		{
			FString FormatStr = Format.ToString();
			checkf(0, TEXT("Invalid shader format passed to Vulkan shader compiler: %s"), *FormatStr);
			return EVulkanShaderVersion::Invalid;
		}
	}

public:
	FVulkanShaderCompilerInternalState(const FShaderCompilerInput& InInput, const FShaderParameterParser* InParameterParser)
		: FSpirvShaderCompilerInternalState(InInput, InParameterParser)
	{
		Version = FormatToVersion(Input.ShaderFormat);

		if (Version == EVulkanShaderVersion::SM6)
		{
			MinimumTargetEnvironment = CrossCompiler::FShaderConductorOptions::ETargetEnvironment::Vulkan_1_3;
		}
		else if (Input.IsRayTracingShader() || Input.Environment.CompilerFlags.Contains(CFLAG_InlineRayTracing))
		{
			MinimumTargetEnvironment = CrossCompiler::FShaderConductorOptions::ETargetEnvironment::Vulkan_1_2;
		}
		else
		{
			MinimumTargetEnvironment = CrossCompiler::FShaderConductorOptions::ETargetEnvironment::Vulkan_1_1;
		}

		bIsAndroid = IsAndroidShaderFormat(Input.ShaderFormat);

		bSupportsOfflineCompiler = 
			Input.ShaderFormat == NAME_VULKAN_ES3_1_ANDROID ||
			Input.ShaderFormat == NAME_VULKAN_ES3_1 ||
			Input.ShaderFormat == NAME_VULKAN_SM5_ANDROID;;
	}

	bool IsSM6() const override final
	{
		return (Version == EVulkanShaderVersion::SM6);
	}

	bool IsSM5() const override final
	{
		return (Version == EVulkanShaderVersion::SM5) || (Version == EVulkanShaderVersion::SM5_ANDROID);
	}

	bool IsMobileES31() const override final
	{
		return (Version == EVulkanShaderVersion::ES3_1 || Version == EVulkanShaderVersion::ES3_1_ANDROID);
	}

	CrossCompiler::FShaderConductorOptions::ETargetEnvironment GetMinimumTargetEnvironment() const override final
	{
		return MinimumTargetEnvironment;
	}

	bool IsAndroid() const override final
	{
		return bIsAndroid;
	}

	bool SupportsOfflineCompiler() const override final
	{
		return bSupportsOfflineCompiler;
	}

private:
	EVulkanShaderVersion Version;
	CrossCompiler::FShaderConductorOptions::ETargetEnvironment MinimumTargetEnvironment;
	bool bIsAndroid = false;
	bool bSupportsOfflineCompiler = false;;
};


void ModifyVulkanCompilerInput(FShaderCompilerInput& Input)
{
	FVulkanShaderCompilerInternalState InternalState(Input, nullptr);
	SpirvShaderCompiler::ModifyCompilerInput(InternalState, Input);
}

// :todo-jn: TEMPORARY EXPERIMENT - will eventually move into preprocessing step
static TArray<FString> ConvertUBToBindless(FString& PreprocessedShaderSource)
{
	// Fill a map so we pull our bindless sampler/resource indices from the right struct
	// :todo-jn: Do we not have the layout somewhere instead of calculating offsets?  there must be a better way...
	auto GenerateNewDecl = [](const int32 CBIndex, const FString& Members, const FString& CBName)
	{
		const FString PrefixedCBName = FString::Printf(TEXT("%s%d_%s"), *SpirvShaderCompiler::kBindlessCBPrefix, CBIndex, *CBName);
		const FString BindlessCBType = PrefixedCBName + TEXT("_Type");
		const FString BindlessCBHeapName = PrefixedCBName + SpirvShaderCompiler::kBindlessHeapSuffix;
		const FString PaddingName = FString::Printf(TEXT("%s_Padding"), *CBName);

		FString CBDecl;
		CBDecl.Reserve(Members.Len() * 3);  // start somewhere approx less bad

		// Declare the struct
		CBDecl += TEXT("struct ") + BindlessCBType + TEXT(" \n{\n") + Members + TEXT("\n};\n");

		// Declare the safetype and bindless array for this cb
		CBDecl += FString::Printf(TEXT("ConstantBuffer<%s> %s[];\n"), *BindlessCBType, *BindlessCBHeapName);

		// Now bring in the CB
		CBDecl += FString::Printf(TEXT("static const %s %s = %s[VulkanHitGroupSystemParameters.BindlessUniformBuffers[%d]];\n"),
			*BindlessCBType, *PrefixedCBName, *BindlessCBHeapName, CBIndex);

		// Now create a global scope var for each value (as the cbuffer would provide) to patch in seemlessly with the rest of the code
		uint32 MemberOffset = 0;
		const TCHAR* MemberSearchPtr = *Members;
		const uint32 LastMemberSemicolonIndex = Members.Find(TEXT(";"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, -1);
		check(LastMemberSemicolonIndex != INDEX_NONE);
		const TCHAR* LastMemberSemicolon = &Members[LastMemberSemicolonIndex];

		do
		{
			const TCHAR* MemberTypeStartPtr = nullptr;
			const TCHAR* MemberTypeEndPtr = nullptr;
			ParseHLSLTypeName(MemberSearchPtr, MemberTypeStartPtr, MemberTypeEndPtr);
			const FString MemberTypeName = FString::ConstructFromPtrSize(MemberTypeStartPtr, MemberTypeEndPtr - MemberTypeStartPtr);

			FString MemberName;
			MemberSearchPtr = ParseHLSLSymbolName(MemberTypeEndPtr, MemberName);
			check(MemberName.Len() > 0);

			if (MemberName.StartsWith(PaddingName))
			{
				while (*MemberSearchPtr && *MemberSearchPtr != ';')
				{
					MemberSearchPtr++;
				}
			}
			else
			{
				// Skip over trailing tokens and pick up arrays
				FString ArrayDecl;
				while (*MemberSearchPtr && *MemberSearchPtr != ';')
				{
					if (*MemberSearchPtr == '[')
					{
						ArrayDecl.AppendChar(*MemberSearchPtr);

						MemberSearchPtr++;
						while (*MemberSearchPtr && *MemberSearchPtr != ']')
						{
							ArrayDecl.AppendChar(*MemberSearchPtr);
							MemberSearchPtr++;
						}

						ArrayDecl.AppendChar(*MemberSearchPtr);
					}

					MemberSearchPtr++;
				}

				CBDecl += FString::Printf(TEXT("static const %s %s%s = %s.%s;\n"), *MemberTypeName, *MemberName, *ArrayDecl, *PrefixedCBName, *MemberName);
			}

			MemberSearchPtr++;

		} while (MemberSearchPtr < LastMemberSemicolon);

		return CBDecl;
	};

	// replace "cbuffer" decl with a struct filled from bindless constant buffer
	TArray<FString> BindlessUBs;
	{
		const FString UniformBufferDeclIdentifier = TEXT("cbuffer");

		int32 SearchIndex = PreprocessedShaderSource.Find(UniformBufferDeclIdentifier, ESearchCase::CaseSensitive, ESearchDir::FromStart, -1);
		while (SearchIndex != INDEX_NONE)
		{
			FString StructName;
			const TCHAR* StructNameEndPtr = ParseHLSLSymbolName(&PreprocessedShaderSource[SearchIndex + UniformBufferDeclIdentifier.Len()], StructName);
			check(StructName.Len() > 0);

			const int32 CBIndex = BindlessUBs.Add(StructName);
			check(CBIndex < 16);

			const TCHAR* OpeningBracePtr = FCString::Strstr(&PreprocessedShaderSource[SearchIndex + UniformBufferDeclIdentifier.Len()], TEXT("{"));
			check(OpeningBracePtr);
			const TCHAR* ClosingBracePtr = FindMatchingClosingBrace(OpeningBracePtr + 1);
			check(ClosingBracePtr);
			const int32 ClosingBraceIndex = ClosingBracePtr - (*PreprocessedShaderSource);

			const FString Members = FString::ConstructFromPtrSize(OpeningBracePtr + 1, ClosingBracePtr - OpeningBracePtr - 1);
			const FString NewDecl = GenerateNewDecl(CBIndex, Members, StructName);

			const int32 OldDeclLen = ClosingBraceIndex - SearchIndex + 1;
			PreprocessedShaderSource.RemoveAt(SearchIndex, OldDeclLen, EAllowShrinking::No);
			PreprocessedShaderSource.InsertAt(SearchIndex, NewDecl);

			SearchIndex = PreprocessedShaderSource.Find(UniformBufferDeclIdentifier, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchIndex + NewDecl.Len());
		}
	}
	return BindlessUBs;
}


// Helper function to know how much space to set aside in the shader record for a global
static uint32 GetSizeForType(FStringView TypeName, FStringView ArraySize)
{
	static TMap<FStringView, uint32> SizeForTypeMap;
	if (SizeForTypeMap.Num() == 0)
	{
		SizeForTypeMap.Add(FStringView(TEXT("uint")),   4);
		SizeForTypeMap.Add(FStringView(TEXT("uint2")),  8);
		SizeForTypeMap.Add(FStringView(TEXT("uint4")),  16);
		SizeForTypeMap.Add(FStringView(TEXT("float")),  4);
		SizeForTypeMap.Add(FStringView(TEXT("float2")), 8);
		SizeForTypeMap.Add(FStringView(TEXT("float4")), 16);
	}

	checkf(ArraySize.Len() == 0, TEXT("Need to add array support!")); // :todo-jn: Add array parsing

	const uint32* TypeSize = SizeForTypeMap.Find(TypeName);
	checkf(TypeSize, TEXT("Missing type size for %.*s"), TypeName.Len(), TypeName.GetData());
	return *TypeSize;
}


// :todo-jn: TEMPORARY EXPERIMENT - will eventually move into preprocessing step
static uint32 ConvertGlobalsToShaderRecord(const FShaderParameterParser& ShaderParameterParser, const TMap<FStringView, FStringView>& ReplacedGlobals, FString& PreprocessedShaderSource, FShaderCompilerOutput& Output)
{
	uint32 ShaderRecordGlobalsSize = 0;
	uint32 ShaderRecordParamCount = 0;
	FString ShaderRecordGlobalsString;

	for (const auto& ParamDecl : ReplacedGlobals)
	{
		ShaderRecordGlobalsString += ParamDecl.Value;

		const FString ParamName(ParamDecl.Key);
		const FShaderParameterParser::FParsedShaderParameter& Info = ShaderParameterParser.FindParameterInfos(ParamName);
		const uint32 ParamSize = GetSizeForType(Info.ParsedType, Info.ParsedArraySize);

		HandleReflectedGlobalConstantBufferMember(
			ParamName,
			ShaderRecordParamCount++,
			ShaderRecordGlobalsSize,
			ParamSize,
			Output
		);

		ShaderRecordGlobalsSize += ParamSize; 
	}

	if (ShaderRecordGlobalsString.Len())
	{
		const int32 ReplacementCount = PreprocessedShaderSource.ReplaceInline(TEXT("uint VulkanShaderRecordDummyGlobals;"), *ShaderRecordGlobalsString, ESearchCase::CaseSensitive);
		checkf(ReplacementCount == 1, TEXT("VulkanShaderRecordDummyGlobals was replaced %d times!"), ReplacementCount);
	}

	return ShaderRecordGlobalsSize;
}


static void UpdateBindlessUBs(const FSpirvShaderCompilerInternalState& InternalState, SpirvShaderCompilerSerializedOutput& SerializedOutput, FShaderCompilerOutput& Output)
{
	checkf(SerializedOutput.Header.Bindings.Num() == 0, TEXT("Shaders using bindless UBs should have no other bindings."));
	for (int32 CBIndex = 0; CBIndex < InternalState.AllBindlessUBs.Num(); CBIndex++)
	{
		const FString& CBName = InternalState.AllBindlessUBs[CBIndex];

		// It's possible SPIRV compilation has optimized out a buffer from every shader in the group
		if (SerializedOutput.UsedBindlessUB.Contains(CBName))
		{
			FVulkanShaderHeader::FUniformBufferInfo& Info = SerializedOutput.Header.UniformBufferInfos.AddZeroed_GetRef();
			Info.LayoutHash = SpirvShaderCompiler::GetUBLayoutHash(InternalState.Input, CBName);
			Info.BindlessCBIndex = CBIndex;

			const int32 UBIndex = SerializedOutput.Header.UniformBufferInfos.Num() - 1;
			Output.ParameterMap.AddParameterAllocation(CBName, UBIndex, 0, 1, EShaderParameterType::UniformBuffer);
		}
	}
}


static bool CompileShaderGroup(
	FSpirvShaderCompilerInternalState& InternalState,
	const FShaderSource::FStringType& OriginalPreprocessedShaderSource,
	FShaderCompilerOutput& MergedOutput
)
{
	checkf(InternalState.bSupportsBindless && InternalState.bUseBindlessUniformBuffer, TEXT("Ray tracing requires full bindless in Vulkan."));

	// Compile each one of the shader modules seperately and create one big blob for the engine
	auto CompilePartialExport = [&OriginalPreprocessedShaderSource, &InternalState, &MergedOutput](
		FSpirvShaderCompilerInternalState::EHitGroupShaderType HitGroupShaderType,
		const TCHAR* PartialFileExtension,
		SpirvShaderCompilerSerializedOutput& PartialSerializedOutput)
	{
		InternalState.HitGroupShaderType = HitGroupShaderType;

		FShaderCompilerOutput TempOutput;
		const bool bIsClosestHit = (HitGroupShaderType == FSpirvShaderCompilerInternalState::EHitGroupShaderType::ClosestHit);
		FShaderCompilerOutput& PartialOutput = bIsClosestHit ? MergedOutput : TempOutput;

		FShaderSource::FViewType OrigSourceView(OriginalPreprocessedShaderSource);
		FShaderSource PartialPreprocessedShaderSource(OrigSourceView);
		UE::ShaderCompilerCommon::RemoveDeadCode(PartialPreprocessedShaderSource, InternalState.GetEntryPointName(), PartialOutput.Errors);

		if (InternalState.bDebugDump)
		{
			DumpDebugShaderText(InternalState.Input, PartialPreprocessedShaderSource.GetView().GetData(), *FString::Printf(TEXT("%s.hlsl"), PartialFileExtension));
		}

		const bool bPartialSuccess = SpirvShaderCompiler::CompileWithShaderConductor(InternalState, PartialPreprocessedShaderSource.GetView(), PartialSerializedOutput, PartialOutput);

		if (!bIsClosestHit)
		{
			MergedOutput.NumInstructions = FMath::Max(MergedOutput.NumInstructions, PartialOutput.NumInstructions);
			MergedOutput.NumTextureSamplers = FMath::Max(MergedOutput.NumTextureSamplers, PartialOutput.NumTextureSamplers);
			MergedOutput.Errors.Append(MoveTemp(PartialOutput.Errors));
		}

		return bPartialSuccess;
	};

	bool bSuccess = false;

	// Closest Hit Module, always present
	SpirvShaderCompilerSerializedOutput ClosestHitSerializedOutput;
	{
		bSuccess = CompilePartialExport(FSpirvShaderCompilerInternalState::EHitGroupShaderType::ClosestHit, TEXT("closest"), ClosestHitSerializedOutput);
	}

	// Any Hit Module, optional
	const bool bHasAnyHitModule = !InternalState.AnyHitEntry.IsEmpty();
	SpirvShaderCompilerSerializedOutput AnyHitSerializedOutput;
	if (bSuccess && bHasAnyHitModule)
	{
		bSuccess = CompilePartialExport(FSpirvShaderCompilerInternalState::EHitGroupShaderType::AnyHit, TEXT("anyhit"), AnyHitSerializedOutput);
	}

	// Intersection Module, optional
	const bool bHasIntersectionModule = !InternalState.IntersectionEntry.IsEmpty();
	SpirvShaderCompilerSerializedOutput IntersectionSerializedOutput;
	if (bSuccess && bHasIntersectionModule)
	{
		bSuccess = CompilePartialExport(FSpirvShaderCompilerInternalState::EHitGroupShaderType::Intersection, TEXT("intersection"), IntersectionSerializedOutput);
	}

	// Collapse the bindless UB usage into one set and then update the headers
	ClosestHitSerializedOutput.UsedBindlessUB.Append(AnyHitSerializedOutput.UsedBindlessUB);
	ClosestHitSerializedOutput.UsedBindlessUB.Append(IntersectionSerializedOutput.UsedBindlessUB);
	UpdateBindlessUBs(InternalState, ClosestHitSerializedOutput, MergedOutput);

	{
		// :todo-jn: Having multiple entrypoints in a single SPIRV blob crashes on FLumenHardwareRayTracingMaterialHitGroup for some reason
		// Adjust the header before we write it out
		ClosestHitSerializedOutput.Header.RayGroupAnyHit = bHasAnyHitModule ? FVulkanShaderHeader::ERayHitGroupEntrypoint::SeparateBlob : FVulkanShaderHeader::ERayHitGroupEntrypoint::NotPresent;
		ClosestHitSerializedOutput.Header.RayGroupIntersection = bHasIntersectionModule ? FVulkanShaderHeader::ERayHitGroupEntrypoint::SeparateBlob : FVulkanShaderHeader::ERayHitGroupEntrypoint::NotPresent;

		check(ClosestHitSerializedOutput.Spirv.Data.Num() != 0);
		FMemoryWriter Ar(MergedOutput.ShaderCode.GetWriteAccess(), true);
		Ar << ClosestHitSerializedOutput.Header;
		Ar << ClosestHitSerializedOutput.ShaderResourceTable;

		{
			uint32 SpirvCodeSizeBytes = ClosestHitSerializedOutput.Spirv.GetByteSize();
			Ar << SpirvCodeSizeBytes;
			Ar.Serialize((uint8*)ClosestHitSerializedOutput.Spirv.Data.GetData(), SpirvCodeSizeBytes);
		}

		if (bHasAnyHitModule)
		{
			uint32 SpirvCodeSizeBytes = AnyHitSerializedOutput.Spirv.GetByteSize();
			Ar << SpirvCodeSizeBytes;
			Ar.Serialize((uint8*)AnyHitSerializedOutput.Spirv.Data.GetData(), SpirvCodeSizeBytes);
		}

		if (bHasIntersectionModule)
		{
			uint32 SpirvCodeSizeBytes = IntersectionSerializedOutput.Spirv.GetByteSize();
			Ar << SpirvCodeSizeBytes;
			Ar.Serialize((uint8*)IntersectionSerializedOutput.Spirv.Data.GetData(), SpirvCodeSizeBytes);
		}
	}

	MergedOutput.bSucceeded = bSuccess;
	return bSuccess;
}

struct FVulkanShaderParameterParserPlatformConfiguration : public FShaderParameterParser::FPlatformConfiguration
{
	FVulkanShaderParameterParserPlatformConfiguration(const FShaderCompilerInput& Input, TMap<FStringView, FStringView>& InReplacedGlobals)
		: FShaderParameterParser::FPlatformConfiguration()
		, bIsRayTracingShader(Input.IsRayTracingShader())
		, HitGroupSystemIndexBufferName(FShaderParameterParser::kBindlessSRVPrefix + FString(TEXT("HitGroupSystemIndexBuffer")))
		, HitGroupSystemVertexBufferName(FShaderParameterParser::kBindlessSRVPrefix + FString(TEXT("HitGroupSystemVertexBuffer")))
		, ReplacedGlobals(InReplacedGlobals)
	{
		EnumAddFlags(Flags, EShaderParameterParserConfigurationFlags::SupportsBindless | EShaderParameterParserConfigurationFlags::BindlessUsesArrays);

		// Create a _RootShaderParameters and bind it in slot 0 like any other uniform buffer
		if (Input.Target.GetFrequency() == SF_RayGen && Input.RootParametersStructure != nullptr)
		{
			ConstantBufferType = TEXTVIEW("cbuffer");
			EnumAddFlags(Flags, EShaderParameterParserConfigurationFlags::UseStableConstantBuffer);
		}

		// Place loose data params in the shader record for shaders with bindless UBs
		if (bIsRayTracingShader && (Input.Target.GetFrequency() != SF_RayGen))
		{
			EnumAddFlags(Flags, EShaderParameterParserConfigurationFlags::ReplaceGlobals);
		}
	}

	virtual FString GenerateBindlessAccess(EBindlessConversionType BindlessType, FStringView FullTypeString, FStringView ArrayNameOverride, FStringView IndexString) const final
	{
		if (bIsRayTracingShader && (BindlessType == EBindlessConversionType::SRV))
		{
			// Patch the HitGroupSystemIndexBuffer/HitGroupSystemVertexBuffer indices to use the ones contained in the shader record
			if (IndexString == HitGroupSystemIndexBufferName)
			{
				IndexString = TEXTVIEW("VulkanHitGroupSystemParameters.BindlessHitGroupSystemIndexBuffer");
			}
			else if (IndexString == HitGroupSystemVertexBufferName)
			{
				IndexString = TEXTVIEW("VulkanHitGroupSystemParameters.BindlessHitGroupSystemVertexBuffer");
			}
		}

		// Heap[Index]
		return FString::Printf(TEXT("%.*s[%.*s]"),
			ArrayNameOverride.Len(), ArrayNameOverride.GetData(),
			IndexString.Len(), IndexString.GetData());
	}

	// Fill the global with the value stored in the shader record
	virtual FString ReplaceGlobal(FStringView FullDeclString, FStringView ParamName) const final
	{
		ReplacedGlobals.Add(ParamName, FullDeclString);

		FString NewDecl(FullDeclString);
		NewDecl = TEXT("static ") + NewDecl;
		NewDecl.InsertAt(NewDecl.Find(TEXT(";")), FString::Printf(TEXT(" = VulkanHitGroupSystemParameters.Globals.%.*s"), ParamName.Len(), ParamName.GetData()));
		return NewDecl;
	}

	const bool bIsRayTracingShader;
	const FString HitGroupSystemIndexBufferName;
	const FString HitGroupSystemVertexBufferName;
	TMap<FStringView, FStringView>& ReplacedGlobals;
};

void CompileVulkanShader(const FShaderCompilerInput& Input, const FShaderPreprocessOutput& InPreprocessOutput, FShaderCompilerOutput& Output, const class FString& WorkingDirectory)
{
	check(IsVulkanShaderFormat(Input.ShaderFormat));

	FString EntryPointName = Input.EntryPointName;
	FString PreprocessedSource(InPreprocessOutput.GetSourceViewWide());

	TMap<FStringView, FStringView> ReplacedGlobals; // Note: these FStringView point to memory in FShaderParameterParser
	FVulkanShaderParameterParserPlatformConfiguration PlatformConfiguration(Input, ReplacedGlobals);
	FShaderParameterParser ShaderParameterParser(PlatformConfiguration);
	if (!ShaderParameterParser.ParseAndModify(Input, Output.Errors, PreprocessedSource))
	{
		// The FShaderParameterParser will add any relevant errors.
		return;
	}

	FVulkanShaderCompilerInternalState InternalState(Input, &ShaderParameterParser);

	if (InternalState.bUseBindlessUniformBuffer)
	{
		InternalState.ShaderRecordGlobalsSize = ConvertGlobalsToShaderRecord(ShaderParameterParser, ReplacedGlobals, PreprocessedSource, Output);
		InternalState.AllBindlessUBs = ConvertUBToBindless(PreprocessedSource);
	}

	if (ShaderParameterParser.DidModifyShader() || InternalState.AllBindlessUBs.Num() || InternalState.ShaderRecordGlobalsSize)
	{
		Output.ModifiedShaderSource = PreprocessedSource;
	}

	bool bSuccess = false;

#if SHADER_SOURCE_ANSI
	// Convert to ANSI prior to calling into ShaderConductor. This copy would have been incurred
	// by SC itself anyways, but would (will?) also be unnecessary if (when) shader parameter parser
	// is modified to operate on ANSI strings.
	const FShaderSource::FStringType PreprocessedSourceToCompile(PreprocessedSource);
#else
	const FShaderSource::FStringType& PreprocessedSourceToCompile = PreprocessedSource;
#endif

#if PLATFORM_MAC || PLATFORM_WINDOWS || PLATFORM_LINUX
	// HitGroup shaders might have multiple entrypoints that we combine into a single blob
	if (InternalState.HasMultipleEntryPoints())
	{
		bSuccess = CompileShaderGroup(InternalState, PreprocessedSourceToCompile, Output);
	}
	else
	{
		// Compile regular shader via ShaderConductor (DXC)
		SpirvShaderCompilerSerializedOutput SerializedOutput;
		bSuccess = SpirvShaderCompiler::CompileWithShaderConductor(InternalState, PreprocessedSourceToCompile, SerializedOutput, Output);

		if (InternalState.bUseBindlessUniformBuffer)
		{
			UpdateBindlessUBs(InternalState, SerializedOutput, Output);
		}

		// Write out the header and shader source code (except for the extra shaders in hit groups)
		checkf(!(bSuccess && SerializedOutput.Spirv.Data.Num() == 0), TEXT("shader compilation was reported as successful but SPIR-V module is empty"));
		FMemoryWriter Ar(Output.ShaderCode.GetWriteAccess(), true);
		Ar << SerializedOutput.Header;
		Ar << SerializedOutput.ShaderResourceTable;

		uint32 SpirvCodeSizeBytes = SerializedOutput.Spirv.GetByteSize();
		Ar << SpirvCodeSizeBytes;
		if (SerializedOutput.Spirv.Data.Num() > 0)
		{
			Ar.Serialize((uint8*)SerializedOutput.Spirv.Data.GetData(), SpirvCodeSizeBytes);
		}
	}
#endif // PLATFORM_MAC || PLATFORM_WINDOWS || PLATFORM_LINUX
	
	if (InternalState.bUseBindlessUniformBuffer)
	{
		// HACK: Because of heavy code alterations with bindless ray tracing shaders, line numbers will be all over the place.  Remove the tag that leads to remapping...
		for (FShaderCompilerError& ErrorMsg : Output.Errors)
		{
			ErrorMsg.StrippedErrorMessage.ReplaceInline(TEXT("__UE_FILENAME_SENTINEL"), *Input.GenerateShaderName());
		}
	}

	if (Input.Environment.CompilerFlags.Contains(CFLAG_ExtraShaderData))
	{
		Output.ShaderCode.AddOptionalData(FShaderCodeName::Key, TCHAR_TO_UTF8(*Input.GenerateShaderName()));
	}

	Output.SerializeShaderCodeValidation();

	ShaderParameterParser.ValidateShaderParameterTypes(Input, InternalState.IsMobileES31(), Output);
	
	if (EnumHasAnyFlags(Input.DebugInfoFlags, EShaderDebugInfoFlags::CompileFromDebugUSF))
	{
		for (const auto& Error : Output.Errors)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s\n"), *Error.GetErrorStringWithLineMarker());
		}
		ensure(bSuccess);
	}
}

void OutputVulkanDebugData(const FShaderCompilerInput& Input, const FShaderPreprocessOutput& PreprocessOutput, const FShaderCompilerOutput& Output)
{
	UE::ShaderCompilerCommon::DumpExtendedDebugShaderData(Input, PreprocessOutput, Output);
}
