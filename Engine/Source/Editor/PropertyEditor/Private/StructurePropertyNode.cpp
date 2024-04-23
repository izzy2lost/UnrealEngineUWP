// Copyright Epic Games, Inc. All Rights Reserved.


#include "StructurePropertyNode.h"
#include "ItemPropertyNode.h"
#include "ObjectPropertyNode.h"
#include "PropertyEditorHelpers.h"

void FStructurePropertyNode::InitChildNodes()
{
	InternalInitChildNodes(FName());
}

void FStructurePropertyNode::InternalInitChildNodes(FName SinglePropertyName)
{
	const bool bShouldShowHiddenProperties = !!HasNodeFlags(EPropertyNodeFlags::ShouldShowHiddenProperties);
	const bool bShouldShowDisableEditOnInstance = !!HasNodeFlags(EPropertyNodeFlags::ShouldShowDisableEditOnInstance);

	const UStruct* Struct = nullptr;
	if (StructProvider)
	{
		if (!StructProvider->IsPropertyIndirection())
		{
			Struct = GetBaseStructure();
		}
		else
		{
			FPropertyNode* Parent = GetParentNode();

			FUncachedPropertyNodeAddresses ParentAddresses;
			ParentAddresses.Reset();
			const bool bIsValid = Parent->InternalGetReadAddressUncached(*Parent, ParentAddresses);
			if (bIsValid)
			{
				auto FindCommonBaseStruct = [](const UStruct* StructA, const UStruct* StructB)
				{
					const UStruct* CommonBaseStruct = StructA;
					while (CommonBaseStruct && StructB && !StructB->IsChildOf(CommonBaseStruct))
					{
						CommonBaseStruct = Cast<UStruct>(CommonBaseStruct->GetSuperStruct());
					}
					return CommonBaseStruct;
				};

				const UStruct* CommonStruct = nullptr;
				for (int32 Index = 0, End = ParentAddresses.Num(); Index < End; ++Index)
				{
					uint8* ParentValueAddress = ParentAddresses[Index].ReadAddress;
				
					const UStruct* TempStruct = StructProvider->GetIndirectedStructType(ParentValueAddress);
					CommonStruct = FindCommonBaseStruct(TempStruct, CommonStruct);
				}
				Struct = CommonStruct;
			}
		}
	}

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

	// Cache the init time base struct so that we can determine if the struct has changed.
	// Store the cached base struct before calling AddChildNode() as they may call back to this node.
	WeakCachedBaseStruct = Struct;

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
}

void FStructurePropertyNode::InitBeforeNodeFlags()
{
	// Cache the base struct. It is used to check if the struct changes later.
	// The struct will be cached on each call to InternalInitChildNodes() as well.
	// We'll cache it here too, so that a FStructurePropertyNode which is initialized
	// with "InitParams.bAllowChildren = false" has it properly set up.
	const UStruct* Struct = nullptr;
	if (StructProvider)
	{
		if (!StructProvider->IsPropertyIndirection())
		{
			Struct = GetBaseStructure();
		}
		else
		{
			FPropertyNode* Parent = GetParentNode();

			FUncachedPropertyNodeAddresses ParentAddresses;
			ParentAddresses.Reset();
			const bool bIsValid = Parent->InternalGetReadAddressUncached(*Parent, ParentAddresses);
			if (bIsValid)
			{
				auto FindCommonBaseStruct = [](const UStruct* StructA, const UStruct* StructB)
				{
					const UStruct* CommonBaseStruct = StructA;
					while (CommonBaseStruct && StructB && !StructB->IsChildOf(CommonBaseStruct))
					{
						CommonBaseStruct = Cast<UStruct>(CommonBaseStruct->GetSuperStruct());
					}
					return CommonBaseStruct;
				};

				const UStruct* CommonStruct = nullptr;
				for (int32 Index = 0, End = ParentAddresses.Num(); Index < End; ++Index)
				{
					uint8* ParentValueAddress = ParentAddresses[Index].ReadAddress;
				
					const UStruct* TempStruct = StructProvider->GetIndirectedStructType(ParentValueAddress);
					CommonStruct = FindCommonBaseStruct(TempStruct, CommonStruct);
				}
				Struct = CommonStruct;
			}
		}
		WeakCachedBaseStruct = Struct;
	}
	

}

