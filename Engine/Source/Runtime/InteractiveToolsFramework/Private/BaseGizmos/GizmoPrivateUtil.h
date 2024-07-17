// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class UGizmoViewContext;
class UInteractiveGizmoManager;

namespace UE::GizmoUtil
{
	struct FTransformSubGizmoCommonParams;
	struct FTransformSubGizmoSharedState;
}

/**
 * This file holds implementation helpers that don't necessarily need exposing. If eventually needed,
 *  we can move some of these into GizmoUtil or TransformSubGizmoUtil
 */

namespace UE::GizmoUtil 
{

	/**
	 * Simple helper that gets the gizmo view context out of the context object store associated with a gizmo manager.
	 * Fires ensures if it does not find the expected objects along the way.
	 */
	UGizmoViewContext* GetGizmoViewContext(UInteractiveGizmoManager* GizmoManager);

}// end UE::GizmoUtil