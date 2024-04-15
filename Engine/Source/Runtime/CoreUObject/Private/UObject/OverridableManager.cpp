// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/OverridableManager.h"
#include "InstancedReferenceSubobjectHelper.h"
#include "UObject/UObjectGlobals.h"

/*
 *************************************************************************************
 * Overridable serialization is experimental, not supported and use at your own risk *
 *************************************************************************************
 */
 
FOverridableManager& FOverridableManager::Get()
{
	static FOverridableManager OverridableManager;
	return OverridableManager;
}

bool FOverridableManager::IsEnabled(const UObject& Object)
{
#ifdef WITH_EDITORONLY_DATA
	return OverriddenObjectAnnotations.IsEnabled(Object);
#else
	return false;
#endif // WITH_EDITORONLY_DATA
}

void FOverridableManager::Enable(UObject& Object)
{
#ifdef WITH_EDITORONLY_DATA
	OverriddenObjectAnnotations.FindOrAdd(Object);
#endif // WITH_EDITORONLY_DATA
}

void FOverridableManager::Disable(UObject& Object)
{
#ifdef WITH_EDITORONLY_DATA
	OverriddenObjectAnnotations.RemoveAnnotation(&Object);
#endif // WITH_EDITORONLY_DATA
}

void FOverridableManager::InheritEnabledFrom(UObject& Object, const UObject* DefaultData)
{
#ifdef WITH_EDITORONLY_DATA
	if (!OverriddenObjectAnnotations.IsEnabled(Object))
	{
		const UObject* Outer = Object.GetOuter();
		if ((Outer && IsEnabled(*Outer)) || (DefaultData && IsEnabled(*DefaultData)))
		{
			Enable(Object);
		}
	}
#endif // WITH_EDITORONLY_DATA
}

bool FOverridableManager::NeedSubObjectTemplateInstantiation(const UObject& Object)
{
#ifdef WITH_EDITORONLY_DATA
	if( const FOverriddenPropertySet* OverriddenProperties = OverriddenObjectAnnotations.Find(Object))
	{
		return OverriddenProperties->bNeedsSubobjectTemplateInstantiation;
	}
#endif // WITH_EDITORONLY_DATA
	return false;
}

FOverriddenPropertySet* FOverridableManager::GetOverriddenProperties(UObject& Object)
{
#ifdef WITH_EDITORONLY_DATA
	return OverriddenObjectAnnotations.Find(Object);
#else
	return nullptr;
#endif // WITH_EDITORONLY_DATA
}

const FOverriddenPropertySet* FOverridableManager::GetOverriddenProperties(const UObject& Object)
{
#ifdef WITH_EDITORONLY_DATA
	return OverriddenObjectAnnotations.Find(Object);
#else
	return nullptr;
#endif // WITH_EDITORONLY_DATA
}

FOverriddenPropertySet& FOverridableManager::SetOverriddenProperties(UObject& Object, EOverriddenPropertyOperation Operation)
{
#ifdef WITH_EDITORONLY_DATA
	FOverriddenPropertySet& ObjectOverriddenProperties = OverriddenObjectAnnotations.FindOrAdd(Object);
	ObjectOverriddenProperties.Reset();
	ObjectOverriddenProperties.SetOverriddenPropertyOperation(Operation, /*CurrentPropertyChain*/nullptr, /*Property*/nullptr);
	return ObjectOverriddenProperties;
#else
	static FOverriddenPropertySet Dummy;
	return Dummy;
#endif // WITH_EDITORONLY_DATA

}