bool FStructurePropertyNode::HasValidStructData() const
{
	if (!StructProvider.IsValid())
	{
		return false;
	}
	if (!StructProvider->IsPropertyIndirection())
	{
		return StructProvider->IsValid();
	}
	else
	{
		const UStruct* ExpectedBaseStruct = WeakCachedBaseStruct.Get();
		if (!ExpectedBaseStruct)
		{
			return false;
		}
		
		const FPropertyNode* Parent = this->GetParentNode();
		FUncachedPropertyNodeAddresses ParentAddresses;
		const bool bIsValid = InternalGetReadAddressUncached(*Parent, ParentAddresses);

		for (int32 AddressIndex = 0, AddressListEnd = ParentAddresses.Num(); AddressIndex < AddressListEnd; ++AddressIndex)
		{
			uint8* ParentAddress = ParentAddresses[AddressIndex].ReadAddress;
			StructProvider->GetValueBaseAddress(ParentAddress, ExpectedBaseStruct);
			StructProvider->GetIndirectedStructType(ParentAddress, ExpectedBaseStruct);
			if (!StructProvider->IsIndirectedValid(ParentAddress))
			{
				return false;
			}
		}
		return true;
	}
}

void FStructurePropertyNode::GetAllStructureData(TArray<TSharedPtr<FStructOnScope>>& OutStructs) const
{
	if (StructProvider)
	{
		if (!StructProvider->IsPropertyIndirection())
		{
			StructProvider->GetInstances(OutStructs, WeakCachedBaseStruct.Get());
		}
		else
		{
			const FPropertyNode* ParentNode = this->GetParentNode();
			if (ParentNode)
			{
				// NOTE: Recursively walks up the chain until there is a node that allows directly getting the memory address
				// of the FInstancedStruct!!!
				// TODO: It is unfortunate that this create temporary arrays on recurse
				FUncachedPropertyNodeAddresses ParentAddresses;
				const bool bIsValid = InternalGetReadAddressUncached(*ParentNode, ParentAddresses);

				const UStruct* ExpectedBaseStruct = WeakCachedBaseStruct.Get();
				if (bIsValid && ExpectedBaseStruct)
				{
					TArray<UPackage*> Packages;
					GetOwnerPackages(Packages);
					OutStructs.Reserve(ParentAddresses.Num());
					for (int32 Index = 0, End = ParentAddresses.Num(); Index < End; ++Index)
					{
						uint8* ParentAddress = ParentAddresses[Index].ReadAddress;
						uint8* ValueAddress = StructProvider->GetValueBaseAddress(ParentAddress, ExpectedBaseStruct);
						const UStruct* Struct = StructProvider->GetIndirectedStructType(ParentAddress, ExpectedBaseStruct);
						if (ValueAddress && Struct)
						{
							OutStructs.Emplace(MakeShared<FStructOnScope>(Struct, ValueAddress));
							OutStructs.Last()->SetPackage(Packages[Index]);
						}
					}
				}
			}
		}
	}
}

void FStructurePropertyNode::GetOwnerPackages(TArray<UPackage*>& OutPackages) const
{
	if (StructProvider)
	{
		// Walk up until we find the objects that contain this struct property to get the packages
		const FPropertyNode* Parent = this;
		
		while(Parent)
		{
			const FComplexPropertyNode* ComplexParent = Parent->FindComplexParent();
			if (!ensureMsgf(ComplexParent != nullptr, TEXT("Expected to find a complex parent")))
			{
				return;
			}
			EPropertyType PropertyType = ComplexParent->GetPropertyType();
				
			switch (PropertyType)
			{
			case EPT_Object:
				{
					const FObjectPropertyNode* ObjectNode = ComplexParent->AsObjectNode();
					for (int32 ObjectIndex = 0, ObjectCount = ObjectNode->GetNumObjects(); ObjectIndex < ObjectCount; ++ObjectIndex)
					{
						const UPackage* Package = ObjectNode->GetUPackage(ObjectIndex);
						OutPackages.Add(const_cast<UPackage*>(Package));
					}
					Parent = nullptr;
					break;
				}
			case EPT_StandaloneStructure:
				{
					// Iterate up until we find objects
					Parent = ComplexParent->GetParentNode();
					break;
				}
			}
		}
	}
}

