// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRCreativeToolset.h"

#include "InputMappingContext.h"
#include "XRCreativeLog.h"
#include "Logging/LogMacros.h"

#if WITH_EDITOR
#	include "XRCreativeSettings.h"
#endif

DEFINE_LOG_CATEGORY(LogXRCreativeToolset);

UInputMappingContext* UXRCreativeBlueprintableTool::GetToolInputMappingContext()
{
	UInputMappingContext* InputMappingContext = DefaultToolInputMappingContext;
	
	#if WITH_EDITOR
		UXRCreativeEditorSettings* Settings = UXRCreativeEditorSettings::GetXRCreativeEditorSettings();

		if (Settings->Handedness == EXRCreativeHandedness::Left && LeftToolInputMappingContext)
		{
			InputMappingContext = LeftToolInputMappingContext;
		}
		else if (Settings->Handedness == EXRCreativeHandedness::Left && !LeftToolInputMappingContext)
		{
			InputMappingContext = DefaultToolInputMappingContext;
			UE_LOG(LogXRCreativeToolset, Warning, TEXT("Handedness is Left but no Left Input Mapping Context found in Toolset - Using Default."));
		}
	
	#endif
	
	return InputMappingContext; 
}


