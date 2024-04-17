// Copyright Epic Games, Inc. All Rights Reserved.


#include "Tables/NodalOffsetTable.h"

#include "LensFile.h"
#include "LensTableUtils.h"

int32 FNodalOffsetFocusPoint::GetNumPoints() const
{
	return LocationOffset[0].GetNumKeys();
}

float FNodalOffsetFocusPoint::GetZoom(int32 Index) const
{
	return LocationOffset[0].Keys[Index].Time;
}

bool FNodalOffsetFocusPoint::GetPoint(float InZoom, FNodalPointOffset& OutData, float InputTolerance) const
{	
	for(int32 Index = 0; Index < LocationDimension; ++Index)
	{
		FKeyHandle Handle = LocationOffset[Index].FindKey(InZoom, InputTolerance);
		if(Handle != FKeyHandle::Invalid())
		{
			OutData.LocationOffset[Index] = LocationOffset[Index].GetKeyValue(Handle);
		}
		else
		{
			return false;
		}
	}

	FRotator Rotator;
	for(int32 Index = 0; Index < RotationDimension; ++Index)
	{
		FKeyHandle Handle = RotationOffset[Index].FindKey(InZoom, InputTolerance);
		if(Handle != FKeyHandle::Invalid())
		{
			Rotator.SetComponentForAxis(static_cast<EAxis::Type>(Index+1), RotationOffset[Index].GetKeyValue(Handle));
		}
		else
		{
			return false;
		}
	}

	OutData.RotationOffset = Rotator.Quaternion();

	return true;
}

bool FNodalOffsetFocusPoint::AddPoint(float InZoom, const FNodalPointOffset& InData, float InputTolerance, bool /** bIsCalibrationPoint */)
{
	for(int32 Index = 0; Index < LocationDimension; ++Index)
	{
		FKeyHandle Handle = LocationOffset[Index].FindKey(InZoom, InputTolerance);
		if(Handle != FKeyHandle::Invalid())
		{
			LocationOffset[Index].SetKeyValue(Handle, InData.LocationOffset[Index]);	
		}
		else
		{
			Handle = LocationOffset[Index].AddKey(InZoom, InData.LocationOffset[Index]);
			LocationOffset[Index].SetKeyTangentMode(Handle, ERichCurveTangentMode::RCTM_Auto);
			LocationOffset[Index].SetKeyInterpMode(Handle, RCIM_Cubic);
		}
	}

	const FRotator NewRotator = InData.RotationOffset.Rotator();
	for(int32 Index = 0; Index < RotationDimension; ++Index)
	{
		FKeyHandle Handle = RotationOffset[Index].FindKey(InZoom, InputTolerance);
		if(Handle != FKeyHandle::Invalid())
		{
			RotationOffset[Index].SetKeyValue(Handle, NewRotator.GetComponentForAxis(static_cast<EAxis::Type>(Index+1)));	
		}
		else
		{
			Handle = RotationOffset[Index].AddKey(InZoom, NewRotator.GetComponentForAxis(static_cast<EAxis::Type>(Index+1)));
			RotationOffset[Index].SetKeyTangentMode(Handle, ERichCurveTangentMode::RCTM_Auto);
			RotationOffset[Index].SetKeyInterpMode(Handle, RCIM_Cubic);
		}
	}
	return true;
}

bool FNodalOffsetFocusPoint::SetPoint(float InZoom, const FNodalPointOffset& InData, float InputTolerance)
{
	for(int32 Index = 0; Index < LocationDimension; ++Index)
	{
		FKeyHandle Handle = LocationOffset[Index].FindKey(InZoom, InputTolerance);
		if(Handle != FKeyHandle::Invalid())
		{
			LocationOffset[Index].SetKeyValue(Handle, InData.LocationOffset[Index]);	
		}
		else
		{
			return false;
		}
	}

	const FRotator NewRotator = InData.RotationOffset.Rotator();
	for(int32 Index = 0; Index < RotationDimension; ++Index)
	{
		FKeyHandle Handle = RotationOffset[Index].FindKey(InZoom, InputTolerance);
		if(Handle != FKeyHandle::Invalid())
		{
			RotationOffset[Index].SetKeyValue(Handle, NewRotator.GetComponentForAxis(static_cast<EAxis::Type>(Index+1)));	
		}
		else
		{
			return false;
		}
	}
	return true;
}

