// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/Function.h"

class UObject;
struct FConcertPropertyChain;

namespace UE::ConcertClientSharedSlate
{
	/** When an object is added to IEditableReplicationStreamModel, these functions are called to allow adding additional objects and properties. */
	class CONCERTCLIENTSHAREDSLATE_API IStreamExtender
	{
	public:

		using FEnumerateProperties = TFunctionRef<void(FConcertPropertyChain)>;
		using FEnumerateObjects = TFunctionRef<void(UObject&)>;

		/** Called when Object has just been added to the model: allows binding some default properties to the object. */
		virtual void ExtendObjectProperties(UObject& Object, FEnumerateProperties ForEachPropertyToAdd) {}

		/** Called when object has just been added to the model: allows adding some additional object, for which this extender will be invoked recursively. */
		virtual void AppendAdditionalObjects(UObject& Object, FEnumerateObjects ForEachAdditionalObject) {}

		virtual ~IStreamExtender() = default;
	};
}
