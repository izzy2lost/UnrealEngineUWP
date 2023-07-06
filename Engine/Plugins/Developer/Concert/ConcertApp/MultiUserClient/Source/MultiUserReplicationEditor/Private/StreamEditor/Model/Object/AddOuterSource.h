// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StreamEditor/Model/Object/IObjectSourceModel.h"

namespace UE::MultiUserReplicationEditor
{
	/** Adds the outer object as object. Used as source in context menu. */
	class FAddOuterSource : public IObjectSourceModel
	{
	public:

		bool TrySetObjectIfValid(UObject* InObject);

		//~ Begin IObjectSourceModel Interface
		virtual ConcertSharedSlate::FSourceDisplayInfo GetDisplayInfo() const override;
		virtual uint32 GetNumSelectableItems() const override;
		virtual void EnumerateSelectableItems(TFunctionRef<EBreakBehavior(const FSelectableObjectInfo& SelectableOption)> Delegate) const override;
		//~ End IObjectSourceModel Interface

	private:

		/** The object whose outer to return. */
		TWeakObjectPtr<UObject> Object;
	};
}


