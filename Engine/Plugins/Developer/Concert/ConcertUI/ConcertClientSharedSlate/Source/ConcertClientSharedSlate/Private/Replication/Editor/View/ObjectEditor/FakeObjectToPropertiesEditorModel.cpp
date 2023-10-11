// Copyright Epic Games, Inc. All Rights Reserved.

#include "FakeObjectToPropertiesEditorModel.h"

#include "Replication/Editor/Model/Property/IPropertySelectionSourceModel.h"
#include "Replication/Editor/Model/Property/IPropertySourceModel.h"
#include "Replication/PropertyChainUtils.h"

#include "GameFramework/Actor.h"

namespace UE::ConcertClientSharedSlate
{
	namespace Private
	{
		static TOptional<FSoftObjectPath> GetTopLevelObjectOf(const FSoftObjectPath& SoftObjectPath)
		{
			// Example of an actor called floor
			// SoftObjectPath = { AssetPath = {PackageName = "/Game/Maps/SyncBoxLevel", AssetName = "SyncBoxLevel"}, SubPathString = "PersistentLevel.Floor" } }
			const FString& SubPathString = SoftObjectPath.GetSubPathString();

			constexpr int32 PersistentLevelStringLength = 16; // "PersistentLevel." has 16 characters
			const bool bIsWorldObject = SubPathString.Contains(TEXT("PersistentLevel."), ESearchCase::CaseSensitive);
			if (!bIsWorldObject)
			{
				// Not a path to a world object
				return {};
			}

			// Start search after the . behind PersistentLevel
			const int32 StartSearch = PersistentLevelStringLength + 1;
			const int32 IndexOfDotAfterActorName = SubPathString.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart, StartSearch);
			if (IndexOfDotAfterActorName == INDEX_NONE)
			{
				// SoftObjectPath points to an actor
				return {};
			}

			const int32 NumToChopOffRight = SubPathString.Len() - IndexOfDotAfterActorName;
			const FString NewSubstring = SubPathString.LeftChop(NumToChopOffRight);
			const FSoftObjectPath PathToOwningActor(SoftObjectPath.GetAssetPath(), NewSubstring);
			return PathToOwningActor;
		}
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
		// This makes SObjectToPropertyViewer only show actors in the outliner
		return IterateTopLevelObjects(Delegate);
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
			const TOptional<FSoftObjectPath> TopLevelObject = Private::GetTopLevelObjectOf(NonTopLevelObject);
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

	bool FFakeObjectToPropertiesEditorModel::IsTopLevelObject(const FSoftObjectPath& ObjectPath) const
	{
		const FSoftClassPath ClassPath = GetObjectClass(ObjectPath);
		const UClass* LoadedClass = ClassPath.TryLoadClass<UObject>();
		return LoadedClass && LoadedClass->IsChildOf(AActor::StaticClass());
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
		
		// This makes SObjectToPropertyViewer list all available properties
		PropertySelectionSource->GetPropertySource(LoadedClass)
		   ->EnumerateSelectableItems([&Delegate](const FSelectablePropertyInfo& PropertyInfo)
		   {
			   return Delegate(PropertyInfo.Property);
		   });
		return true;
	}
}
