// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseGizmos/GizmoPrivateUtil.h"

#include "BaseGizmos/GizmoViewContext.h"
#include "ContextObjectStore.h"
#include "InteractiveGizmoManager.h"

UGizmoViewContext* UE::GizmoUtil::GetGizmoViewContext(UInteractiveGizmoManager* GizmoManager)
{
	if (!ensure(GizmoManager))
	{
		return nullptr;
	}

	UContextObjectStore* ContextObjectStore = GizmoManager->GetContextObjectStore();
	if (!ensure(ContextObjectStore))
	{
		return nullptr;
	}

	UGizmoViewContext* GizmoViewContext = ContextObjectStore->FindContext<UGizmoViewContext>();
	ensure(GizmoViewContext);

	return GizmoViewContext;
}
