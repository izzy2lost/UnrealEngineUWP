// Copyright Epic Games, Inc. All Rights Reserved.

#include "FakeObjectToPropertiesEditorModel.h"

#include "Replication/Editor/Model/Property/IPropertySelectionSourceModel.h"
#include "Replication/Editor/Model/Property/IPropertySourceModel.h"
#include "Replication/Editor/View/ObjectUtils.h"
#include "Replication/PropertyChainUtils.h"

#include "GameFramework/Actor.h"

namespace UE::ConcertClientSharedSlate
{
	bool FFakeObjectToPropertiesEditorModel::IsTopLevelObject(const FSoftObjectPath& ObjectPath) const
	{
		const FSoftClassPath ClassPath = GetObjectClass(ObjectPath);
		const UClass* LoadedClass = ClassPath.TryLoadClass<UObject>();
		return LoadedClass && LoadedClass->IsChildOf(AActor::StaticClass());
	}
	
	FSoftClassPath FFakeObjectToPropertiesEditorModel::GetObjectClass(const FSoftObjectPath& Object) const
	{
		const FSoftClassPath ResolvedClass = RealModel->GetObjectClass(Object);
		if (ResolvedClass.IsValid())
		{
			return ResolvedClass;
		}

		// The object is not yet in the model.
		const UObject* LoadedObject = Object.ResolveObject();
		return LoadedObject ? LoadedObject->GetClass() : nullptr;
	}

	bool FFakeObjectToPropertiesEditorModel::ForEachReplicatedObject(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Delegate) const
	{
		return EnumHasAnyFlags(Flags, EFakeObjectModelFlags::OnlyTopLevelObjects)
			// This makes SReplicationStreamViewer only show actors in the outliner
			? IterateTopLevelObjects(Delegate) 
			: RealModel->ForEachReplicatedObject(Delegate);
	}

	bool FFakeObjectToPropertiesEditorModel::ForEachProperty(const FSoftObjectPath& ObjectPath, TFunctionRef<EBreakBehavior(const FConcertPropertyChain& Property)> Delegate) const
	{
		return IterateDisplayedProperties(ObjectPath, Delegate);
	}

	bool FFakeObjectToPropertiesEditorModel::IterateTopLevelObjects(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Delegate) const
	{
		// Case: RealModel contains only components but not the owning actor.
		// In that case, we want the UI to still show the owning actor.
		// We'll track this with these containers:
		TSet<FSoftObjectPath> NonTopLevelObjects;
		TSet<FSoftObjectPath> FoundTopLevelObjects;

		// Here we only exposes objects that are top level so we skip components, etc.
		EBreakBehavior BreakBehavior = EBreakBehavior::Continue;
		const bool bResult = RealModel->ForEachReplicatedObject([this, &Delegate, &NonTopLevelObjects, &FoundTopLevelObjects, &BreakBehavior](const FSoftObjectPath& Object)
		{
			if (IsTopLevelObject(Object))
			{
				FoundTopLevelObjects.Add(Object);
				BreakBehavior = Delegate(Object); 
				return BreakBehavior;
			}
			
			NonTopLevelObjects.Add(Object);
			return EBreakBehavior::Continue;
		});

		// No additional objects to show
		if (BreakBehavior == EBreakBehavior::Break)
		{
			return bResult;
		}

		// Now determine the top level objects that are not in RealModel but that need to be shown for subobjects that are in RealModel
		for (const FSoftObjectPath& NonTopLevelObject : NonTopLevelObjects)
		{
			const TOptional<FSoftObjectPath> TopLevelObject = ObjectUtils::GetActorOf(NonTopLevelObject);
			if (!TopLevelObject)
			{
				continue;
			}
			
			if (!FoundTopLevelObjects.Contains(*TopLevelObject))
			{
				if (Delegate(*TopLevelObject) == EBreakBehavior::Break)
				{
					return true;
				}

				// Avoid calling Delegate again for TopLevelObject
				FoundTopLevelObjects.Add(*TopLevelObject);
			}
		}

		return bResult;
	}

	bool FFakeObjectToPropertiesEditorModel::IterateDisplayedProperties(const FSoftObjectPath& ObjectPath, TFunctionRef<EBreakBehavior(const FConcertPropertyChain& Property)> Delegate) const
	{
		// GetObjectClass might return null because ObjectPath might not be contained in the model
		const FSoftClassPath ClassPath = GetObjectClass(ObjectPath);
		UClass* LoadedClass = ClassPath.TryLoadClass<UObject>();
		if (!LoadedClass)
		{
			UObject* Object = ObjectPath.ResolveObject();
			LoadedClass = Object ? Object->GetClass() : nullptr;
		}
		if (!LoadedClass)
		{
			return false;
		}
		
		// This makes SObjectToPropertyView list all available properties
		PropertySelectionSource->GetPropertySource(LoadedClass)
		   ->EnumerateSelectableItems([&Delegate](const FSelectablePropertyInfo& PropertyInfo)
		   {
			   return Delegate(PropertyInfo.Property);
		   });
		return true;
	}
}
