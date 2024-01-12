// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Misc/EBreakBehavior.h"
#include "Templates/Function.h"

enum class EBreakBehavior : uint8;
struct FSoftObjectPath;

namespace UE::ConcertSharedSlate
{
	/**
	 * Decides on the contents for the subobject tree view.
	 */
	class CONCERTSHAREDSLATE_API ISubobjectModel
	{
	public:

		/** Sets the object for which to build the subobject hierarchy. */
		virtual void SetTopLevelObject(const FSoftObjectPath& TopLevelObject) = 0;
		virtual FSoftObjectPath GetTopLevelObject() const = 0;

		/** @return Whether Object is a top-level object, i.e. valid to pass to SetTopLevelObject. */
		virtual bool IsTopLevelObject(const FSoftObjectPath& Object) const = 0;

		/**
		 * Gets the categories that can be passed to ForEachRootSubobject.
		 * Objects are grouped by category. The categories are separated by separator lines.
		 * Example: One category could be the the USceneComponent hierarchy and another could be all other UActorComponents (like in the SSubobjectEditor).
		 */
		virtual TArray<FName> GetCategories() const = 0;
		
		/**
		 * Gets the direct subobjects of the top level objects.
		 * @param Category The category for the objects.
		 * @param Callback Callback to invoke for each found root object.
		 * @see GetCategories for valid categories.
		 */
		virtual void ForEachRootSubobject(FName Category, TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Callback) const = 0;
		/** Gets the direct subobject children of another subobject. Child subobjects are implicitly in the same category as Parent. */
		virtual void ForEachDirectChildSubobject(const FSoftObjectPath& Parent, TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Callback) const = 0;

		/** Util for iterating all subobjects. */
		void ForEachSubobject(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Parent, const FSoftObjectPath& ChildObject)> Callback);

		virtual ~ISubobjectModel() = default;
	};
}
