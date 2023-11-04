// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMDeveloperProjectSettings.h"

#include "BlueprintEditorSettings.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MVVMBlueprintViewModelContext.h"
#include "PropertyPermissionList.h"
#include "Types/MVVMExecutionMode.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "MVVMDeveloperProjectSettings"

UMVVMDeveloperProjectSettings::UMVVMDeveloperProjectSettings()
{
	AllowedExecutionMode.Add(EMVVMExecutionMode::Immediate);
	AllowedExecutionMode.Add(EMVVMExecutionMode::Delayed);
	AllowedExecutionMode.Add(EMVVMExecutionMode::Tick);
	AllowedExecutionMode.Add(EMVVMExecutionMode::DelayedWhenSharedElseImmediate);
	
	AllowedContextCreationType.Add(EMVVMBlueprintViewModelContextCreationType::Manual);
	AllowedContextCreationType.Add(EMVVMBlueprintViewModelContextCreationType::CreateInstance);
	AllowedContextCreationType.Add(EMVVMBlueprintViewModelContextCreationType::GlobalViewModelCollection);
	AllowedContextCreationType.Add(EMVVMBlueprintViewModelContextCreationType::PropertyPath);
	AllowedContextCreationType.Add(EMVVMBlueprintViewModelContextCreationType::PropertyPath);
	AllowedContextCreationType.Add(EMVVMBlueprintViewModelContextCreationType::Resolver);
}

FName UMVVMDeveloperProjectSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FText UMVVMDeveloperProjectSettings::GetSectionText() const
{
	return LOCTEXT("MVVMProjectSettings", "UMG Model View Viewmodel");
}

bool UMVVMDeveloperProjectSettings::PropertyHasFiltering(const FProperty* Property) const
{
	check(Property);
	const UStruct* ObjectStruct = nullptr;

	if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
	{
		ObjectStruct = ObjectProperty->PropertyClass;
	}
	else
	{
		ObjectStruct = Property->GetOwnerStruct();
	}

	if (!FPropertyEditorPermissionList::Get().HasFiltering(ObjectStruct))
	{
		return false;
	}

	TStringBuilder<512> StringBuilder;
	Property->GetOwnerClass()->GetPathName(nullptr, StringBuilder);
	FSoftClassPath StructPath;
	StructPath.SetPath(StringBuilder);
	if (const FMVVMDeveloperProjectWidgetSettings* Settings = FieldSelectorPermissions.Find(StructPath))
	{
		return !Settings->DisallowedFieldNames.Find(Property->GetFName());
	}
	return true;
}

namespace UE::MVVM::Private
{
bool ShouldDoPropertyEditorPermission(const UBlueprint* GeneratingFor, const UClass* FieldOwner)
{
	if (GeneratingFor && FieldOwner)
	{
		const UClass* UpToDateClass = FBlueprintEditorUtils::GetMostUpToDateClass(FieldOwner);
		return GeneratingFor->SkeletonGeneratedClass != UpToDateClass;
	}
	return true;
}
}//namespace

bool UMVVMDeveloperProjectSettings::IsPropertyAllowed(const UBlueprint* GeneratingFor, const FProperty* Property) const
{
	check(Property);
	check(GeneratingFor);

	const UStruct* OwnerStruct = Property->GetOwnerStruct();
	check(OwnerStruct);

	// The editor permission doesn't work with skeletal class
	const UClass* OwnerClass = Cast<UClass>(OwnerStruct);
	if (OwnerClass)
	{
		OwnerClass = OwnerClass->GetAuthoritativeClass();
		OwnerStruct = OwnerClass;
	}

	const bool bDoPropertyEditorPermission = UE::MVVM::Private::ShouldDoPropertyEditorPermission(GeneratingFor, OwnerClass);
	if (bDoPropertyEditorPermission)
	{
		if (!FPropertyEditorPermissionList::Get().DoesPropertyPassFilter(OwnerStruct, Property->GetFName()))
		{
			return false;
		}
	}

	if (OwnerClass)
	{
		TStringBuilder<512> StringBuilder;
		OwnerClass->GetPathName(nullptr, StringBuilder);
		FSoftClassPath StructPath;
		StructPath.SetPath(StringBuilder);
		if (const FMVVMDeveloperProjectWidgetSettings* Settings = FieldSelectorPermissions.Find(StructPath))
		{
			return !Settings->DisallowedFieldNames.Find(Property->GetFName());
		}
	}
	return true;
}

