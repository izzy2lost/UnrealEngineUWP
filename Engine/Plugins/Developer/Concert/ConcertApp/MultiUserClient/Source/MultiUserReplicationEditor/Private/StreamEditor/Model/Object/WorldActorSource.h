// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StreamEditor/Model/Object/IObjectSourceModel.h"

namespace UE::MultiUserReplicationEditor
{
	/** Gets all the actors in the current GWorld */
	class FWorldActorSource : public IObjectSourceModel
	{
	public:

		//~ Begin IObjectSourceModel Interface
		virtual ConcertSharedSlate::FSourceDisplayInfo GetDisplayInfo() const override;
		virtual uint32 GetNumSelectableItems() const override;
		virtual void EnumerateSelectableItems(TFunctionRef<EBreakBehavior(const FSelectableObjectInfo& SelectableOption)> Delegate) const override;
		//~ End IObjectSourceModel Interface
	};
}


