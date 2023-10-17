// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderPreprocessor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "PreprocessorPrivate.h"
#include "ShaderCompilerDefinitions.h"

#include "stb_preprocess/preprocessor.h"
#include "stb_preprocess/stb_alloc.h"
#include "stb_preprocess/stb_ds.h"

static TAutoConsoleVariable<int32> CVarShaderCompilerThreadLocalPreprocessBuffer(
	TEXT("r.ShaderCompiler.ThreadLocalPreprocessBuffer"),
	1280 * 1024,
	TEXT("Amount to preallocate for preprocess output per worker thread, to save reallocation overhead in the preprocessor."),
	ECVF_Default
);

namespace
{
	const FString PlatformHeader = TEXT("/Engine/Public/Platform.ush");
	const FString PlatformHeaderLowerCase = PlatformHeader.ToLower();
	void LogMandatoryHeaderError(const FShaderCompilerInput& Input, FShaderPreprocessOutput& Output)
	{
		FString Path = Input.VirtualSourceFilePath;
		FString Message = FString::Printf(TEXT("Error: Shader is required to include %s"), *PlatformHeader);
		Output.LogError(MoveTemp(Path), MoveTemp(Message), 1);
	}
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS		// FShaderCompilerDefinitions will be made internal in the future, marked deprecated until then
static void AddStbDefine(stb_arena* MacroArena, macro_definition**& StbDefines, const TCHAR* Name, const TCHAR* Value);
static void AddStbDefines(stb_arena* MacroArena, macro_definition**& StbDefines, const FShaderCompilerDefinitions& Defines);

class FShaderPreprocessorUtilities
{
public:
	static void DumpShaderDefinesAsCommentedCode(const FShaderCompilerEnvironment& Environment, FString* OutDefines)
	{
		TArray<FString> DefinesLines;
		DefinesLines.Reserve(Environment.Definitions->Num());
		for (FShaderCompilerDefinitions::FConstIterator DefineIt(*Environment.Definitions); DefineIt; ++DefineIt)
		{
			DefinesLines.Add(FString::Printf(TEXT("// #define %s %s\n"), DefineIt.Key(), DefineIt.Value()));
		}
		DefinesLines.Sort();

		FString Defines;
		for (const FString& DefineLine : DefinesLines)
		{
			Defines += DefineLine;
		}

		*OutDefines += MakeInjectedShaderCodeBlock(TEXT("DumpShaderDefinesAsCommentedCode"), Defines);
	}

	static void PopulateDefines(const FShaderCompilerEnvironment& Environment, const FShaderCompilerDefinitions& AdditionalDefines, stb_arena* MacroArena, macro_definition**& OutDefines)
	{
		arrsetcap(OutDefines, Environment.Definitions->Num() + AdditionalDefines.Num());
		AddStbDefines(MacroArena, OutDefines, *Environment.Definitions);
		AddStbDefines(MacroArena, OutDefines, AdditionalDefines);
	}
};
PRAGMA_ENABLE_DEPRECATION_WARNINGS

//////////////////////////////////////////////////////////////////////////
extern "C"
{
	// adapter functions for STB memory allocation
	void* StbMalloc(size_t Size) 
	{
		void* Alloc = FMemory::Malloc(Size);
		return Alloc; 
	}

	void* StbRealloc(void* Pointer, size_t Size) 
	{
		void* Alloc = FMemory::Realloc(Pointer, Size);
		return Alloc;
	}

	void StbFree(void* Pointer) 
	{
		return FMemory::Free(Pointer); 
	}

	ANSICHAR* StbStrDup(const ANSICHAR* InString)
	{
		if (InString)
		{
			int32 Len = FCStringAnsi::Strlen(InString) + 1;
			ANSICHAR* Result = reinterpret_cast<ANSICHAR*>(StbMalloc(Len));
			return FCStringAnsi::Strncpy(Result, InString, Len);
		}
		return nullptr;
	}
}

struct FStbLoadedInclude
{
	const ANSICHAR* Data = nullptr;				// Points to SharedData, LocalData, or data from FShaderCompilerEnvironment
	size_t DataLength = 0;
	size_t DataCapacity = 0;
	FShaderSharedAnsiStringPtr SharedData;
	TArray<ANSICHAR> LocalData;
};

struct FStbPreprocessContext
{
	const FShaderCompilerInput& ShaderInput;
	const FShaderCompilerEnvironment& Environment;
	TMap<FString, FStbLoadedInclude> LoadedIncludesCache;
	TMap<FString, TUniquePtr<ANSICHAR[]>> SeenPathsLowerCase;

