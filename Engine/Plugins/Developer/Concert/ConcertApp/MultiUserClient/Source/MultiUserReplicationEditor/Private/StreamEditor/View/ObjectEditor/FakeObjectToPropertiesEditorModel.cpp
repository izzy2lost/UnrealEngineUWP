// Copyright Epic Games, Inc. All Rights Reserved.

#include "FakeObjectToPropertiesEditorModel.h"

#include "StreamEditor/Model/Property/IPropertySelectionSourceModel.h"
#include "StreamEditor/Model/Property/IPropertySourceModel.h"
#include "Replication/PropertyChainUtils.h"

namespace UE::MultiUserReplicationEditor
{
	bool FFakeObjectToPropertiesEditorModel::ForEachProperty(const FSoftObjectPath& Object, TFunctionRef<EBreakBehavior(const FConcertPropertyChain& Property)> Delegate) const
	{
		PropertySelectionSource->GetPropertySource(GetObjectClass(Object))
			->EnumerateSelectableItems([&Delegate](const FSelectablePropertyInfo& PropertyInfo)
			{
				return Delegate(PropertyInfo.Property);
			});
		return true;
	}
}
