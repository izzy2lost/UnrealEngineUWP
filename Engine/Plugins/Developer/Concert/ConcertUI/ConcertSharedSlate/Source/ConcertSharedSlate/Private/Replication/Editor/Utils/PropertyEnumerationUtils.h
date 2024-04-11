// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Containers/ContainersFwd.h"
#include "HAL/Platform.h"
#include "Templates/FunctionFwd.h"

enum class EBreakBehavior : uint8;
struct FConcertPropertyChain;
struct FSoftClassPath;
struct FSoftObjectPath;

namespace UE::ConcertSharedSlate
{
	class IPropertySelectionSourceModel;
	class IReplicationStreamModel;
	using FEnumerateProperties = TFunctionRef<EBreakBehavior(const FSoftClassPath&, const FConcertPropertyChain&)>;
	
	/** Gets the class from the model or loads it. This function is designed to be used without assuming that it is run in editor-builds. */
	FSoftClassPath GetObjectClassFromModelOrLoad(const FSoftObjectPath& Object, const IReplicationStreamModel& Model);

	/** Calls EnumerateRegisteredPropertiesOnly or EnumerateAllProperties depending on whether OptionalSource is nullptr. */
	void EnumerateProperties(const TSet<FSoftObjectPath>& Objects, const IReplicationStreamModel& Model, const IPropertySelectionSourceModel* OptionalSource, FEnumerateProperties Callback);
	/** Enumerates the properties that are assigned to the object in Model */
	void EnumerateRegisteredPropertiesOnly(const TSet<FSoftObjectPath>& Objects, const IReplicationStreamModel& Model, FEnumerateProperties Callback);
	/** Enumerate the properties that are selectable in Source (e.g. all properties in that class, @see FSelectPropertyFromUClassModel). */
	void EnumerateAllProperties(const TSet<FSoftObjectPath>& Objects, const IPropertySelectionSourceModel& Source, const IReplicationStreamModel& Model, FEnumerateProperties Callback);
}