bool FStructurePropertyNode::GetReadAddressUncached(const FPropertyNode& InPropertyNode, FReadAddressListData& OutAddresses) const
{
	bool bHasData = false;
	if (StructProvider)
	{
		if (!StructProvider->IsPropertyIndirection())
		{
			if (!HasValidStructData())
			{
				return false;
			}
			check(StructProvider.IsValid());

			const FProperty* InItemProperty = InPropertyNode.GetProperty();
			if (!InItemProperty)
			{
				return false;
			}

			UStruct* OwnerStruct = InItemProperty->GetOwnerStruct();
			if (!OwnerStruct || OwnerStruct->IsStructTrashed())
			{
				// Verify that the property is not part of an invalid trash class
				return false;
			}

			TArray<TSharedPtr<FStructOnScope>> Instances;
			StructProvider->GetInstances(Instances, WeakCachedBaseStruct.Get());

			for (TSharedPtr<FStructOnScope>& Instance : Instances)
			{
				uint8* ReadAddress = Instance.IsValid() ? Instance->GetStructMemory() : nullptr;
				if (ReadAddress)
				{
					OutAddresses.Add(nullptr, InPropertyNode.GetValueBaseAddress(ReadAddress, InPropertyNode.HasNodeFlags(EPropertyNodeFlags::IsSparseClassData) != 0, /*bIsStruct=*/true), /*bIsStruct=*/true);
					bHasData = true;
				}
			}
		}
		else
		{
			// Need the parent of this node to be passed to the struct provider.  The struct provider will know what type it is and what to do with it
			const FComplexPropertyNode* CurrentNode = this;
			const FPropertyNode* ParentNode = CurrentNode->GetParentNode();
			if (ParentNode)
			{
				// NOTE: Recursively walks up the chain until there is a node that allows directly getting the memory address
				// of the FInstancedStruct!!!
				// TODO: It is unfortunate that this create temporary arrays on recurse
				FUncachedPropertyNodeAddresses ParentAddresses;
				const bool bIsValid = InternalGetReadAddressUncached(*ParentNode, ParentAddresses);
			
				// Pass those addresses down to the StructProvider... it will know what to do with them
				const UStruct* ExpectedBaseStruct = WeakCachedBaseStruct.Get();
				if (bIsValid && ExpectedBaseStruct)
				{
					for (int32 Index = 0, End = ParentAddresses.Num(); Index < End; ++Index)
					{
						const UObject* Object = nullptr;
						uint8* ParentValueAddress = ParentAddresses[Index].ReadAddress;
						const bool bIsStruct = true;
					
						OutAddresses.Add(Object, StructProvider->GetValueBaseAddress(ParentValueAddress, ExpectedBaseStruct), bIsStruct);
					}
					bHasData = true;
				}
			}
		}
	}
	
	return bHasData;
}

bool FStructurePropertyNode::InternalGetReadAddressUncached(const FPropertyNode& InPropertyNode, FUncachedPropertyNodeAddresses& OutAddresses) const
{
	bool bHasData = false;
	const FProperty* InItemProperty = InPropertyNode.GetProperty();
	if (!InItemProperty)
	{
		return false;
	}

	UStruct* OwnerStruct = InItemProperty->GetOwnerStruct();
	if (!OwnerStruct || OwnerStruct->IsStructTrashed())
	{
		// Verify that the property is not part of an invalid trash class
		return false;
	}
	
	if (!StructProvider)
	{
		return false;
	}
	
	if (!StructProvider->IsPropertyIndirection())
	{
		TArray<TSharedPtr<FStructOnScope>> Instances;
		StructProvider->GetInstances(Instances, WeakCachedBaseStruct.Get());

		for (TSharedPtr<FStructOnScope>& Instance : Instances)
		{
			uint8* ReadAddress = Instance.IsValid() ? Instance->GetStructMemory() : nullptr;
			if (ReadAddress)
			{
				const UObject* Object = nullptr;
				const bool bIsSparse = InPropertyNode.HasNodeFlags(EPropertyNodeFlags::IsSparseClassData) != 0;
				constexpr bool bIsStruct = true;
				uint8* Address = InPropertyNode.GetValueBaseAddress(ReadAddress, bIsSparse, bIsStruct);
				OutAddresses.Emplace(FAddressPair(Object, Address, bIsStruct));
				bHasData = true;
			}
		}
	}
	else
	{
		// Need the parent of this node to be passed to the struct provider.  The struct provider will know what type it is and what to do with it
		const FComplexPropertyNode* CurrentNode = this;
		const FPropertyNode* ParentNode = CurrentNode->GetParentNode();
		if (ParentNode)
		{
			// NOTE: Recursively walks up the chain until there is a node that allows directly getting the memory address
			// of the FInstancedStruct!!!
			// TODO: It is unfortunate that this create temporary arrays on recurse
			FUncachedPropertyNodeAddresses ParentAddresses;
			const bool bIsValid = ParentNode->InternalGetReadAddressUncached(InPropertyNode, ParentAddresses);
			
			// Pass those addresses down to the StructProvider... it will know what to do with them
			const UStruct* ExpectedBaseStruct = WeakCachedBaseStruct.Get();
			if (bIsValid && ExpectedBaseStruct)
			{
				for (int32 Index = 0, End = ParentAddresses.Num(); Index < End; ++Index)
				{
					const UObject* Object = nullptr;
					uint8* ParentValueAddress = ParentAddresses[Index].ReadAddress;
					const bool bIsStruct = true;

					OutAddresses.Emplace(FAddressPair(Object, ParentValueAddress, bIsStruct));
				}
				bHasData = true;
			}
		}
	}
	
	return bHasData;
}