bool UMVVMDeveloperProjectSettings::IsFunctionAllowed(const UBlueprint* GeneratingFor, const UFunction* Function) const
{
	if (Function == nullptr)
	{
		return false;
	}

	const UClass* OriginalOwnerClass = Function->GetOwnerClass();
	const UClass* OwnerClass = OriginalOwnerClass ? OriginalOwnerClass->GetAuthoritativeClass() : nullptr;
	if (OwnerClass == nullptr)
	{
		return false;
	}

	TStringBuilder<512> StringBuilder;
	const FPathPermissionList& FunctionPermissions = GetMutableDefault<UBlueprintEditorSettings>()->GetFunctionPermissions();
	if (FunctionPermissions.HasFiltering())
	{
		const UFunction* FunctionToTest = Function;
		if (OriginalOwnerClass != OwnerClass)
		{
			FunctionToTest = OwnerClass->FindFunctionByName(Function->GetFName());
		}
		if (FunctionToTest == nullptr)
		{
			return false;
		}

		StringBuilder.Reset();
		FunctionToTest->GetPathName(nullptr, StringBuilder);
		if (!FunctionPermissions.PassesFilter(StringBuilder.ToView()))
		{
			return false;
		}
	}

	{
		StringBuilder.Reset();
		OwnerClass->GetPathName(nullptr, StringBuilder);
		FSoftClassPath StructPath;
		StructPath.SetPath(StringBuilder);
		if (const FMVVMDeveloperProjectWidgetSettings* Settings = FieldSelectorPermissions.Find(StructPath))
		{
			return !Settings->DisallowedFieldNames.Find(Function->GetFName());
		}
	}

	return true;
}

bool UMVVMDeveloperProjectSettings::IsConversionFunctionAllowed(const UBlueprint* GeneratingFor, const UFunction* Function) const
{
	static FName NAME_ComplexConversionFunction = TEXT("MVVMComplexConversionFunction");
	if (Function->HasMetaData(NAME_ComplexConversionFunction))
	{
		return true;
	}

	if (ConversionFunctionFilter == EMVVMDeveloperConversionFunctionFilterType::BlueprintActionRegistry)
	{
		return IsFunctionAllowed(GeneratingFor, Function);
	}
	else
	{
		check(ConversionFunctionFilter == EMVVMDeveloperConversionFunctionFilterType::AllowedList);

		if (Function->HasAllFunctionFlags(FUNC_Static))
		{
			TStringBuilder<512> FunctionClassPath;
			Function->GetOwnerClass()->GetPathName(nullptr, FunctionClassPath);
			TStringBuilder<512> AllowedClassPath;
			for (const FSoftClassPath& SoftClass : AllowedClassForConversionFunctions)
			{
				SoftClass.ToString(AllowedClassPath);
				if (AllowedClassPath.ToView() == FunctionClassPath.ToView())
				{
					return true;
				}
				AllowedClassPath.Reset();
			}

			return false;
		}
		else
		{
			// The function is on self and may have been filtered.
			return IsFunctionAllowed(GeneratingFor, Function);
		}
	}
}

TArray<const UClass*> UMVVMDeveloperProjectSettings::GetAllowedConversionFunctionClasses() const
{
	TArray<const UClass*> Result;
	for (const FSoftClassPath& SoftClass : AllowedClassForConversionFunctions)
	{
		if (UClass* Class = SoftClass.ResolveClass())
		{
			Result.Add(Class);
		}
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