void FNodalOffsetFocusPoint::RemovePoint(float InZoomValue)
{
	for(int32 Index = 0; Index < LocationDimension; ++Index)
	{
		const FKeyHandle KeyHandle = LocationOffset[Index].FindKey(InZoomValue);
		if(KeyHandle != FKeyHandle::Invalid())
		{
			LocationOffset[Index].DeleteKey(KeyHandle);
		}
	}

	for(int32 Index = 0; Index < RotationDimension; ++Index)
	{
		const FKeyHandle KeyHandle = RotationOffset[Index].FindKey(InZoomValue);
		if(KeyHandle != FKeyHandle::Invalid())
		{
			RotationOffset[Index].DeleteKey(KeyHandle);
		}
	}
}

bool FNodalOffsetFocusPoint::IsEmpty() const
{
	return LocationOffset[0].IsEmpty();
}

bool FNodalOffsetTable::DoesZoomPointExists(float InFocus, float InZoom, float InputTolerance) const
{
	FNodalPointOffset NodalPointOffset;
	if (GetPoint(InFocus, InZoom, NodalPointOffset, InputTolerance))
	{
		return true;
	}

	return false;
}

const FBaseFocusPoint* FNodalOffsetTable::GetBaseFocusPoint(int32 InIndex) const
{
	if (FocusPoints.IsValidIndex(InIndex))
	{
		return &FocusPoints[InIndex];
	}

	return nullptr;
}

TMap<ELensDataCategory, FLinkPointMetadata> FNodalOffsetTable::GetLinkedCategories() const
{
	static TMap<ELensDataCategory, FLinkPointMetadata> LinkedToCategories =
	{
		{ELensDataCategory::Distortion, {false}},
		{ELensDataCategory::Zoom, {false}},
		{ELensDataCategory::STMap, {false}},
		{ELensDataCategory::ImageCenter, {false}},
	};
	return LinkedToCategories;
}

int32 FNodalOffsetTable::GetTotalPointNum() const
{
	return LensDataTableUtils::GetTotalPointNum(FocusPoints);
}

UScriptStruct* FNodalOffsetTable::GetScriptStruct() const
{
	return StaticStruct();
}

bool FNodalOffsetTable::BuildParameterCurveAtFocus(float InFocus, int32 InParameterIndex, FRichCurve& OutCurve) const
{
	if (!FParameters::IsValidComposed(InParameterIndex))
	{
		return false;
	}

	int32 Parameter;
	EAxis::Type Axis;
	FParameters::Decompose(InParameterIndex, Parameter, Axis);
	
	if (const FNodalOffsetFocusPoint* FocusPoint = GetFocusPoint(InFocus))
	{
		if (Parameter == FParameters::Location)
		{
			OutCurve = FocusPoint->LocationOffset[Axis - 1];
		}
		else
		{
			OutCurve = FocusPoint->RotationOffset[Axis - 1];
		}
		
		return true;
	}

	return false;
}

bool FNodalOffsetTable::BuildParameterCurveAtZoom(float InZoom, int32 InParameterIndex, FRichCurve& OutCurve) const
{
	if (!FParameters::IsValidComposed(InParameterIndex))
	{
		return false;
	}

	int32 Parameter;
	EAxis::Type Axis;
	FParameters::Decompose(InParameterIndex, Parameter, Axis);
	
	for (const FNodalOffsetFocusPoint& FocusPoint : FocusPoints)
	{
		FNodalPointOffset ZoomPoint;
		if (FocusPoint.GetPoint(InZoom, ZoomPoint))
		{
			const float Value = Parameter == 0 ? ZoomPoint.LocationOffset[Axis - 1] : ZoomPoint.RotationOffset.Rotator().GetComponentForAxis(Axis);
			const FKeyHandle NewKeyHandle = OutCurve.AddKey(FocusPoint.Focus, Value);
			FRichCurveKey& NewKey = OutCurve.GetKey(NewKeyHandle);
			NewKey.TangentMode = ERichCurveTangentMode::RCTM_None;
			NewKey.InterpMode = ERichCurveInterpMode::RCIM_Linear;
		}
	}

	return true;
}