EOverriddenState FOverridableManager::GetOverriddenState(UObject& Object)
{
#ifdef WITH_EDITORONLY_DATA
	if(const FOverriddenPropertySet* OverriddenProperties = GetOverriddenProperties(Object))
	{
		// Consider any object that its template is a CDO as added.
		if (UObject* Archetype = Object.GetArchetype())
		{
			if (Archetype->HasAnyFlags(RF_ClassDefaultObject))
			{
				return EOverriddenState::Added;
			}
		}

		const EOverriddenPropertyOperation Operation = OverriddenProperties->GetOverriddenPropertyOperation((FArchiveSerializedPropertyChain*)nullptr, (FProperty*)nullptr);
		if (Operation != EOverriddenPropertyOperation::None)
		{
			return Operation == EOverriddenPropertyOperation::Replace ? EOverriddenState::AllOverridden : EOverriddenState::HasOverrides;
		}

		// Need to check subobjects to 
		TSet<UObject*> InstancedSubObjects;
		FFindInstancedReferenceSubobjectHelper::GetInstancedSubObjects(&Object, InstancedSubObjects);
		for (UObject* InstancedSubObject : InstancedSubObjects)
		{
			if (InstancedSubObject && InstancedSubObject->IsIn(&Object))
			{
				if (GetOverriddenState(*InstancedSubObject) != EOverriddenState::NoOverrides)
				{
					return EOverriddenState::HasOverrides;
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
	return EOverriddenState::NoOverrides;
}

void FOverridableManager::OverrideObject(UObject& Object)
{
#ifdef WITH_EDITORONLY_DATA
	if (FOverriddenPropertySet* ThisObjectOverriddenProperties = OverriddenObjectAnnotations.Find(Object))
	{
		// Passing no property node means we are overriding the object itself
		ThisObjectOverriddenProperties->OverrideProperty(FPropertyChangedEvent(nullptr), /*PropertyNode*/nullptr, &Object);
	}
#endif // WITH_EDITORONLY_DATA
}

void FOverridableManager::OverrideInstancedSubObject(UObject& Object, UObject& InstancedSubObject)
{
#ifdef WITH_EDITORONLY_DATA
	if (InstancedSubObject.IsIn(&Object))
	{
		OverrideObject(InstancedSubObject);
	}
#endif // WITH_EDITORONLY_DATA
}

void FOverridableManager::PropagateOverrideToInstancedSubObjects(UObject& Object)
{
#ifdef WITH_EDITORONLY_DATA
	TSet<UObject*> InstancedSubObjects;
	FFindInstancedReferenceSubobjectHelper::GetInstancedSubObjects(&Object, InstancedSubObjects);
	for (UObject* InstancedSubObject : InstancedSubObjects)
	{
		checkf(InstancedSubObject, TEXT("Expecting non null SubObjects"));
		OverrideInstancedSubObject(Object, *InstancedSubObject);
	}
#endif // WITH_EDITORONLY_DATA
}

void FOverridableManager::OverrideProperty(UObject& Object, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain& PropertyChain)
{
#ifdef WITH_EDITORONLY_DATA
	if (FOverriddenPropertySet* ThisObjectOverriddenProperties = OverriddenObjectAnnotations.Find(Object))
	{
		ThisObjectOverriddenProperties->OverrideProperty(PropertyEvent, PropertyChain.GetActiveMemberNode() ? PropertyChain.GetActiveMemberNode() : PropertyChain.GetHead(), &Object);
	}
#endif // WITH_EDITORONLY_DATA
}

bool FOverridableManager::ClearOverriddenProperty(UObject& Object, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode)
{
#ifdef WITH_EDITORONLY_DATA
	if (FOverriddenPropertySet* ThisObjectOverriddenProperties = OverriddenObjectAnnotations.Find(Object))
	{
		return ThisObjectOverriddenProperties->ClearOverriddenProperty(PropertyEvent, PropertyNode);
	}
	return false;
#endif // WITH_EDITORONLY_DATA
}

void FOverridableManager::PreOverrideProperty(UObject& Object, const FEditPropertyChain& PropertyChain)
{
#ifdef WITH_EDITORONLY_DATA
	if (FOverriddenPropertySet* ThisObjectOverriddenProperties = OverriddenObjectAnnotations.Find(Object))
	{
		ThisObjectOverriddenProperties->NotifyPropertyChange(EPropertyNotificationType::PreEdit, FPropertyChangedEvent(nullptr), PropertyChain.GetActiveMemberNode() ? PropertyChain.GetActiveMemberNode() : PropertyChain.GetHead(), &Object);
	}
#endif // WITH_EDITORONLY_DATA
}

void FOverridableManager::PostOverrideProperty(UObject& Object, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain& PropertyChain)
{
#ifdef WITH_EDITORONLY_DATA
	if (FOverriddenPropertySet* ThisObjectOverriddenProperties = OverriddenObjectAnnotations.Find(Object))
	{
		ThisObjectOverriddenProperties->NotifyPropertyChange(EPropertyNotificationType::PostEdit, PropertyEvent, PropertyChain.GetActiveMemberNode() ? PropertyChain.GetActiveMemberNode() : PropertyChain.GetHead(), &Object);
	}
#endif // WITH_EDITORONLY_DATA
}

void FOverridableManager::NotifyPropertyChange(const EPropertyNotificationType Notification, UObject& Object, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode)
{
#ifdef WITH_EDITORONLY_DATA
	if (FOverriddenPropertySet* ThisObjectOverriddenProperties = OverriddenObjectAnnotations.Find(Object))
	{
		ThisObjectOverriddenProperties->NotifyPropertyChange(Notification, PropertyEvent, PropertyNode, &Object);
	}
#endif // WITH_EDITORONLY_DATA
}

EOverriddenPropertyOperation FOverridableManager::GetOverriddenPropertyOperation(UObject& Object, const FPropertyChangedEvent& PropertyEvent, const FEditPropertyChain::TDoubleLinkedListNode* PropertyNode, bool* bOutInheritedOperation)
{
#ifdef WITH_EDITORONLY_DATA
	if (const FOverriddenPropertySet* ThisObjectOverriddenProperties = OverriddenObjectAnnotations.Find(Object))
	{
		return ThisObjectOverriddenProperties->GetOverriddenPropertyOperation(PropertyEvent, PropertyNode, bOutInheritedOperation);
	}
	return EOverriddenPropertyOperation::None;
#endif // WITH_EDITORONLY_DATA
}

void FOverridableManager::ClearOverrides(UObject& Object)
{
#ifdef WITH_EDITORONLY_DATA
	if(FOverriddenPropertySet* ThisObjectOverriddenProperties = OverriddenObjectAnnotations.Find(Object))
	{
		ThisObjectOverriddenProperties->Reset();
	}
	PropagateClearOverridesToInstancedSubObjects(Object);
#endif // WITH_EDITORONLY_DATA
}

void FOverridableManager::ClearInstancedSubObjectOverrides(UObject& Object, UObject& InstancedSubObject)
{
#ifdef WITH_EDITORONLY_DATA
	if (InstancedSubObject.IsIn(&Object))
	{
		ClearOverrides(InstancedSubObject);
	}
#endif // WITH_EDITORONLY_DATA
}

void FOverridableManager::PropagateClearOverridesToInstancedSubObjects(UObject& Object)
{
#ifdef WITH_EDITORONLY_DATA
	TSet<UObject*> InstancedSubObjects;
	FFindInstancedReferenceSubobjectHelper::GetInstancedSubObjects(&Object, InstancedSubObjects);
	for (UObject* InstancedSubObject : InstancedSubObjects)
	{
		checkf(InstancedSubObject, TEXT("Expecting non null SubObjects"));

		// There are some cases where the property has information about that should be an instanced subobject, but it is not owned by us.
		ClearInstancedSubObjectOverrides(Object, *InstancedSubObject);
	}
#endif // WITH_EDITORONLY_DATA
}

void FOverridableManager::SerializeOverriddenProperties(UObject& Object, FStructuredArchive::FRecord ObjectRecord)
{
#ifdef WITH_EDITORONLY_DATA
	const FArchiveState& ArchiveState = ObjectRecord.GetArchiveState();
	FOverriddenPropertySet* OverriddenProperties = ArchiveState.IsSaving() ? GetOverriddenProperties(Object) : nullptr;
	TOptional<FStructuredArchiveSlot> OverridenPropertiesSlot = ObjectRecord.TryEnterField(TEXT("OverridenProperties"), OverriddenProperties != nullptr);
	if (OverridenPropertiesSlot.IsSet())
	{
		EOverriddenPropertyOperation Operation = OverriddenProperties ? OverriddenProperties->GetOverriddenPropertyOperation((FArchiveSerializedPropertyChain*)nullptr, (FProperty*)nullptr) : EOverriddenPropertyOperation::None;
		*OverridenPropertiesSlot << SA_ATTRIBUTE( TEXT("OverriddenOperation"), Operation);

		if (ArchiveState.IsLoading())
		{
			OverriddenProperties = &SetOverriddenProperties(Object, Operation);
		}

		if (Operation != EOverriddenPropertyOperation::None)
		{
			FOverriddenPropertySet::StaticStruct()->SerializeItem(*OverridenPropertiesSlot, OverriddenProperties, /* Defaults */ nullptr);
		}
	}
	else if (ArchiveState.IsLoading())
	{
		Disable(Object);
	}
#endif // WITH_EDITORONLY_DATA
}

FOverridableManager::FOverridableManager()
{
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectsReinstanced.AddRaw(this, &FOverridableManager::HandleObjectsReInstantiated);
#endif
}

void FOverridableManager::HandleObjectsReInstantiated(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
#ifdef WITH_EDITORONLY_DATA
	const TMap<const UObjectBase *, FOverriddenPropertyAnnotation>& AnnotationMap = OverriddenObjectAnnotations.GetAnnotationMap();
	for (const auto& Pair : AnnotationMap)
	{
		if( FOverriddenPropertySet* OverridenProperties = Pair.Value.OverriddenProperties.Get())
		{
			OverridenProperties->HandleObjectsReInstantiated(OldToNewInstanceMap);
		}
	}
#endif // WITH_EDITORONLY_DATA
}
