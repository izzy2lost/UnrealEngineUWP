// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectEditorFunctionLibrary.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/ICustomizableObjectEditorModule.h"

ECustomizableObjectCompilationState UCustomizableObjectEditorFunctionLibrary::CompileCustomizableObjectSynchronously(
	UCustomizableObject* CustomizableObject,
	ECustomizableObjectOptimizationLevel InOptimizationLevel,
	ECustomizableObjectTextureCompression InTextureCompression)
{
	// store package dirty state so that we can restore it - compile is not an edit:
	const bool bPackageWasDirty = CustomizableObject->GetOutermost()->IsDirty();

	const double StartTime = FPlatformTime::Seconds();

	TSharedRef<FCompilationRequest> CompileRequest = MakeShared<FCompilationRequest>(*CustomizableObject, false);
	FCompilationOptions& Options = CompileRequest->GetCompileOptions();
	Options.OptimizationLevel = static_cast<int32>(InOptimizationLevel);
	Options.TextureCompression = InTextureCompression;
	Options.bSilentCompilation = false;
	ICustomizableObjectEditorModule::GetChecked().CompileCustomizableObject(CompileRequest);

	check(CompileRequest->GetCompilationState() == ECompilationStatePrivate::Completed);

	CustomizableObject->GetOutermost()->SetDirtyFlag(bPackageWasDirty);

	const bool bCompilationSuccess = CompileRequest->GetCompilationResult() == ECompilationResultPrivate::Success ||
		CompileRequest->GetCompilationResult() == ECompilationResultPrivate::Warnings;
	
	const double CurrentTime = FPlatformTime::Seconds();
	UE_LOG( LogMutable, Display,
		TEXT("Synchronously Compiled %s %s in %f seconds"),
		*GetPathNameSafe(CustomizableObject), 
		bCompilationSuccess ? TEXT("successfully") : TEXT("unsuccessfully"),
		CurrentTime - StartTime
	);

	if (!CustomizableObject->IsCompiled())
	{
		UE_LOG(LogMutable, Warning, TEXT("CO not marked as compiled"));
	}

	return bCompilationSuccess ? ECustomizableObjectCompilationState::Completed : ECustomizableObjectCompilationState::Failed;
}
