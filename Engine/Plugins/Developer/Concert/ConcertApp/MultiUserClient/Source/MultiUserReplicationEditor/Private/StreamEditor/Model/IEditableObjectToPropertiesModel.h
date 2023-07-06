// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Delegates/DelegateCombinations.h"
#include "IObjectToPropertiesModel.h"

class UObject;
struct FConcertPropertyChain;

namespace UE::MultiUserReplicationEditor
{
	enum class EReplicatedObjectChangeReason : uint8
	{
		ChangedDirectly,
		Transacted
	};
	
	/**
	 * Abstracts the concept of mapping objects to properties. This allows writing.
	 * 
	 * Models may not always be writable. When editing a UAsset, it will be writable. However, if we join a multi-user
	 * session we do not want to edit the objects nor properties - only read.
	 */
	class IEditableObjectToPropertiesModel : public IObjectToPropertiesModel
	{
	public:

		/** Adds the objects to the mapping */
		virtual void AddObjects(TArrayView<UObject*> Objects) = 0;
		/** Removes the objects from the mapping */
		virtual void RemoveObjects(TArrayView<FSoftObjectPath> Objects) = 0;

		/** Adds these properties to the object's list of selected properties. */
		virtual void AddProperties(const FSoftObjectPath& Object, TArrayView<FConcertPropertyChain> Properties) = 0;
		/** Removes these properties from the object's list of selected properties. */
		virtual void RemoveProperties(const FSoftObjectPath& Object, TArrayView<FConcertPropertyChain> Properties) = 0;

		/** Called when the object list changes. AddedObjects and RemovedObjects are empty if and only if ChangeReason == EReplicatedObjectChangeReason::Transacted. */
		DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnObjectsChanged, TArrayView<UObject*> AddedObjects, TArrayView<FSoftObjectPath> RemovedObjects, EReplicatedObjectChangeReason ChangeReason);
		virtual FOnObjectsChanged& OnObjectsChanged() = 0;

		/** Called when the property list of some object changes. If the properties are removed because the object is removed outright, FOnObjectsChanged is called instead.  */
		DECLARE_MULTICAST_DELEGATE(FOnPropertiesChanged);
		virtual FOnPropertiesChanged& OnPropertiesChanged() = 0;
	};
}