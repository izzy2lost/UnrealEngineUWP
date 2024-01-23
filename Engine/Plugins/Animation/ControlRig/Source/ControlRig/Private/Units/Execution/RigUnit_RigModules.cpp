// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Execution/RigUnit_RigModules.h"
#include "Units/RigUnitContext.h"
#include "ControlRig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_RigModules)

FRigUnit_ResolveConnector_Execute()
{
	Result = Connector;
	
	if(const FRigBaseElement* ResolvedElement = ExecuteContext.Hierarchy->Find(Connector))
	{
		Result = ResolvedElement->GetKey();
	}
}

FRigUnit_GetCurrentNameSpace_Execute()
{
	if(!ExecuteContext.IsRigModule())
	{
#if WITH_EDITOR
		if(ExecuteContext.GetLog() != nullptr)
		{
			static const FString Message = TEXT("This node should only be used in a Rig Module."); 
			ExecuteContext.GetLog()->Report(EMessageSeverity::Warning, ExecuteContext.GetFunctionName(), ExecuteContext.GetInstructionIndex(), Message);
		}
#endif
	}
	NameSpace = *ExecuteContext.GetRigModuleNameSpace();
}

FRigUnit_GetItemShortName_Execute()
{
	ShortName = NAME_None;

	if(const URigHierarchy* Hierarchy = ExecuteContext.Hierarchy)
	{
		ShortName = *Hierarchy->GetDisplayNameForUI(Item).ToString();
	}

	if(ShortName.IsNone())
	{
		ShortName = Item.Name;
	}
}

FRigUnit_GetItemNameSpace_Execute()
{
	NameSpace.Reset();
	HasNameSpace = false;

	if(const URigHierarchy* Hierarchy = ExecuteContext.Hierarchy)
	{
		const FString NameSpaceForItem = Hierarchy->GetNameSpace(Item);
		if(!NameSpaceForItem.IsEmpty())
		{
			NameSpace = NameSpaceForItem;
			HasNameSpace = true;
		}
	}
}

FRigUnit_IsItemInCurrentNameSpace_Execute()
{
	FString CurrentNameSpace;
	FRigUnit_GetCurrentNameSpace::StaticExecute(ExecuteContext, CurrentNameSpace);
	bool bHasNameSpace = false;
	FString ItemNameSpace;
	FRigUnit_GetItemNameSpace::StaticExecute(ExecuteContext, Item, bHasNameSpace, ItemNameSpace);

	Result = false;
	if(!CurrentNameSpace.IsEmpty() && !ItemNameSpace.IsEmpty())
	{
		Result = ItemNameSpace.Equals(CurrentNameSpace, ESearchCase::CaseSensitive); 
	}
}

FRigUnit_GetItemsInNameSpace_Execute()
{
	const URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;

	FString NameSpace;
	FRigUnit_GetCurrentNameSpace::StaticExecute(ExecuteContext, NameSpace);
	if(NameSpace.IsEmpty())
	{
		Items.Reset();
		return;
	}
	const FName NameSpaceName = *NameSpace;
	
	uint32 Hash = GetTypeHash(StaticStruct());
	Hash = HashCombine(Hash, GetTypeHash((int32)TypeToSearch));
	Hash = HashCombine(Hash, GetTypeHash(NameSpaceName));

	if(const FRigElementKeyCollection* Cache = Hierarchy->FindCachedCollection(Hash))
	{
		Items = Cache->Keys;
	}
	else
	{
		FRigElementKeyCollection Collection;
		Hierarchy->Traverse([Hierarchy, NameSpaceName, &Collection, TypeToSearch]
			(const FRigBaseElement* InElement, bool &bContinue)
			{
				bContinue = true;
						
				const FRigElementKey Key = InElement->GetKey();
				if(((uint8)TypeToSearch & (uint8)Key.Type) == (uint8)Key.Type)
				{
					const FName ItemNameSpace = Hierarchy->GetNameSpaceFName(Key);
					if(!ItemNameSpace.IsNone())
					{
						if(ItemNameSpace.IsEqual(NameSpaceName, ENameCase::CaseSensitive))
						{
							Collection.AddUnique(Key);
						}
					}
				}
			}
		);

		Hierarchy->AddCachedCollection(Hash, Collection);
		Items = Collection.Keys;
	}
}