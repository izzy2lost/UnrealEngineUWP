// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertySourceModel.h"
#include "Model/Item/SourceSelectionCategory.h"

#include "Templates/SharedPointer.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"

namespace UE::ConcertSharedSlate
{
	using FPropertySourceCategory = ConcertSharedSlate::TSourceSelectionCategory<FSelectablePropertyInfo>;

	/** Arguments for getting properties associated with an object / class. */
	struct FPropertySourceContext
	{
		/** The object for which the properties are supposed to be displayed */
		TSoftObjectPtr<> Object;
		/** The class of Object */
		FSoftClassPath Class;

		FPropertySourceContext(const TSoftObjectPtr<>& Object, const FSoftClassPath& Class)
			: Object(Object)
			, Class(Class)
		{}
	};
	
	/** Decides which properties can be added to a IReplicationStreamModel. */
	class IPropertySelectionSourceModel : public TSharedFromThis<IPropertySelectionSourceModel>
	{
	public:

		/** Gets the single source determining which properties can be selected. */
		UE_DEPRECATED(5.5, "Use the version that takes FPropertySourceContext instead.")
		virtual TSharedRef<IPropertySourceModel> GetPropertySource(const FSoftClassPath& Class) const { return GetPropertySource(FPropertySourceContext({}, Class)); }

		/** @return Gets an object that will tell you the properties that are associated with the passed in object / class context. */
		virtual TSharedRef<IPropertySourceModel> GetPropertySource(const FPropertySourceContext& Context) const = 0;
		
		virtual ~IPropertySelectionSourceModel() = default;
	};
}
