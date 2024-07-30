// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "BaseGizmos/ParameterToTransformAdapters.h"
#include "BaseGizmos/GizmoMath.h"


void UGizmoAxisTranslationParameterSource::SetParameter(float NewValue)
{
	Parameter = NewValue;
	LastChange.CurrentValue = NewValue;

	double UseDelta = LastChange.GetChangeDelta();

	// check for any constraints on the delta value
	double SnappedDelta = 0;
	if (AxisDeltaConstraintFunction(UseDelta, SnappedDelta))
	{
		UseDelta = SnappedDelta;
	}

	// construct translation as delta from initial position
	FVector Translation = UseDelta * CurTranslationAxis;

	// translate the initial transform
	FTransform NewTransform = InitialTransform;
	NewTransform.AddToTranslation(Translation);

	// apply position constraint
	FVector SnappedPos;
	if (PositionConstraintFunction(NewTransform.GetTranslation(), SnappedPos))
	{
		FVector SnappedLinePos = GizmoMath::ProjectPointOntoLine(SnappedPos, CurTranslationOrigin, CurTranslationAxis);
		NewTransform.SetTranslation(SnappedLinePos);
	}

	TransformSource->SetTransform(NewTransform);

	OnParameterChanged.Broadcast(this, LastChange);
}

void UGizmoAxisTranslationParameterSource::BeginModify()
{
	check(AxisSource);

	LastChange = FGizmoFloatParameterChange(Parameter);

	InitialTransform = TransformSource->GetTransform();
	CurTranslationAxis = AxisSource->GetDirection();
	CurTranslationOrigin = AxisSource->GetOrigin();
}

void UGizmoAxisTranslationParameterSource::EndModify()
{
}







void UGizmoPlaneTranslationParameterSource::SetParameter(const FVector2D& NewValue)
{
	Parameter = NewValue;
	LastChange.CurrentValue = NewValue;

	// construct translation as delta from initial position
	FVector2D Delta = LastChange.GetChangeDelta();
	double UseDeltaX = Delta.X;
	double UseDeltaY = Delta.Y;

	// check for any constraints on the delta value
	double SnappedDeltaX = 0, SnappedDeltaY = 0;
	if (AxisXDeltaConstraintFunction(UseDeltaX, SnappedDeltaX))
	{
		UseDeltaX = SnappedDeltaX;
	}
	if (AxisYDeltaConstraintFunction(UseDeltaY, SnappedDeltaY))
	{
		UseDeltaY = SnappedDeltaY;
	}

	FVector Translation = UseDeltaX*CurTranslationAxisX + UseDeltaY*CurTranslationAxisY;

	// apply translation to initial transform
	FTransform NewTransform = InitialTransform;
	NewTransform.AddToTranslation(Translation);

	// apply position constraint
	FVector SnappedPos;
	if (PositionConstraintFunction(NewTransform.GetTranslation(), SnappedPos))
	{
		FVector PlanePos = GizmoMath::ProjectPointOntoPlane(SnappedPos, CurTranslationOrigin, CurTranslationNormal);
		NewTransform.SetTranslation(PlanePos);
	}

	TransformSource->SetTransform(NewTransform);

	OnParameterChanged.Broadcast(this, LastChange);
}

void UGizmoPlaneTranslationParameterSource::BeginModify()
{
	check(AxisSource);

	LastChange = FGizmoVec2ParameterChange(Parameter);

	// save initial transformation and axis information
	InitialTransform = TransformSource->GetTransform();
	CurTranslationOrigin = AxisSource->GetOrigin();
	AxisSource->GetAxisFrame(CurTranslationNormal, CurTranslationAxisX, CurTranslationAxisY);
}

void UGizmoPlaneTranslationParameterSource::EndModify()
{
}



