// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigBlueprintEditorLibrary.h"
#include "Editor/SRigHierarchy.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "RigVMModel/RigVMPin.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigBlueprintEditorLibrary)

void UControlRigBlueprintEditorLibrary::CastToControlRigBlueprint(
	UObject* Object,
	ECastToControlRigBlueprintCases& Branches,
	UControlRigBlueprint*& AsControlRigBlueprint)
{
	AsControlRigBlueprint = Cast<UControlRigBlueprint>(Object);
	Branches = AsControlRigBlueprint == nullptr ? 
		ECastToControlRigBlueprintCases::CastFailed : 
		ECastToControlRigBlueprintCases::CastSucceeded;
}

void UControlRigBlueprintEditorLibrary::SetPreviewMesh(UControlRigBlueprint* InRigBlueprint, USkeletalMesh* PreviewMesh, bool bMarkAsDirty)
{
	if(InRigBlueprint == nullptr)
	{
		return;
	}
	InRigBlueprint->SetPreviewMesh(PreviewMesh, bMarkAsDirty);
}

USkeletalMesh* UControlRigBlueprintEditorLibrary::GetPreviewMesh(UControlRigBlueprint* InRigBlueprint)
{
	if(InRigBlueprint == nullptr)
	{
		return nullptr;
	}
	return InRigBlueprint->GetPreviewMesh();
}

void UControlRigBlueprintEditorLibrary::RecompileVM(UControlRigBlueprint* InRigBlueprint)
{
	if(InRigBlueprint == nullptr)
	{
		return;
	}
	InRigBlueprint->RecompileVM();
}

void UControlRigBlueprintEditorLibrary::RecompileVMIfRequired(UControlRigBlueprint* InRigBlueprint)
{
	if(InRigBlueprint == nullptr)
	{
		return;
	}
	InRigBlueprint->RecompileVMIfRequired();
}

void UControlRigBlueprintEditorLibrary::RequestAutoVMRecompilation(UControlRigBlueprint* InRigBlueprint)
{
	if(InRigBlueprint == nullptr)
	{
		return;
	}
	InRigBlueprint->RequestAutoVMRecompilation();
}

void UControlRigBlueprintEditorLibrary::RequestControlRigInit(UControlRigBlueprint* InRigBlueprint)
{
	if(InRigBlueprint == nullptr)
	{
		return;
	}
	InRigBlueprint->RequestRigVMInit();
}

URigVMGraph* UControlRigBlueprintEditorLibrary::GetModel(UControlRigBlueprint* InRigBlueprint)
{
	if(InRigBlueprint == nullptr)
	{
		return nullptr;
	}
	return InRigBlueprint->GetDefaultModel();
}

URigVMController* UControlRigBlueprintEditorLibrary::GetController(UControlRigBlueprint* InRigBlueprint)
{
	if(InRigBlueprint == nullptr)
	{
		return nullptr;
	}
	return InRigBlueprint->GetController();
}

TArray<UControlRigBlueprint*> UControlRigBlueprintEditorLibrary::GetCurrentlyOpenRigBlueprints()
{
	return UControlRigBlueprint::GetCurrentlyOpenRigBlueprints();
}

TArray<UStruct*> UControlRigBlueprintEditorLibrary::GetAvailableRigUnits()
{
	UControlRigBlueprint* CDO = CastChecked<UControlRigBlueprint>(UControlRigBlueprint::StaticClass()->GetDefaultObject());
	return CDO->GetAvailableRigVMStructs();
}

URigHierarchy* UControlRigBlueprintEditorLibrary::GetHierarchy(UControlRigBlueprint* InRigBlueprint)
{
	if(InRigBlueprint == nullptr)
	{
		return nullptr;
	}
	return InRigBlueprint->Hierarchy;
}

URigHierarchyController* UControlRigBlueprintEditorLibrary::GetHierarchyController(UControlRigBlueprint* InRigBlueprint)
{
	if(InRigBlueprint == nullptr)
	{
		return nullptr;
	}
	return InRigBlueprint->GetHierarchyController();
}

void UControlRigBlueprintEditorLibrary::SetupAllEditorMenus()
{
	SRigHierarchy::CreateContextMenu();
	SRigHierarchy::CreateDragDropMenu();
}

TArray<FRigModuleDescription> UControlRigBlueprintEditorLibrary::GetAvailableRigModules()
{
	TArray<FRigModuleDescription> ModuleDescriptions;
	
	// Load the asset registry module
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Collect a full list of assets with the specified class
	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssetsByClass(UControlRigBlueprint::StaticClass()->GetClassPathName(), AssetDataList, true);

	for(const FAssetData& AssetData : AssetDataList)
	{
		static const FName ModuleSettingsName = GET_MEMBER_NAME_CHECKED(UControlRigBlueprint, ModuleSettings);
		if(AssetData.FindTag(ModuleSettingsName))
		{
			const FString TagValue = AssetData.GetTagValueRef<FString>(ModuleSettingsName);
			if(TagValue.IsEmpty())
			{
				FRigModuleDescription ModuleDescription;
				ModuleDescription.Path = AssetData.ToSoftObjectPath();
				
				FRigVMPinDefaultValueImportErrorContext ErrorPipe;
				FRigModuleSettings::StaticStruct()->ImportText(*TagValue, &ModuleDescription.Settings, nullptr, PPF_None, &ErrorPipe, FString());
				if(ErrorPipe.NumErrors == 0)
				{
					if(ModuleDescription.Settings.IsValidModule())
					{
						ModuleDescriptions.Add(ModuleDescription);
					}
				}
			}
		}
	}
	
	return ModuleDescriptions;
}

