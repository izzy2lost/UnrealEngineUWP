// Copyright Epic Games, Inc. All Rights Reserved.


#include "EditorGizmos/EditorTransformGizmoDataBinder.h"

#include "EditorModeManager.h"
#include "EditorGizmos/EditorTransformGizmoUtil.h"
#include "EditorGizmos/TransformGizmo.h"

FEditorTransformGizmoDataBinder::~FEditorTransformGizmoDataBinder()
{
	const TSet<TWeakObjectPtr<UTransformGizmo>> Gizmos(BoundGizmos);
	for (const TWeakObjectPtr<UTransformGizmo> Gizmo : Gizmos)
	{
		if (Gizmo.IsValid())
		{
			UnbindFromGizmo(Gizmo.Get(), Gizmo->ActiveTarget);
			Gizmo->OnSetActiveTarget.RemoveAll(this);
			Gizmo->OnAboutToClearActiveTarget.RemoveAll(this);
		}
	}
	BoundGizmos.Empty();

	if (WeakContext.IsValid())
	{
		WeakContext->OnGizmoCreatedDelegate().RemoveAll(this);
	}
	WeakContext.Reset();
}

void FEditorTransformGizmoDataBinder::BindToGizmoContextObject(UEditorTransformGizmoContextObject* InContextObject)
{
	if (!ensure(IsValid(InContextObject)))
	{
		return;
	}

	InContextObject->OnGizmoCreatedDelegate().AddSP(this, &FEditorTransformGizmoDataBinder::BindToUninitializedGizmo);
	
	WeakContext = InContextObject;
}

void FEditorTransformGizmoDataBinder::BindToUninitializedGizmo(UTransformGizmo* InGizmo)
{
	FEditorModeTools* ModeTools = WeakContext.IsValid() ? WeakContext->GetModeTools() : nullptr;
	if (ensure(ModeTools))
	{
		if (!ModeTools->OnWidgetModeChanged().IsBoundToObject(InGizmo))
		{
			ModeTools->OnWidgetModeChanged().AddUObject(InGizmo, &UTransformGizmo::HandleWidgetModeChanged);
		}
	}

	const bool bSetBound = InGizmo->OnSetActiveTarget.IsBoundToObject(this);
	const bool bClearBound = InGizmo->OnAboutToClearActiveTarget.IsBoundToObject(this);
	if ( ensure(!bSetBound && !bClearBound))
	{
		InGizmo->OnSetActiveTarget.AddSP(this, &FEditorTransformGizmoDataBinder::BindToInitializedGizmo);
		InGizmo->OnAboutToClearActiveTarget.AddSP(this, &FEditorTransformGizmoDataBinder::UnbindFromGizmo);
	}
}

void FEditorTransformGizmoDataBinder::BindToInitializedGizmo(UTransformGizmo* InGizmo, UTransformProxy* InProxy)
{
	const bool bHasTarget = InGizmo && InGizmo->ActiveTarget && InGizmo->ActiveTarget == InProxy; 
	if (ensure(!bHasTarget && !BoundGizmos.Contains(InGizmo)))
	{
		return;
	}

	// bind InProxy OnBeginTransformEdit/OnTransformChanged/OnEndTransformEdit etc here if needed

	BoundGizmos.Add(InGizmo);
}

void FEditorTransformGizmoDataBinder::UnbindFromGizmo(UTransformGizmo* InGizmo, UTransformProxy* InProxy)
{
	if (!InGizmo)
	{
		return;
	}

	FEditorModeTools* ModeTools = WeakContext.IsValid() ? WeakContext->GetModeTools() : nullptr;
	if (ensure(ModeTools))
	{
		ModeTools->OnWidgetModeChanged().RemoveAll(InGizmo);
	}

	// unbind InProxy OnBeginTransformEdit/OnTransformChanged/OnEndTransformEdit etc here if needed

	BoundGizmos.Remove(InGizmo);
}
