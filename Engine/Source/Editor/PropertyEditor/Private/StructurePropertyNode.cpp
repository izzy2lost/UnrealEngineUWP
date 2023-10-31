// Copyright Epic Games, Inc. All Rights Reserved.


#include "StructurePropertyNode.h"
#include "ItemPropertyNode.h"
#include "PropertyEditorHelpers.h"

void FStructurePropertyNode::InitChildNodes()
{
	InternalInitChildNodes(FName());
}

void FStructurePropertyNode::InternalInitChildNodes(FName SinglePropertyName)
{
	const bool bShouldShowHiddenProperties = !!HasNodeFlags(EPropertyNodeFlags::ShouldShowHiddenProperties);
	const bool bShouldShowDisableEditOnInstance = !!HasNodeFlags(EPropertyNodeFlags::ShouldShowDisableEditOnInstance);

	const UStruct* Struct = GetBaseStructure();

	TArray<FProperty*> StructMembers;

	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		FProperty* StructMember = *It;
		if (PropertyEditorHelpers::ShouldBeVisible(*this, StructMember))
		{
			if (SinglePropertyName == NAME_None || StructMember->GetFName() == SinglePropertyName)
			{
				StructMembers.Add(StructMember);
				if (SinglePropertyName != NAME_None)
				{
					break;
				}
			}
		}
	}

	PropertyEditorHelpers::OrderPropertiesFromMetadata(StructMembers);

	for (FProperty* StructMember : StructMembers)
	{
		TSharedPtr<FItemPropertyNode> NewItemNode(new FItemPropertyNode);

		FPropertyNodeInitParams InitParams;
		InitParams.ParentNode = SharedThis(this);
		InitParams.Property = StructMember;
		InitParams.ArrayOffset = 0;
		InitParams.ArrayIndex = INDEX_NONE;
		InitParams.bAllowChildren = SinglePropertyName == NAME_None;
		InitParams.bForceHiddenPropertyVisibility = bShouldShowHiddenProperties;
		InitParams.bCreateDisableEditOnInstanceNodes = bShouldShowDisableEditOnInstance;
		InitParams.bCreateCategoryNodes = false;

		NewItemNode->InitNode(InitParams);
		AddChildNode(NewItemNode);
	}

	// Cache the init time base struct so that we can determine of the struct has changed.
	WeakCachedBaseStruct = Struct;
}

uint8* FStructurePropertyNode::GetValueBaseAddress(uint8* StartAddress, bool bIsSparseData, bool bIsStruct) const
{
	// If called with struct data, we expect that it is compatible with the first structure node down in the property node chain, return the address as is.
	// This gets called usually when the calling code is dealing with a parent complex node.
	if (bIsStruct)
	{
		return StartAddress;
	}

	if (StructProvider)
	{
		// Assume that this code gets called with an object or object sparse data.
		//
		// The passed object might not be the one that contains the values provided by struct provider.
		// For example this function might get called on an edited object's template object.
		// In that case the data structure is expected to match between the data edited by this node and the foreign object.

		// If the structure provider is set up as indirection, then it knows how to translate parent node's value address to
		// new value address even on data that is not the same as in the structure provider.
		if (StructProvider->IsPropertyIndirection())
		{
			const TSharedPtr<FPropertyNode> ParentNode = ParentNodeWeakPtr.Pin();
			if (!ensureMsgf(ParentNode, TEXT("Expecting valid parent node when indirection structure provider is called with Object data.")))
			{
				return nullptr;
			}
			// Resolve from parent nodes data.
			uint8* ParentValueAddress = ParentNode->GetValueAddress(StartAddress, bIsSparseData);
			uint8* ValueAddress = StructProvider->GetValueBaseAddress(ParentValueAddress, WeakCachedBaseStruct.Get());
			return ValueAddress;
		}

		// The struct is really standalone, in which case we always return the standalone struct data.
		// In that case we can only support one instance, since we cannot discern them.
		// Note: Multiple standalone structure instances are supported when bIsStruct is true (e.g. when the structure property is root node).
		TArray<TSharedPtr<FStructOnScope>> Instances;
		StructProvider->GetInstances(Instances, WeakCachedBaseStruct.Get());
		ensureMsgf(Instances.Num() <= 1, TEXT("Expecting max one instance on standalone structure provider."));
		if (Instances.Num() == 1 && Instances[0].IsValid())
		{
			return Instances[0]->GetStructMemory();
		}
	}
	
	return nullptr;
}

TSharedPtr<FPropertyNode> FStructurePropertyNode::GenerateSingleChild(FName ChildPropertyName)
{
	constexpr bool bDestroySelf = false;
	DestroyTree(bDestroySelf);

	// No category nodes should be created in single property mode
	SetNodeFlags(EPropertyNodeFlags::ShowCategories, false);

	InternalInitChildNodes(ChildPropertyName);

	if (ChildNodes.Num() > 0)
	{
		// only one node should be been created
		check(ChildNodes.Num() == 1);

		return ChildNodes[0];
	}

	return nullptr;
}

EPropertyDataValidationResult FStructurePropertyNode::EnsureDataIsValid()
{
	CachedReadAddresses.Reset();

	// If the struct has changed, rebuild children.
	const UStruct* CachedBaseStruct = WeakCachedBaseStruct.Get();
	if (GetBaseStructure() != CachedBaseStruct)
	{
		RebuildChildren();
		return EPropertyDataValidationResult::ChildrenRebuilt;
	}
	
	return FPropertyNode::EnsureDataIsValid();
}
