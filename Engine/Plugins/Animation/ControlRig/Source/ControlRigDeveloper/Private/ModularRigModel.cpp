// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularRigModel.h"
#include "ModularRig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularRigModel)

FString FRigModuleReference::GetNamespace() const
{
	if (ParentNamespace.IsEmpty())
	{
		return Name.ToString() + UModularRig::NamespaceSeparator;
	}
	return ParentNamespace + Name.ToString() + UModularRig::NamespaceSeparator;
}

UModularRigController* FModularRigModel::GetController(bool bCreateIfNeeded)
{
	if (Controller)
	{
		return Controller;
	}

	if (bCreateIfNeeded)
	{
		const FName SafeControllerName = *FString::Printf(TEXT("%s_ModularRig_Controller"), *GetOuter()->GetPathName());
		Controller = NewObject<UModularRigController>(GetOuter(), UModularRigController::StaticClass(), SafeControllerName);
		Controller->SetModel(this);
	}
	return Controller;
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
		PathToModule.Add(Module.GetNamespace(), &Module);
	}
	
	RootModules.Reset();
	for (FRigModuleReference& Module : Modules)
	{
		if (Module.ParentNamespace.IsEmpty())
		{
			RootModules.Add(&Module);
		}
		else
		{
			if (FRigModuleReference** ParentModule = PathToModule.Find(Module.ParentNamespace))
			{
				(*ParentModule)->CachedChildren.Add(&Module);
			}
		}
	}
}

FRigModuleReference* FModularRigModel::FindModule(const FString InNameSpace)
{
	TArray<FRigModuleReference*>* Children = &RootModules;

	FString Left = InNameSpace, Right;
	while (Left.Split(UModularRig::NamespaceSeparator, &Left, &Right))
	{
		FRigModuleReference** Child = Children->FindByPredicate([Left](FRigModuleReference* Module)
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

	FRigModuleReference** Child = Children->FindByPredicate([Left](FRigModuleReference* Module)
		{
			return Module->Name.ToString() == Left;
		});

	if (!Child)
	{
		return nullptr;
	}
	return *Child;
}
