// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "MaterialSourceTemplate.h"
#include "Misc/ScopeRWLock.h"

FMaterialSourceTemplate& FMaterialSourceTemplate::Get()
{
	static FMaterialSourceTemplate Instance;
	return Instance;
}

FStringTemplateResolver FMaterialSourceTemplate::BeginResolve(EShaderPlatform ShaderPlatform, int32* MaterialTemplateLineNumber)
{
	Preload(ShaderPlatform);

	if (MaterialTemplateLineNumber)
	{
		*MaterialTemplateLineNumber = MaterialTemplateLineNumbers[ShaderPlatform];
	}

	return { Templates[ShaderPlatform], 50 * 1024 };
}

const FSHA1& FMaterialSourceTemplate::GetTemplateHash(EShaderPlatform ShaderPlatform)
{
	Preload(ShaderPlatform);
	return TemplateHash[ShaderPlatform];
}

bool FMaterialSourceTemplate::Preload(EShaderPlatform ShaderPlatform)
{
	// Is the material source template already loaded?
	{
		FReadScopeLock Lock{ RWLocks[ShaderPlatform] };
		if (!Templates[ShaderPlatform].GetTemplateString().IsEmpty())
		{
			return true;
		}
	}

	// Material source template not yet loaded. Acquire a write lock an try again.
	FWriteScopeLock Lock{ RWLocks[ShaderPlatform] };
	if (!Templates[ShaderPlatform].GetTemplateString().IsEmpty())
	{
		return true;
	}

	FString MaterialTemplateString;
	LoadShaderSourceFileChecked(TEXT("/Engine/Private/MaterialTemplate.ush"), ShaderPlatform, MaterialTemplateString);

	// Find the string index of the '#line' statement in MaterialTemplate.usf
	const int32 LineIndex = MaterialTemplateString.Find(TEXT("#line"), ESearchCase::CaseSensitive);
	check(LineIndex != INDEX_NONE);

	// Count line endings before the '#line' statement
	int32 TemplateLineNumber = INDEX_NONE;
	int32 StartPosition = LineIndex + 1;
	do
	{
		TemplateLineNumber++;
		// Using \n instead of LINE_TERMINATOR as not all of the lines are terminated consistently
		// Subtract one from the last found line ending index to make sure we skip over it
		StartPosition = MaterialTemplateString.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, StartPosition - 1);
	} while (StartPosition != INDEX_NONE);

	check(TemplateLineNumber != INDEX_NONE);

	// At this point MaterialTemplateLineNumber is one less than the line number of the '#line' statement
	// For some reason we have to add 2 more to the #line value to get correct error line numbers from D3DXCompileShader
	TemplateLineNumber += 3;

	// Save the material template line numbers for this shader platform
	MaterialTemplateLineNumbers[ShaderPlatform] = TemplateLineNumber;

	// Load the material string template
	FStringTemplate::FErrorInfo ErrorInfo;
	if (!Templates[ShaderPlatform].Load(MoveTemp(MaterialTemplateString), ErrorInfo))
	{
		UE_LOG(LogMaterial, Error, TEXT("Error in MaterialTemplate.ush source template at line %d offset %d: %s"), ErrorInfo.Line, ErrorInfo.Offset, ErrorInfo.Message.GetData());
		return false;
	}

	// Extract the material template string TemplateVersion parameter
	const TStringView TemplateVersionKeyword = TEXT("$TemplateVersion{");
	int Begin = MaterialTemplateString.Find(TemplateVersionKeyword.GetData());
	int End = MaterialTemplateString.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Begin + TemplateVersionKeyword.Len());
	if (Begin > 0 && End > 0)
	{
		Begin += TemplateVersionKeyword.Len();
		TemplateHash[ShaderPlatform].UpdateWithString(*MaterialTemplateString + Begin, End - Begin);
	}

	TArray<FStringView> Parameters;
	Templates[ShaderPlatform].GetParameters(Parameters);
	Parameters.Sort();
	for (const FStringView& Param : Parameters)
	{
		TemplateHash[ShaderPlatform].UpdateWithString(Param.GetData(), Param.Len());
	}

	return true;
}

#endif // WITH_EDITOR