bool FStructurePropertyNode::GetReadAddressUncached(const FPropertyNode& InPropertyNode,
	bool InRequiresSingleSelection,
	FReadAddressListData* OutAddresses,
	bool bComparePropertyContents,
	bool bObjectForceCompare,
	bool bArrayPropertiesCanDifferInSize) const
{
	if (!HasValidStructData())
	{
		return false;
	}
	check(StructProvider.IsValid());

	const FProperty* InItemProperty = InPropertyNode.GetProperty();
	if (!InItemProperty)
	{
		return false;
	}

	const UStruct* OwnerStruct = InItemProperty->GetOwnerStruct();
	if (!OwnerStruct || OwnerStruct->IsStructTrashed())
	{
		// Verify that the property is not part of an invalid trash class
		return false;
	}

	bool bAllTheSame = true;

	struct InstanceData
	{
		uint8* Address;
		const UStruct* Struct;
	};
	TArray<InstanceData> TempInstances;

	{
		if (!StructProvider->IsPropertyIndirection())
		{
			TArray<TSharedPtr<FStructOnScope>> Instances;
			StructProvider->GetInstances(Instances, WeakCachedBaseStruct.Get());
	
			if (Instances.IsEmpty())
			{
				return false;
			}

			// TODO Convert Instances to TempInstances
			for (auto& Instance : Instances)
			{
				if (Instance)
				{
					TempInstances.Emplace(
						InstanceData{
							.Address = Instance->GetStructMemory(),
							.Struct = Instance->GetStruct()
					});
				}
			}
		}
		else
		{
			const FPropertyNode* ParentNode = this->GetParentNode();
			FUncachedPropertyNodeAddresses ParentAddresses;
			const bool bIsValid = InternalGetReadAddressUncached(*ParentNode, ParentAddresses);

			const UStruct* ExpectedBaseStruct = WeakCachedBaseStruct.Get();
			if (bIsValid && ExpectedBaseStruct)
			{
				for (int32 Index = 0, End = ParentAddresses.Num(); Index < End; ++Index)
				{
					uint8* ParentValueAddress = ParentAddresses[Index].ReadAddress;
					
					const UStruct* StructType = StructProvider->GetIndirectedStructType(ParentValueAddress, ExpectedBaseStruct);
					uint8* Address = StructProvider->GetValueBaseAddress(ParentValueAddress, ExpectedBaseStruct);
					TempInstances.Emplace(
						InstanceData{
							.Address = Address,
							.Struct = StructType
					});
				}
			}
			else
			{
				return false;
			}
		}
	}
	
	if (bComparePropertyContents || bObjectForceCompare)
	{
		const bool bIsSparse = InPropertyNode.HasNodeFlags(EPropertyNodeFlags::IsSparseClassData) != 0;
		const uint8* BaseAddress = nullptr;
		const UStruct* BaseStruct = nullptr;

		for (auto& Instance : TempInstances)
		{
			if (const UStruct* Struct = Instance.Struct)
			{
				if (const uint8* ReadAddress = InPropertyNode.GetValueBaseAddress(Instance.Address, bIsSparse, /*bIsStruct=*/true))
				{
					if (!BaseAddress)
					{
						BaseAddress = ReadAddress;
						BaseStruct = Struct;
					}
					else
					{
						if (BaseStruct != Struct)
						{
							bAllTheSame = false;
							break;
						}
						if (!InItemProperty->Identical(BaseAddress, ReadAddress))
						{
							bAllTheSame = false;
							break;
						}
					}
				}
			}
		}

		// If none of the instances have data, treat it as if the instance data was empty.
		if (!BaseStruct)
		{
			bAllTheSame = false;
		}
	}
	else
	{
		// Check that all are valid or invalid.
		const UStruct* BaseStruct = TempInstances[0].Struct;
		for (int32 Index = 1; Index < TempInstances.Num(); Index++)
		{
			const UStruct* Struct = TempInstances[Index].Struct;
			if (BaseStruct != Struct)
			{
				bAllTheSame = false;
				break;
			}
		}
		
		// If none of the instances have data, treat it as if the instance data was empty.
		if (!BaseStruct)
		{
			bAllTheSame = false;
		}
	}

	if (bAllTheSame && OutAddresses)
	{
		for (const auto& Instance : TempInstances)
		{
			uint8* ReadAddress = Instance.Address;
			if (ReadAddress)
			{
				OutAddresses->Add(nullptr, InPropertyNode.GetValueBaseAddress(ReadAddress, InPropertyNode.HasNodeFlags(EPropertyNodeFlags::IsSparseClassData) != 0, /*bIsStruct=*/true), /*bIsStruct=*/true);
			}
		}
	}

	return bAllTheSame;
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
		if (!StructProvider->IsPropertyIndirection())
		{
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
		// Assume that this code gets called with an object or object sparse data.
		//
		// The passed object might not be the one that contains the values provided by struct provider.
		// For example this function might get called on an edited object's template object.
		// In that case the data structure is expected to match between the data edited by this node and the foreign object.

		// If the structure provider is set up as indirection, then it knows how to translate parent node's value address to
		// new value address even on data that is not the same as in the structure provider.
		else
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
	}
	
	return nullptr;
}