void UGizmoAxisRotationParameterSource::SetParameter(float NewValue)
{
	Angle = NewValue;
	LastChange.CurrentValue = NewValue;

	double AngleDelta = LastChange.GetChangeDelta();
	double SnappedDelta;
	if (AngleDeltaConstraintFunction(AngleDelta, SnappedDelta))
	{
		AngleDelta = SnappedDelta;
	}

	// construct rotation as delta from initial position
	FQuat DeltaRotation(CurRotationAxis, AngleDelta);
	DeltaRotation = RotationConstraintFunction(DeltaRotation);

	// rotate the vector from the rotation origin to the transform origin, 
	// to get the translation of the origin produced by the rotation
	FVector DeltaPosition = InitialTransform.GetLocation() - CurRotationOrigin;
	DeltaPosition = DeltaRotation * DeltaPosition;
	FVector NewLocation = CurRotationOrigin + DeltaPosition;

	// rotate the initial transform by the rotation
	FQuat NewRotation = DeltaRotation * InitialTransform.GetRotation();

	// construct new transform
	FTransform NewTransform = InitialTransform;
	NewTransform.SetLocation(NewLocation);
	NewTransform.SetRotation(NewRotation);
	TransformSource->SetTransform(NewTransform);

	OnParameterChanged.Broadcast(this, LastChange);
}


void UGizmoAxisRotationParameterSource::BeginModify()
{
	check(AxisSource != nullptr);

	LastChange = FGizmoFloatParameterChange(Angle);

	// save initial transformation and axis information
	InitialTransform = TransformSource->GetTransform();
	CurRotationAxis = AxisSource->GetDirection();
	CurRotationOrigin = AxisSource->GetOrigin();
}

void UGizmoAxisRotationParameterSource::EndModify()
{
}




void UGizmoUniformScaleParameterSource::SetParameter(const FVector2D& NewValue)
{
	Parameter = NewValue;
	LastChange.CurrentValue = NewValue;

	// Convert 2D parameter delta to a 1D uniform scale change
	// This possibly be exposed as a TFunction to allow customization?
	double SignedDelta = LastChange.GetChangeDelta().X + LastChange.GetChangeDelta().Y;
	SignedDelta *= ScaleMultiplier;
	SignedDelta += 1.0;

	FTransform NewTransform = InitialTransform;
	const FVector StartScale = InitialTransform.GetScale3D();
	
	double SnappedDelta;
	
	// if using snapping while scaling
	if (ScaleAxisDeltaConstraintFunction(SignedDelta, SnappedDelta))
	{
		SignedDelta = SnappedDelta;
	}
	// ensures that all 3 axes scale proportionally while following closest snap factor
	// ex: Scale Snap is set to 1, StartScale = (1,2,3), when uniform scaling NewScale=(2, 4, 6), instead of (2, 3, 4) to retain proportions
	const FVector NewScale = SignedDelta * StartScale;

	// currently calling ScaleConstraintFunction has no effect (no changes made to SignedDelta) because this constraint function
	// is intended to relate to WorldGridSnapping. Currently WorldGridSnapping has no effect on scaling, with or without normal snapping
	// because the viewport scale mode fixes the transform space to local
	SignedDelta = ScaleConstraintFunction(SignedDelta);

	NewTransform.SetScale3D(NewScale);

	TransformSource->SetTransform(NewTransform);

	OnParameterChanged.Broadcast(this, LastChange);
}

void UGizmoUniformScaleParameterSource::BeginModify()
{
	check(AxisSource);

	LastChange = FGizmoVec2ParameterChange(Parameter);

	// save initial transformation and axis information
	InitialTransform = TransformSource->GetTransform();
	CurScaleOrigin = AxisSource->GetOrigin();
	// note: currently not used!
	AxisSource->GetAxisFrame(CurScaleNormal, CurScaleAxisX, CurScaleAxisY);
}

void UGizmoUniformScaleParameterSource::EndModify()
{
}





void UGizmoAxisScaleParameterSource::SetParameter(float NewValue)
{
	Parameter = NewValue;
	LastChange.CurrentValue = NewValue;
	
	double ScaleDelta = LastChange.GetChangeDelta();
	ScaleDelta *= ScaleMultiplier;
	ScaleDelta += 1.0;

	FTransform NewTransform = InitialTransform;
	const FVector StartScale = InitialTransform.GetScale3D();

	FVector NewScale;
	double SnappedDelta;
	
	// check for any constraints on the delta value
	if (ScaleAxisDeltaConstraintFunction(ScaleDelta, SnappedDelta))
	{
		ScaleDelta = SnappedDelta;
		const FVector ScaleDeltaVector = ScaleDelta * CurScaleAxis;
		// uses addition when snapping
		// ex: Scale Snap is set to 1, StartScale=(2,2,2), when scaling X axis NewScale=(3,2,2), will NOT be (4,2,2)
		// Note: Plane/Uniform Scale Snapping not implemented in this way because they
		// need to preserve proportional relationship between 2+ axes, therefore use multiplication
		NewScale = StartScale + (ScaleDeltaVector + CurScaleAxis);
	}
	else
	{
		NewScale = StartScale + ScaleDelta * CurScaleAxis;
	}

	// currently calling ScaleConstraintFunction has no effect (no changes made to ScaleDelta) because this constraint function
	// is intended to relate to WorldGridSnapping. Currently WorldGridSnapping has no effect on scaling, with or without normal snapping
	// because the viewport scale mode fixes the transform space to local
	ScaleDelta = ScaleConstraintFunction(ScaleDelta);

	if (bClampToZero)
	{
		NewScale = FVector::Max(FVector::ZeroVector, NewScale);
	}

	NewTransform.SetScale3D(NewScale);

	TransformSource->SetTransform(NewTransform);

	OnParameterChanged.Broadcast(this, LastChange);
}