void FNodalOffsetTable::SetParameterCurveKeysAtFocus(float InFocus, int32 InParameterIndex, const FRichCurve& InSourceCurve, TArrayView<const FKeyHandle> InKeys)
{
	if (!FParameters::IsValidComposed(InParameterIndex))
	{
		return;
	}
	
	if (FNodalOffsetFocusPoint* FocusPoint = GetFocusPoint(InFocus))
	{
		int32 Parameter;
		EAxis::Type Axis;
		FParameters::Decompose(InParameterIndex, Parameter, Axis);
		
		FRichCurve* ActiveCurve = nullptr;
		if (Parameter == FParameters::Location)
		{
			ActiveCurve = &FocusPoint->LocationOffset[Axis - 1];
		}
		else
		{
			ActiveCurve = &FocusPoint->RotationOffset[Axis - 1];
		}
		
		for (int32 Index = 0; Index < InKeys.Num(); ++Index)
		{
			const FKeyHandle Handle = InKeys[Index];
			const int32 KeyIndex = InSourceCurve.GetIndexSafe(Handle);
			if (KeyIndex != INDEX_NONE)
			{
				ActiveCurve->Keys[KeyIndex] = InSourceCurve.GetKey(Handle);
			}
		}

		ActiveCurve->AutoSetTangents();
	}
}

void FNodalOffsetTable::SetParameterCurveKeysAtZoom(float InZoom, int32 InParameterIndex, const FRichCurve& InSourceCurve, TArrayView<const FKeyHandle> InKeys)
{
	if (!FParameters::IsValidComposed(InParameterIndex))
	{
		return;
	}

	int32 Parameter;
	EAxis::Type Axis;
	FParameters::Decompose(InParameterIndex, Parameter, Axis);
	
	for (const FKeyHandle& KeyHandle : InKeys)
	{
		// Assume the focus keys are put into the source curve in the same order as they are stored internally
		const int32 KeyIndex = InSourceCurve.GetIndexSafe(KeyHandle);
		if (KeyIndex != INDEX_NONE)
		{
			if (ensure(FocusPoints.IsValidIndex(KeyIndex)))
			{
				FNodalOffsetFocusPoint& FocusPoint = FocusPoints[KeyIndex];
				FNodalPointOffset ZoomPoint;
				if (!FocusPoint.GetPoint(InZoom, ZoomPoint))
				{
					continue;
				}

				if (Parameter == FParameters::Location)
				{
					ZoomPoint.LocationOffset[Axis - 1] = InSourceCurve.GetKeyValue(KeyHandle);
				}
				else if (Parameter == FParameters::Rotation)
				{
					FRotator Rotator = ZoomPoint.RotationOffset.Rotator();
					Rotator.SetComponentForAxis(Axis, InSourceCurve.GetKeyValue(KeyHandle));
					ZoomPoint.RotationOffset = Rotator.Quaternion();
				}

				FocusPoint.SetPoint(InZoom, ZoomPoint);

				if (Parameter == FParameters::Location)
				{
					FocusPoint.LocationOffset[Axis - 1].AutoSetTangents();
				}
				else if (Parameter == FParameters::Rotation)
				{
					FocusPoint.RotationOffset[Axis - 1].AutoSetTangents();
				}
			}
		}
	}
}

FText FNodalOffsetTable::GetParameterValueLabel(int32 InParameterIndex) const
{
	if (!FParameters::IsValidComposed(InParameterIndex))
	{
		return FText();
	}
	
	int32 Parameter;
	EAxis::Type Axis;
	FParameters::Decompose(InParameterIndex, Parameter, Axis);
	
	if (Parameter == FParameters::Location)
	{
		return NSLOCTEXT("FNodalOffsetTable", "LocationParameterValueLabel", "(cm)");
	}
	else
	{
		return NSLOCTEXT("FNodalOffsetTable", "RotationParameterValueLabel", "(deg)");
	}
}

FText FNodalOffsetTable::GetParameterValueUnitLabel(int32 InParameterIndex) const
{
	if (!FParameters::IsValidComposed(InParameterIndex))
	{
		return FText();
	}
	
	int32 Parameter;
	EAxis::Type Axis;
	FParameters::Decompose(InParameterIndex, Parameter, Axis);
	
	if (Parameter == FParameters::Location)
	{
		return NSLOCTEXT("FNodalOffsetTable", "LocationParameterUnitLabel", "cm");
	}
	else
	{
		return NSLOCTEXT("FNodalOffsetTable", "RotationParameterUnitLabel", "deg");
	}
}

