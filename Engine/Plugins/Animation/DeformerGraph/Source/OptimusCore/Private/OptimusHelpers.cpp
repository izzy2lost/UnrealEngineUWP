// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusHelpers.h"

#include "OptimusDataTypeRegistry.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "ShaderParameterMetadataBuilder.h"
#include "ShaderParameterMetadata.h"
#include "Engine/UserDefinedStruct.h"

FName Optimus::GetUniqueNameForScope(UObject* InScopeObj, FName InName)
{
	// If there's already an object with this name, then attempt to make the name unique.
	// For some reason, MakeUniqueObjectName does not already do this check, hence this function.
	if (StaticFindObjectFast(UObject::StaticClass(), InScopeObj, InName) != nullptr)
	{
		InName = MakeUniqueObjectName(InScopeObj, UObject::StaticClass(), InName);
	}

	return InName;
}

FName Optimus::GetSanitizedNameForHlsl(FName InName)
{
	// Sanitize the name
	FString Name = InName.ToString();
	for (int32 i = 0; i < Name.Len(); ++i)
	{
		TCHAR& C = Name[i];

		const bool bGoodChar =
			FChar::IsAlpha(C) ||											// Any letter (upper and lowercase) anytime
			(C == '_') ||  													// _  
			((i > 0) && FChar::IsDigit(C));									// 0-9 after the first character

		if (!bGoodChar)
		{
			C = '_';
		}
	}

	return *Name;
}

bool Optimus::RenameObject(UObject* InObjectToRename, const TCHAR* InNewName, UObject* InNewOuter)
{
	return InObjectToRename->Rename(InNewName, InNewOuter, REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
}

void Optimus::RemoveObject(UObject* InObjectToRemove)
{
	InObjectToRemove->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
	InObjectToRemove->MarkAsGarbage();
}

TArray<UClass*> Optimus::GetClassObjectsInPackage(UPackage* InPackage)
{
	TArray<UObject*> Objects;
	GetObjectsWithOuter(InPackage, Objects, false);

	TArray<UClass*> ClassObjects;
	for (UObject* Object : Objects)
	{
		if (UClass* Class = Cast<UClass>(Object))
		{
			ClassObjects.Add(Class);
		}
	}

	return ClassObjects;
}

FText Optimus::GetTypeDisplayName(UScriptStruct* InStructType)
{
#if WITH_EDITOR
	FText DisplayName = InStructType->GetDisplayNameText();
#else
	FText DisplayName = FText::FromName(InStructType->GetFName());
#endif

	return DisplayName;
}

FName Optimus::GetMemberPropertyShaderName(UScriptStruct* InStruct, const FProperty* InMemberProperty)
{
	if (UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(InStruct))
	{
		// Remove Spaces
		FString ShaderMemberName = InStruct->GetAuthoredNameForField(InMemberProperty).Replace(TEXT(" "), TEXT(""));

		if (ensure(!ShaderMemberName.IsEmpty()))
		{
			// Sanitize the name, user defined struct can have members with names that start with numbers
			if (!FChar::IsAlpha(ShaderMemberName[0]) && !FChar::IsUnderscore(ShaderMemberName[0]))
			{
				ShaderMemberName = FString(TEXT("_")) + ShaderMemberName;
			}
		}

		return *ShaderMemberName;
	}

	return InMemberProperty->GetFName();
}

FName Optimus::GetTypeName(UScriptStruct* InStructType, bool bInShouldGetUniqueNameForUserDefinedStruct)
{
	if (UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(InStructType))
	{
		if (bInShouldGetUniqueNameForUserDefinedStruct)
		{
			return FName(*FString::Printf(TEXT("FUserDefinedStruct_%s"), *UserDefinedStruct->GetCustomGuid().ToString()));
		}
	}
	
	return FName(*InStructType->GetStructCPPName());
}

void Optimus::ConvertObjectPathToShaderFilePath(FString& InOutPath)
{
	// Shader compiler recognizes "/Engine/Generated/..." path as special. 
	// It doesn't validate file suffix etc.
	InOutPath = FString::Printf(TEXT("/Engine/Generated/UObject%s.ush"), *InOutPath);
	// Shader compilation result parsing will break if it finds ':' where it doesn't expect.
	InOutPath.ReplaceCharInline(TEXT(':'), TEXT('@'));
}

bool Optimus::ConvertShaderFilePathToObjectPath(FString& InOutPath)
{
	if (!InOutPath.RemoveFromStart(TEXT("/Engine/Generated/UObject")))
	{
		return false;
	}

	InOutPath.ReplaceCharInline(TEXT('@'), TEXT(':'));
	InOutPath.RemoveFromEnd(TEXT(".ush"));
	return true;
}

FString Optimus::GetCookedKernelSource(
	const FString& InObjectPathName,
	const FString& InShaderSource,
	const FString& InKernelName,
	FIntVector InGroupSize
	)
{
	// FIXME: Create source range mappings so that we can go from error location to
	// our source.
	FString Source = InShaderSource;

#if PLATFORM_WINDOWS
	// Remove old-school stuff.
	Source.ReplaceInline(TEXT("\r"), TEXT(""));
#endif

	FString ShaderPathName = InObjectPathName;
	Optimus::ConvertObjectPathToShaderFilePath(ShaderPathName);

	const bool bHasKernelKeyword = Source.Contains(TEXT("KERNEL"), ESearchCase::CaseSensitive);

	const FString ComputeShaderUtilsInclude = TEXT("#include \"/Engine/Private/ComputeShaderUtils.ush\"");
	
	const FString KernelFunc = FString::Printf(
		TEXT("[numthreads(%d,%d,%d)]\nvoid %s(uint3 GroupId : SV_GroupID, uint GroupIndex : SV_GroupIndex)"), 
		InGroupSize.X, InGroupSize.Y, InGroupSize.Z, *InKernelName);
	
	const FString UnWrappedDispatchThreadId = FString::Printf(
	TEXT("GetUnWrappedDispatchThreadId(GroupId, GroupIndex, %d)"),
		InGroupSize.X * InGroupSize.Y * InGroupSize.Z
	);

	if (bHasKernelKeyword)
	{
		Source.ReplaceInline(TEXT("KERNEL"), TEXT("void __kernel_func(uint Index)"), ESearchCase::CaseSensitive);

		return FString::Printf(
			TEXT(
				"#line 1 \"%s\"\n"
				"%s\n\n"
				"%s\n\n"
				"%s { __kernel_func(%s); }\n"
				), *ShaderPathName, *Source, *ComputeShaderUtilsInclude,*KernelFunc, *UnWrappedDispatchThreadId);
	}
	else
	{
		return FString::Printf(
		TEXT(
			"%s\n"
			"%s\n"
			"{\n"
			"uint Index = %s;\n"
			"#line 1 \"%s\"\n"
			"%s\n"
			"}\n"
			), *ComputeShaderUtilsInclude,*KernelFunc, *UnWrappedDispatchThreadId, *ShaderPathName, *Source);
	}
}