int32 FStructurePropertyNode::GetInstancesNum() const
{
	// Can't get instance count directly from standalone structures, need to walk to the next parent that is an object
	// to get number of instances from that
	const FComplexPropertyNode* CurrentNode = this;	
	
	while(CurrentNode)
	{
		if (const FObjectPropertyNode* ObjectNode = CurrentNode->AsObjectNode())
		{
			// Found owning UObject
			const int32 ObjectCount = ObjectNode->GetInstancesNum();
			return ObjectCount;
		}
		else if (const FStructurePropertyNode* StructNode = CurrentNode->AsStructureNode())
		{
			TSharedPtr<IStructureDataProvider> TempStructProvider = StructNode->GetStructProvider();
			if (TempStructProvider->IsPropertyIndirection())
			{
				// Keep walking up
				const FPropertyNode* ParentNode = CurrentNode->GetParentNode();
				CurrentNode = ParentNode->FindComplexParent();
			}
			else
			{
				TArray<TSharedPtr<FStructOnScope>> Instances;
				TempStructProvider->GetInstances(Instances, StructNode->WeakCachedBaseStruct.Get());
				return Instances.Num();
			}
		}
		else
		{
			return 0;
		}
	}
	return 0;
}

uint8* FStructurePropertyNode::GetMemoryOfInstance(int32 Index) const
{
	if (StructProvider)
	{
		if (!StructProvider->IsPropertyIndirection())
		{
			TArray<TSharedPtr<FStructOnScope>> Instances;
			StructProvider->GetInstances(Instances, WeakCachedBaseStruct.Get());
			if (Instances.IsValidIndex(Index) && Instances[Index].IsValid())
			{
				return Instances[Index]->GetStructMemory();
			}
		}
		else
		{
			const UStruct* ExpectedBaseStruct = WeakCachedBaseStruct.Get();
			const FPropertyNode* Parent = GetParentNode();
			FUncachedPropertyNodeAddresses ParentAddresses;
			bool bSuccessful = InternalGetReadAddressUncached(*Parent, ParentAddresses);

			if (bSuccessful && ParentAddresses.IsValidIndex(Index))
			{
				uint8* ParentAddress = ParentAddresses[Index].ReadAddress;
				// Dereference to get the address of the struct
				uint8* ValueAddress = StructProvider->GetValueBaseAddress(ParentAddress, ExpectedBaseStruct);
				return ValueAddress;
			}
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