	bool HasIncludedMandatoryHeaders()
	{
		return SeenPathsLowerCase.Contains(PlatformHeaderLowerCase);
	}
};

static const ANSICHAR* StbLoadFile(const ANSICHAR* Filename, void* RawContext, size_t* OutLength)
{
	FStbPreprocessContext& Context = *reinterpret_cast<FStbPreprocessContext*>(RawContext);
	FString FilenameConverted = StringCast<TCHAR>(Filename).Get();
	uint32 FilenameConvertedHash = GetTypeHash(FilenameConverted);
	FStbLoadedInclude* ContentsCached = Context.LoadedIncludesCache.FindByHash(FilenameConvertedHash, FilenameConverted);
	if (!ContentsCached)
	{
		ContentsCached = &Context.LoadedIncludesCache.AddByHash(FilenameConvertedHash, FilenameConverted);

		const FString* InMemorySource = Context.Environment.IncludeVirtualPathToContentsMap.FindByHash(FilenameConvertedHash, FilenameConverted);

		if (InMemorySource)
		{
			check(!InMemorySource->IsEmpty());
			ShaderConvertAndStripComments(*InMemorySource, ContentsCached->LocalData);

			ContentsCached->Data = ContentsCached->LocalData.GetData();
			ContentsCached->DataLength = ContentsCached->LocalData.Num();
			ContentsCached->DataCapacity = ContentsCached->LocalData.Max();
		}
		else
		{
			const FThreadSafeSharedAnsiStringPtr* InMemorySourceAnsi = Context.Environment.IncludeVirtualPathToSharedContentsMap.FindByHash(FilenameConvertedHash, FilenameConverted);

			if (InMemorySourceAnsi)
			{
				ContentsCached->Data = InMemorySourceAnsi->Get()->GetData();
				ContentsCached->DataLength = InMemorySourceAnsi->Get()->Num();
				ContentsCached->DataCapacity = InMemorySourceAnsi->Get()->Max();
			}
			else
			{
				CheckShaderHashCacheInclude(FilenameConverted, Context.ShaderInput.Target.GetPlatform(), Context.ShaderInput.ShaderFormat.ToString());
				LoadShaderSourceFile(*FilenameConverted, Context.ShaderInput.Target.GetPlatform(), nullptr, nullptr, nullptr, &ContentsCached->SharedData);

				ContentsCached->Data = ContentsCached->SharedData->GetData();
				ContentsCached->DataLength = ContentsCached->SharedData->Num();
				ContentsCached->DataCapacity = ContentsCached->SharedData->Max();
			}
		}

		// Need 15 characters beyond null terminator, so an unaligned SSE read at the null terminator can safely read 15 extra unused characters
		// without going out of memory bounds.  ShaderConvertAndStripComments ensures this padding.  We could optionally allocate (or reallocate)
		// a local copy as a fallback to handle this case without asserting, but it would be a silent performance degradation.
		checkf(ContentsCached->DataCapacity >= ContentsCached->DataLength + 15, TEXT("Shader preprocessor ANSI files must include 15 bytes of capacity padding past null terminator"));
	}
	check(ContentsCached);
	*OutLength = ContentsCached->DataLength;
	return ContentsCached->Data;
}

static void StbFreeFile(const ANSICHAR* Filename, const ANSICHAR* Contents, void* RawContext)
{
	// No-op; stripped/converted shader source will be freed from the cache in FStbPreprocessContext when it's destructed;
	// we want to keep it around until that point in case includes are loaded multiple times from different source locations
}

static const ANSICHAR* StbResolveInclude(const ANSICHAR* PathInSource, uint32 PathLen, const ANSICHAR* ParentPathAnsi, void* RawContext)
{
	FStbPreprocessContext& Context = *reinterpret_cast<FStbPreprocessContext*>(RawContext);
	FString PathModified(PathLen, PathInSource);
	FString ParentFolder(ParentPathAnsi);
	ParentFolder = FPaths::GetPath(ParentFolder);
	if (!PathModified.StartsWith(TEXT("/"))) // if path doesn't start with / it's relative, if so append the parent's folder and collapse any relative dirs
	{
		PathModified = ParentFolder / PathModified;
		FPaths::CollapseRelativeDirectories(PathModified);
	}

	FixupShaderFilePath(PathModified, Context.ShaderInput.Target.GetPlatform(), &Context.ShaderInput.ShaderPlatformName);

	FString PathModifiedLowerCase = PathModified.ToLower();
	const TUniquePtr<ANSICHAR[]>* SeenPath = Context.SeenPathsLowerCase.Find(PathModifiedLowerCase);
	// Keep track of previously resolved paths in a case insensitive manner so preprocessor will handle #pragma once with files included with inconsistent casing correctly
	// (we store the first correctly resolved path with original casing so we get "nice" line directives)
	if (SeenPath)
	{
		return SeenPath->Get();
	}

	bool bExists =
		Context.Environment.IncludeVirtualPathToContentsMap.Contains(PathModified) ||
		Context.Environment.IncludeVirtualPathToSharedContentsMap.Contains(PathModified) ||
		// LoadShaderSourceFile will load the file if it exists, but then cache it internally, so the next call in StbLoadFile will be cheap
		// (and hence this is not wasteful, just performs the loading earlier)
		LoadShaderSourceFile(*PathModified, Context.ShaderInput.Target.GetPlatform(), nullptr, nullptr);

	if (bExists)
	{
		int32 Length = FPlatformString::ConvertedLength<ANSICHAR>(*PathModified);
		TUniquePtr<ANSICHAR[]>& OutPath = Context.SeenPathsLowerCase.Add(PathModifiedLowerCase, MakeUnique<ANSICHAR[]>(Length));
		FPlatformString::Convert<TCHAR, ANSICHAR>(OutPath.Get(), Length, *PathModified);
		return OutPath.Get();
	}

	return nullptr;
}

class FShaderPreprocessorModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		init_preprocessor(&StbLoadFile, &StbFreeFile, &StbResolveInclude);
		// disable the "directive not at start of line" error; this allows a few things:
		// 1. #define'ing #pragma messages - consumed by the preprocessor (to handle UESHADERMETADATA hackery)
		// 2. #define'ing other #pragmas (those not processed explicitly by the preprocessor are copied into the preprocessed code
		// 3. handling the HLSL infinity constant (1.#INF); STB preprocessor interprets any use of # as a directive which is not the case here
		pp_set_warning_mode(PP_RESULT_directive_not_at_start_of_line, PP_RESULT_MODE_no_warning); 
	}
};
IMPLEMENT_MODULE(FShaderPreprocessorModule, ShaderPreprocessor);