void UGizmoAxisScaleParameterSource::BeginModify()
{
	check(AxisSource);

	LastChange = FGizmoFloatParameterChange(Parameter);

	InitialTransform = TransformSource->GetTransform();
		
	CurScaleAxis = AxisSource->GetDirection();
	CurScaleOrigin = AxisSource->GetOrigin();
}

void UGizmoAxisScaleParameterSource::EndModify()
{
}





void UGizmoPlaneScaleParameterSource::SetParameter(const FVector2D& NewValue)
{
	Parameter = NewValue;
	LastChange.CurrentValue = NewValue;

	// construct Scale as delta from initial position
	FVector2D ScaleDelta = LastChange.GetChangeDelta() * ScaleMultiplier;

	if (bUseEqualScaling)
	{
		ScaleDelta = FVector2D(ScaleDelta.X + ScaleDelta.Y);
	}

	FTransform NewTransform = InitialTransform;
	const FVector StartScale = InitialTransform.GetScale3D();

	double UseScaleDeltaX = ScaleDelta.X;
	double UseScaleDeltaY = ScaleDelta.Y;
	
	FVector NewScale;

	if (bUseEqualScaling)
	{
		double SnappedDeltaX = 0.0, SnappedDeltaY = 0.0;
		
		// if using snapping while scaling on X and Y axis
		if (ScaleAxisXDeltaConstraintFunction(UseScaleDeltaX, SnappedDeltaX))
		{
			UseScaleDeltaX = SnappedDeltaX;
		}
		if (ScaleAxisYDeltaConstraintFunction(UseScaleDeltaY, SnappedDeltaY))
		{
			UseScaleDeltaY = SnappedDeltaY;
		}
		// ensures that 2 axes on plane scale proportionally while following closest snap factor
		// ex: Scale Snap is set to 1, StartScale = (1,2,3), scaling on X axis NewScale=(1, 4, 6), instead of (1, 3, 4)
		NewScale = StartScale + (StartScale*(UseScaleDeltaX*CurScaleAxisX)) + (StartScale*(UseScaleDeltaY*CurScaleAxisY));
	}
	else
	{
		NewScale = StartScale + ScaleDelta.X*CurScaleAxisX + ScaleDelta.Y*CurScaleAxisY;
	}
	
	// currently calling ScaleConstraintFunction has no effect (no changes made to SignedDelta) because this constraint function
	// is intended to relate to WorldGridSnapping. Currently WorldGridSnapping has no effect on scaling, with or without normal snapping
	// because the viewport scale mode fixes the transform space to local
	ScaleDelta = ScaleConstraintFunction(ScaleDelta);

	if (bClampToZero)
	{
		NewScale = FVector::Max(NewScale, FVector::ZeroVector);
	}

	NewTransform.SetScale3D(NewScale);

	TransformSource->SetTransform(NewTransform);

	OnParameterChanged.Broadcast(this, LastChange);
}

void UGizmoPlaneScaleParameterSource::BeginModify()
{
	check(AxisSource);

	LastChange = FGizmoVec2ParameterChange(Parameter);

	// save initial transformation and axis information
	InitialTransform = TransformSource->GetTransform();
	CurScaleOrigin = AxisSource->GetOrigin();
	AxisSource->GetAxisFrame(CurScaleNormal, CurScaleAxisX, CurScaleAxisY);
}

void UGizmoPlaneScaleParameterSource::EndModify()
{
}