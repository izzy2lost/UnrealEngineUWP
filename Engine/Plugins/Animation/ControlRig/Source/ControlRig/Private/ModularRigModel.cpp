// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularRigModel.h"
#include "ModularRigController.h"
#include "ModularRig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularRigModel)

FString FRigModuleReference::GetShortName() const
{
	if(!ShortName.IsEmpty())
	{
		return ShortName;
	}
	return GetPath();
}

FString FRigModuleReference::GetPath() const
{
	if (ParentPath.IsEmpty())
	{
		return Name.ToString();
	}
	return ParentPath + UModularRig::NamespaceSeparator + Name.ToString();
}

FString FRigModuleReference::GetNamespace() const
{
	return GetPath() + UModularRig::NamespaceSeparator;
}

UModularRigController* FModularRigModel::GetController(bool bCreateIfNeeded)
{
	if (bCreateIfNeeded && Controller == nullptr)
	{
		const FName SafeControllerName = *FString::Printf(TEXT("%s_ModularRig_Controller"), *GetOuter()->GetPathName());
		UModularRigController* NewController = NewObject<UModularRigController>(GetOuter(), UModularRigController::StaticClass(), SafeControllerName);
		NewController->SetModel(this);
		Controller = NewController;
	}
	return Cast<UModularRigController>(Controller);
}

void FModularRigModel::SetOuterClientHost(UObject* InOuterClientHost)
{
	OuterClientHost = InOuterClientHost;
}

void FModularRigModel::UpdateCachedChildren()
{
	TMap<FString, FRigModuleReference*> PathToModule;
	for (FRigModuleReference& Module : Modules)
	{
		Module.CachedChildren.Reset();
		PathToModule.Add(Module.GetPath(), &Module);
	}
	
	RootModules.Reset();
	for (FRigModuleReference& Module : Modules)
	{
		if (Module.ParentPath.IsEmpty())
		{
			RootModules.Add(&Module);
		}
		else
		{
			if (FRigModuleReference** ParentModule = PathToModule.Find(Module.ParentPath))
			{
				(*ParentModule)->CachedChildren.Add(&Module);
			}
		}
	}
}

FRigModuleReference* FModularRigModel::FindModule(const FString InPath) const
{
	const TArray<FRigModuleReference*>* Children = &RootModules;

	FString Left = InPath, Right;
	while (Left.Split(UModularRig::NamespaceSeparator, &Left, &Right))
	{
		FRigModuleReference* const * Child = Children->FindByPredicate([Left](FRigModuleReference* Module)
		{
			return Module->Name.ToString() == Left;
		});
		
		if (!Child)
		{
			return nullptr;
		}
		
		Children = &(*Child)->CachedChildren;
		Left = Right;
	}

	FRigModuleReference* const * Child = Children->FindByPredicate([Left](FRigModuleReference* Module)
		{
			return Module->Name.ToString() == Left;
		});

	if (!Child)
	{
		return nullptr;
	}
	return *Child;
}

FString FModularRigModel::FindParentPath(const FString InPath) const
{
	if (FRigModuleReference* Element = FindModule(InPath))
	{
		return Element->ParentPath;
	}
	return FString();
}

void FModularRigModel::ForEachModule(TFunction<bool(const FRigModuleReference*)> PerModule) const
{
	TArray<FRigModuleReference*> ModuleInstances = RootModules;
	for (int32 Index=0; Index < ModuleInstances.Num(); ++Index)
	{
		if (!PerModule(ModuleInstances[Index]))
		{
			break;
		}
		ModuleInstances.Append(ModuleInstances[Index]->CachedChildren);
	}
}

TArray<FString> FModularRigModel::SortPaths(const TArray<FString>& InPaths) const
{
	TArray<FString> Result;
	ForEachModule([InPaths, &Result](const FRigModuleReference* Element) -> bool
	{
		const FString& Path = Element->GetPath();
		if(InPaths.Contains(Path))
		{
			Result.AddUnique(Path);
		}
		return true;
	});
	return Result;
}