static void AddStbDefine(stb_arena* MacroArena, macro_definition**& StbDefines, const TCHAR* Name, const TCHAR* Value)
{
	TAnsiStringBuilder<256> Define;

	// Define format:  "%s %s"  (Name Value)
	Define.Append(Name);
	Define.AppendChar(' ');
	Define.Append(Value);

	arrput(StbDefines, pp_define(MacroArena, Define.ToString()));
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS		// FShaderCompilerDefinitions will be made internal in the future, marked deprecated until then

static void AddStbDefines(stb_arena* MacroArena, macro_definition**& StbDefines, const FShaderCompilerDefinitions& Defines)
{
	for (FShaderCompilerDefinitions::FConstIterator It(Defines); It; ++It)
	{
		AddStbDefine(MacroArena, StbDefines, It.Key(), It.Value());
	}
}

bool InnerPreprocessShaderStb(
	FShaderPreprocessOutput& Output,
	const FShaderCompilerInput& Input,
	const FShaderCompilerEnvironment& Environment,
	const FShaderCompilerDefinitions& AdditionalDefines
)
{
	stb_arena MacroArena = { 0 };
	macro_definition** StbDefines = nullptr;
	FShaderPreprocessorUtilities::PopulateDefines(Environment, AdditionalDefines, &MacroArena, StbDefines);

	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	FStbPreprocessContext Context{ Input, Environment };

	auto InFilename = StringCast<ANSICHAR>(*Input.VirtualSourceFilePath);
	int NumDiagnostics = 0;
	pp_diagnostic* Diagnostics = nullptr;

	static const int32 ThreadLocalPreprocessBufferSize = CVarShaderCompilerThreadLocalPreprocessBuffer.GetValueOnAnyThread();
	static thread_local char* ThreadLocalPreprocessBuffer = nullptr;

	// Sanity check the buffer size so it won't OOM if a bad value is entered.
	int32 ClampedPreprocessBufferSize = ThreadLocalPreprocessBufferSize ? FMath::Clamp(ThreadLocalPreprocessBufferSize, 64 * 1024, 4 * 1024 * 1024) : 0;
	if (ClampedPreprocessBufferSize && !ThreadLocalPreprocessBuffer)
	{
		ThreadLocalPreprocessBuffer = new char[ClampedPreprocessBufferSize];
	}

	char* OutPreprocessedAnsi = preprocess_file(InFilename.Get(), &Context, StbDefines, arrlen(StbDefines), &Diagnostics, &NumDiagnostics, ThreadLocalPreprocessBuffer, ClampedPreprocessBufferSize);

	bool HasError = false;
	if (Diagnostics != nullptr)
	{
		for (int DiagIndex = 0; DiagIndex < NumDiagnostics; ++DiagIndex)
		{
			pp_diagnostic* Diagnostic = &Diagnostics[DiagIndex];
			HasError |= (Diagnostic->error_level == PP_RESULT_MODE_error);
			
			FString Message = Diagnostic->message;
			// ignore stb warnings (for now?)
			if (Diagnostic->error_level == PP_RESULT_MODE_error)
			{
				FString Filename = Diagnostic->where->filename;
				Output.LogError(MoveTemp(Filename), MoveTemp(Message), Diagnostic->where->line_number);
			}
			else
			{
				EMessageType Type = FilterPreprocessorError(Message);
				if (Type == EMessageType::ShaderMetaData)
				{
					FString Directive;
					ExtractDirective(Directive, Message);
					Output.AddDirective(MoveTemp(Directive));
				}
			}
		}
	}

	if (!HasError)
	{
		// "preprocessor_file_size" includes null terminator, so subtract one for Append call -- passing size saves an expensive strlen in Append
		Output.EditSource().Append(OutPreprocessedAnsi, preprocessor_file_size(OutPreprocessedAnsi) - 1);
	}

	if (!HasError && !Context.HasIncludedMandatoryHeaders())
	{
		LogMandatoryHeaderError(Input, Output);
		HasError = true;
	}

	preprocessor_file_free(OutPreprocessedAnsi, Diagnostics);
	stbds_arrfree(StbDefines);
	stb_arena_free(&MacroArena);

	return !HasError;
}

bool PreprocessShader(
	FString& OutPreprocessedShader,
	FShaderCompilerOutput& ShaderOutput,
	const FShaderCompilerInput& ShaderInput,
	PRAGMA_DISABLE_DEPRECATION_WARNINGS		// FShaderCompilerDefinitions will be made internal in the future, marked deprecated until then
	const FShaderCompilerDefinitions& AdditionalDefines,
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	EDumpShaderDefines DefinesPolicy)
{
	FShaderPreprocessOutput Output;
	// when called via this overload, environment is assumed to be already merged in input struct
	const FShaderCompilerEnvironment& Environment = ShaderInput.Environment;
	bool bSucceeded = PreprocessShader(Output, ShaderInput, Environment, AdditionalDefines, DefinesPolicy);

	OutPreprocessedShader = MoveTemp(Output.EditSource());

	Output.MoveDirectives(ShaderOutput.PragmaDirectives);
	for (FShaderCompilerError& Error : Output.EditErrors())
	{
		ShaderOutput.Errors.Add(MoveTemp(Error));
	}
	return bSucceeded;
}

/**
 * Preprocess a shader.
 * @param OutPreprocessedShader - Upon return contains the preprocessed source code.
 * @param ShaderOutput - ShaderOutput to which errors can be added.
 * @param ShaderInput - The shader compiler input.
 * @param AdditionalDefines - Additional defines with which to preprocess the shader.
 * @param DefinesPolicy - Whether to add shader definitions as comments.
 * @returns true if the shader is preprocessed without error.
 */
bool PreprocessShader(
	FShaderPreprocessOutput& Output,
	const FShaderCompilerInput& Input,
	const FShaderCompilerEnvironment& Environment,
	PRAGMA_DISABLE_DEPRECATION_WARNINGS		// FShaderCompilerDefinitions will be made internal in the future, marked deprecated until then
	const FShaderCompilerDefinitions& AdditionalDefines,
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	EDumpShaderDefines DefinesPolicy
)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(PreprocessShader);

	// Skip the cache system and directly load the file path (used for debugging)
	if (Input.bSkipPreprocessedCache)
	{
		return FFileHelper::LoadFileToString(Output.EditSource(), *Input.VirtualSourceFilePath);
	}

	check(CheckVirtualShaderFilePath(Input.VirtualSourceFilePath));

	Output.EditSource().Empty();

	// List the defines used for compilation in the preprocessed shaders, especially to know which permutation vector this shader is.
	if (DefinesPolicy == EDumpShaderDefines::AlwaysIncludeDefines || (DefinesPolicy == EDumpShaderDefines::DontCare && Input.DumpDebugInfoPath.Len() > 0))
	{
		FShaderPreprocessorUtilities::DumpShaderDefinesAsCommentedCode(Environment, &Output.EditSource());
	}

	return InnerPreprocessShaderStb(Output, Input, Environment, AdditionalDefines);
}
