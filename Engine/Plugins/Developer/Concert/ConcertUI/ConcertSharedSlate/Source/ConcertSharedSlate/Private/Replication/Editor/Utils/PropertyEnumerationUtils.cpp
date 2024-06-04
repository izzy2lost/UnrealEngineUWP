// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyEnumerationUtils.h"

#include "Misc/EBreakBehavior.h"
#include "Replication/Editor/Model/IReplicationStreamModel.h"
#include "Replication/Editor/Model/Property/IPropertySelectionSourceModel.h"

#include "Containers/Set.h"
#include "UObject/SoftObjectPath.h"

namespace UE::ConcertSharedSlate
{
	/** Gets the class from the model or loads it. This function is designed to be used without assuming that it is run in editor-builds. */
	FSoftClassPath GetObjectClassFromModelOrLoad(const FSoftObjectPath& Object, const IReplicationStreamModel& Model)
	{
		const FSoftClassPath ResolvedClass = Model.GetObjectClass(Object);
		if (ResolvedClass.IsValid())
		{
			return ResolvedClass;
		}

#if WITH_EDITOR
		// The object is not yet in the model.
		const UObject* LoadedObject = Object.ResolveObject();
		return LoadedObject ? LoadedObject->GetClass() : FSoftClassPath{};
#else
		return FSoftClassPath{};
#endif
	}

	void EnumerateProperties(TConstArrayView<FSoftObjectPath> Objects, const IReplicationStreamModel& Model, const IPropertySelectionSourceModel* OptionalSource, FEnumerateProperties Callback)
	{
		if (OptionalSource)
		{
			EnumerateAllProperties(Objects, *OptionalSource, Model, Callback);
		}
		else
		{
			EnumerateRegisteredPropertiesOnly(Objects, Model, Callback);
		}
	}

	/** Enumerates the properties that are assigned to the object in Model */
	void EnumerateRegisteredPropertiesOnly(TConstArrayView<FSoftObjectPath> Objects, const IReplicationStreamModel& Model, FEnumerateProperties Callback)
	{
		for (const FSoftObjectPath& Object : Objects)
		{
			const FSoftClassPath ObjectClass = GetObjectClassFromModelOrLoad(Object, Model);

			EBreakBehavior BreakBehavior = EBreakBehavior::Continue;
			Model.ForEachProperty(Object, [&Callback, &ObjectClass, &BreakBehavior](const FConcertPropertyChain& Chain)
			{
				BreakBehavior = Callback(ObjectClass, Chain);
				return BreakBehavior;
			});

			if (BreakBehavior == EBreakBehavior::Break)
			{
				break;
			}
		}
	}

	/** Enumerate the properties that are selectable in Source (e.g. all properties in that class, @see FSelectPropertyFromUClassModel). */
	void EnumerateAllProperties(TConstArrayView<FSoftObjectPath> Objects, const IPropertySelectionSourceModel& Source, const IReplicationStreamModel& Model, FEnumerateProperties Callback)
	{
		TSet<FSoftClassPath> VisitedClasses; 
			
		for (const FSoftObjectPath& Object : Objects)
		{
			const FSoftClassPath ObjectClass = GetObjectClassFromModelOrLoad(Object, Model);
			if (VisitedClasses.Contains(ObjectClass))
			{
				continue;
			}
			VisitedClasses.Add(ObjectClass);
					
			EBreakBehavior BreakBehavior = EBreakBehavior::Continue;
			Source.GetPropertySource(ObjectClass)->EnumerateSelectableItems([&Callback, &ObjectClass, &BreakBehavior](const FSelectablePropertyInfo& PropertyInfo)
			{
				BreakBehavior = Callback(ObjectClass, PropertyInfo.Property);
				return BreakBehavior;
			});
					
			if (BreakBehavior == EBreakBehavior::Break)
			{
				break;
			}
		}
	}
}