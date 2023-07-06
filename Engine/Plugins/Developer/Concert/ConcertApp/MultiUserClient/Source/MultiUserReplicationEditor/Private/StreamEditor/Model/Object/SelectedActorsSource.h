// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StreamEditor/Model/Object/IObjectSourceModel.h"

namespace UE::MultiUserReplicationEditor
{
	/** Gets the actors currently selected in the editor. */
	class FSelectedActorsSource : public IObjectSourceModel
	{
	public:

		//~ Begin IObjectSourceModel Interface
		virtual ConcertSharedSlate::FSourceDisplayInfo GetDisplayInfo() const override;
		virtual uint32 GetNumSelectableItems() const override;
		virtual void EnumerateSelectableItems(TFunctionRef<EBreakBehavior(const FSelectableObjectInfo& SelectableOption)> Delegate) const override;
		//~ End IObjectSourceModel Interface
	};
}
