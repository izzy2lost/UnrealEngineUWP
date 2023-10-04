// Copyright Epic Games, Inc. All Rights Reserved.

#include "FakeObjectToPropertiesEditorModel.h"

#include "Replication/Editor/Model/Property/IPropertySelectionSourceModel.h"
#include "Replication/Editor/Model/Property/IPropertySourceModel.h"
#include "Replication/PropertyChainUtils.h"

#include "GameFramework/Actor.h"

namespace UE::ConcertClientSharedSlate
{
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
		return RealModel->ForEachReplicatedObject([this, &Delegate](const FSoftObjectPath& Object)
		{
			return IsRootObject(Object)
				? Delegate(Object)
				: EBreakBehavior::Continue;
		});
	}

	bool FFakeObjectToPropertiesEditorModel::ForEachProperty(const FSoftObjectPath& ObjectPath, TFunctionRef<EBreakBehavior(const FConcertPropertyChain& Property)> Delegate) const
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

	bool FFakeObjectToPropertiesEditorModel::IsRootObject(const FSoftObjectPath& ObjectPath) const
	{
		const FSoftClassPath ClassPath = GetObjectClass(ObjectPath);
		UClass* LoadedClass = ClassPath.TryLoadClass<UObject>();
		return LoadedClass && LoadedClass->IsChildOf(AActor::StaticClass());
	}
}