const FNodalOffsetFocusPoint* FNodalOffsetTable::GetFocusPoint(float InFocus, float InputTolerance) const
{
	return FocusPoints.FindByPredicate([InFocus, InputTolerance](const FNodalOffsetFocusPoint& Point) { return FMath::IsNearlyEqual(Point.Focus, InFocus, InputTolerance); });
}

FNodalOffsetFocusPoint* FNodalOffsetTable::GetFocusPoint(float InFocus, float InputTolerance)
{
	return FocusPoints.FindByPredicate([InFocus, InputTolerance](const FNodalOffsetFocusPoint& Point) { return FMath::IsNearlyEqual(Point.Focus, InFocus, InputTolerance); });
}

TConstArrayView<FNodalOffsetFocusPoint> FNodalOffsetTable::GetFocusPoints() const
{
	return FocusPoints;
}

TArray<FNodalOffsetFocusPoint>& FNodalOffsetTable::GetFocusPoints()
{
	return FocusPoints;
}

void FNodalOffsetTable::ForEachPoint(FFocusPointCallback InCallback) const
{
	for (const FNodalOffsetFocusPoint& Point : FocusPoints)
	{
		InCallback(Point);
	}
}

void FNodalOffsetTable::RemoveFocusPoint(float InFocus)
{
	LensDataTableUtils::RemoveFocusPoint(FocusPoints, InFocus);
}

bool FNodalOffsetTable::HasFocusPoint(float InFocus, float InputTolerance) const
{
	return DoesFocusPointExists(InFocus, InputTolerance);
}

void FNodalOffsetTable::ChangeFocusPoint(float InExistingFocus, float InNewFocus, float InputTolerance)
{
	LensDataTableUtils::ChangeFocusPoint(FocusPoints, InExistingFocus, InNewFocus, InputTolerance);
}

void FNodalOffsetTable::MergeFocusPoint(float InSrcFocus, float InDestFocus, bool bReplaceExistingZoomPoints, float InputTolerance)
{
	LensDataTableUtils::MergeFocusPoint(FocusPoints, InSrcFocus, InDestFocus, bReplaceExistingZoomPoints, InputTolerance);
}

void FNodalOffsetTable::RemoveZoomPoint(float InFocus, float InZoom)
{
	LensDataTableUtils::RemoveZoomPoint(FocusPoints, InFocus, InZoom);
}

bool FNodalOffsetTable::HasZoomPoint(float InFocus, float InZoom, float InputTolerance)
{
	return DoesZoomPointExists(InFocus, InZoom, InputTolerance);
}

void FNodalOffsetTable::ChangeZoomPoint(float InFocus, float InExistingZoom, float InNewZoom, float InputTolerance)
{
	LensDataTableUtils::ChangeZoomPoint(FocusPoints, InFocus, InExistingZoom, InNewZoom, InputTolerance);
}

bool FNodalOffsetTable::DoesFocusPointExists(float InFocus, float InputTolerance) const
{
	if (GetFocusPoint(InFocus, InputTolerance) != nullptr)
	{
		return true;
	}

	return false;
}

bool FNodalOffsetTable::AddPoint(float InFocus, float InZoom, const FNodalPointOffset& InData, float InputTolerance, bool bIsCalibrationPoint)
{
	return LensDataTableUtils::AddPoint(FocusPoints, InFocus, InZoom, InData, InputTolerance, bIsCalibrationPoint);
}

bool FNodalOffsetTable::GetPoint(const float InFocus, const float InZoom, FNodalPointOffset& OutData, float InputTolerance) const
{
	if (const FNodalOffsetFocusPoint* NodalOffsetFocusPoint = GetFocusPoint(InFocus, InputTolerance))
	{
		FNodalPointOffset NodalPointOffset;
		if (NodalOffsetFocusPoint->GetPoint(InZoom, NodalPointOffset, InputTolerance))
		{
			// Copy struct to outer
			OutData = NodalPointOffset;
			return true;
		}
	}
	
	return false;
}

bool FNodalOffsetTable::SetPoint(float InFocus, float InZoom, const FNodalPointOffset& InData, float InputTolerance)
{
	return LensDataTableUtils::SetPoint(*this, InFocus, InZoom, InData, InputTolerance);
}

