// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tables/ImageCenterTable.h"

#include "LensFile.h"
#include "LensTableUtils.h"

int32 FImageCenterFocusPoint::GetNumPoints() const
{
	return Cx.GetNumKeys();
}

float FImageCenterFocusPoint::GetZoom(int32 Index) const
{
	return Cx.Keys[Index].Time;
}

bool FImageCenterFocusPoint::GetPoint(float InZoom, FImageCenterInfo& OutData, float InputTolerance) const
{
	const FKeyHandle CxHandle = Cx.FindKey(InZoom, InputTolerance);
	if (CxHandle != FKeyHandle::Invalid())
	{
		const FKeyHandle CyHandle = Cy.FindKey(InZoom, InputTolerance);
		check(CyHandle != FKeyHandle::Invalid());

		OutData.PrincipalPoint.X = Cx.GetKeyValue(CxHandle);
		OutData.PrincipalPoint.Y = Cy.GetKeyValue(CyHandle);

		return true;
	}

	
	return false;
}

bool FImageCenterFocusPoint::AddPoint(float InZoom, const FImageCenterInfo& InData, float InputTolerance, bool /*bIsCalibrationPoint*/)
{
	if (SetPoint(InZoom, InData, InputTolerance))
	{
		return true;
	}
	
	//Add new zoom point
	const FKeyHandle NewKeyHandle = Cx.AddKey(InZoom, InData.PrincipalPoint.X);
	Cx.SetKeyTangentMode(NewKeyHandle, ERichCurveTangentMode::RCTM_Auto);
	Cx.SetKeyInterpMode(NewKeyHandle, RCIM_Cubic);

	Cy.AddKey(InZoom, InData.PrincipalPoint.Y, false, NewKeyHandle);
	Cy.SetKeyTangentMode(NewKeyHandle, ERichCurveTangentMode::RCTM_Auto);
	Cy.SetKeyInterpMode(NewKeyHandle, RCIM_Cubic);

	// The function return true all the time
	return true;
}

bool FImageCenterFocusPoint::SetPoint(float InZoom, const FImageCenterInfo& InData, float InputTolerance)
{
	const FKeyHandle CxHandle = Cx.FindKey(InZoom, InputTolerance);
	if (CxHandle != FKeyHandle::Invalid())
	{
		const FKeyHandle CyHandle = Cy.FindKey(InZoom, InputTolerance);
		check(CyHandle != FKeyHandle::Invalid());
		Cx.SetKeyValue(CxHandle, InData.PrincipalPoint.X);
		Cy.SetKeyValue(CyHandle, InData.PrincipalPoint.Y);

		return true;
	}

	return false;
}

void FImageCenterFocusPoint::RemovePoint(float InZoomValue)
{
	const FKeyHandle CxKeyHandle = Cx.FindKey(InZoomValue);
	if(CxKeyHandle != FKeyHandle::Invalid())
	{
		Cx.DeleteKey(CxKeyHandle);
	}

	const FKeyHandle CyKeyHandle = Cy.FindKey(InZoomValue);
	if (CyKeyHandle != FKeyHandle::Invalid())
	{
		Cy.DeleteKey(CyKeyHandle);
	}

}

bool FImageCenterFocusPoint::IsEmpty() const
{
	return Cx.IsEmpty();
}

void FImageCenterTable::ForEachPoint(FFocusPointCallback InCallback) const
{
	for (const FImageCenterFocusPoint& Point : FocusPoints)
	{
		InCallback(Point);
	}
}

bool FImageCenterTable::DoesFocusPointExists(float InFocus, float InputTolerance) const
{
	if (GetFocusPoint(InFocus, InputTolerance) != nullptr)
	{
		return true;
	}

	return false;
}

int32 FImageCenterTable::GetTotalPointNum() const
{
	return LensDataTableUtils::GetTotalPointNum(FocusPoints);
}

UScriptStruct* FImageCenterTable::GetScriptStruct() const
{
	return StaticStruct();
}

bool FImageCenterTable::BuildParameterCurveAtFocus(float InFocus, int32 ParameterIndex, FRichCurve& OutCurve) const
{
	if (!FParameters::IsValid(ParameterIndex))
	{
		return false;
	}
	
	if (const FImageCenterFocusPoint* FocusPoint = GetFocusPoint(InFocus))
	{
		if (ParameterIndex == FParameters::Cx)
		{
			OutCurve = FocusPoint->Cx;
		}
		else
		{
			OutCurve = FocusPoint->Cy;
		}
		
		return true;
	}

	return false;
}

bool FImageCenterTable::BuildParameterCurveAtZoom(float InZoom, int32 InParameterIndex, FRichCurve& OutCurve) const
{
	if (!FParameters::IsValid(InParameterIndex))
	{
		return false;
	}
	
	for (const FImageCenterFocusPoint& FocusPoint : FocusPoints)
	{
		FImageCenterInfo ZoomPoint;
		if (FocusPoint.GetPoint(InZoom, ZoomPoint))
		{
			const float Value = ZoomPoint.PrincipalPoint[InParameterIndex];
			const FKeyHandle NewKeyHandle = OutCurve.AddKey(FocusPoint.Focus, Value);
			FRichCurveKey& NewKey = OutCurve.GetKey(NewKeyHandle);
			NewKey.TangentMode = ERichCurveTangentMode::RCTM_None;
			NewKey.InterpMode = ERichCurveInterpMode::RCIM_Linear;
		}
	}

	return true;
}

void FImageCenterTable::SetParameterCurveKeysAtFocus(float InFocus, int32 InParameterIndex, const FRichCurve& InSourceCurve, TArrayView<const FKeyHandle> InKeys)
{
	if (!FParameters::IsValid(InParameterIndex))
	{
		return;
	}
	
	if (FImageCenterFocusPoint* FocusPoint = GetFocusPoint(InFocus))
	{
		FRichCurve* ActiveCurve = nullptr;
		if (InParameterIndex == FParameters::Cx)
		{
			ActiveCurve = &FocusPoint->Cx;
		}
		else
		{
			ActiveCurve = &FocusPoint->Cy;
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

void FImageCenterTable::SetParameterCurveKeysAtZoom(float InZoom, int32 InParameterIndex, const FRichCurve& InSourceCurve, TArrayView<const FKeyHandle> InKeys)
{
	if (!FParameters::IsValid(InParameterIndex))
	{
		return;
	}
	
	for (const FKeyHandle& KeyHandle : InKeys)
	{
		// Assume the focus keys are put into the source curve in the same order as they are stored internally
		const int32 KeyIndex = InSourceCurve.GetIndexSafe(KeyHandle);
		if (KeyIndex != INDEX_NONE)
		{
			if (ensure(FocusPoints.IsValidIndex(KeyIndex)))
			{
				FImageCenterFocusPoint& FocusPoint = FocusPoints[KeyIndex];
				FImageCenterInfo ZoomPoint;
				if (!FocusPoint.GetPoint(InZoom, ZoomPoint))
				{
					continue;
				}

				ZoomPoint.PrincipalPoint[InParameterIndex] = InSourceCurve.GetKeyValue(KeyHandle);
				FocusPoint.SetPoint(InZoom, ZoomPoint);

				if (InParameterIndex == FParameters::Cx)
				{
					FocusPoint.Cx.AutoSetTangents();
				}
				else
				{
					FocusPoint.Cy.AutoSetTangents();
				}
			}
		}
	}
}

FText FImageCenterTable::GetParameterValueLabel(int32 InParameterIndex) const
{
	return NSLOCTEXT("FImageCenterTable", "ParameterValueLabel", "(normalized)");
}

const FImageCenterFocusPoint* FImageCenterTable::GetFocusPoint(float InFocus, float InputTolerance) const
{
	return FocusPoints.FindByPredicate([InFocus, InputTolerance](const FImageCenterFocusPoint& Points) { return FMath::IsNearlyEqual(Points.Focus, InFocus, InputTolerance); });
}

FImageCenterFocusPoint* FImageCenterTable::GetFocusPoint(float InFocus, float InputTolerance)
{
	return FocusPoints.FindByPredicate([InFocus, InputTolerance](const FImageCenterFocusPoint& Points) { return FMath::IsNearlyEqual(Points.Focus, InFocus, InputTolerance); });
}

TConstArrayView<FImageCenterFocusPoint> FImageCenterTable::GetFocusPoints() const
{
	return FocusPoints;
}

TArray<FImageCenterFocusPoint>& FImageCenterTable::GetFocusPoints()
{
	return FocusPoints;
}

bool FImageCenterTable::DoesZoomPointExists(float InFocus, float InZoom, float InputTolerance) const
{
	FImageCenterInfo ImageCenterInfo;
	if (GetPoint(InFocus, InZoom, ImageCenterInfo, InputTolerance))
	{
		return true;
	}

	return false;
}

const FBaseFocusPoint* FImageCenterTable::GetBaseFocusPoint(int32 InIndex) const
{
	if (FocusPoints.IsValidIndex(InIndex))
	{
		return &FocusPoints[InIndex];
	}

	return nullptr;
}

TMap<ELensDataCategory, FLinkPointMetadata> FImageCenterTable::GetLinkedCategories() const
{
	static TMap<ELensDataCategory, FLinkPointMetadata> LinkedToCategories =
	{
		{ELensDataCategory::Distortion, {true}},
		{ELensDataCategory::Zoom, {true}},
		{ELensDataCategory::STMap, {true}},
		{ELensDataCategory::NodalOffset, {false}},
	};
	return LinkedToCategories;
}

void FImageCenterTable::RemoveFocusPoint(float InFocus)
{
	LensDataTableUtils::RemoveFocusPoint(FocusPoints, InFocus);
}

bool FImageCenterTable::HasFocusPoint(float InFocus, float InputTolerance) const
{
	return DoesFocusPointExists(InFocus, InputTolerance);
}

void FImageCenterTable::ChangeFocusPoint(float InExistingFocus, float InNewFocus, float InputTolerance)
{
	LensDataTableUtils::ChangeFocusPoint(FocusPoints, InExistingFocus, InNewFocus, InputTolerance);
}

void FImageCenterTable::MergeFocusPoint(float InSrcFocus, float InDestFocus, bool bReplaceExistingZoomPoints, float InputTolerance)
{
	LensDataTableUtils::MergeFocusPoint(FocusPoints, InSrcFocus, InDestFocus, bReplaceExistingZoomPoints, InputTolerance);
}

void FImageCenterTable::RemoveZoomPoint(float InFocus, float InZoom)
{
	LensDataTableUtils::RemoveZoomPoint(FocusPoints, InFocus, InZoom);
}

bool FImageCenterTable::HasZoomPoint(float InFocus, float InZoom, float InputTolerance)
{
	return DoesZoomPointExists(InFocus, InZoom, InputTolerance);
}

void FImageCenterTable::ChangeZoomPoint(float InFocus, float InExistingZoom, float InNewZoom, float InputTolerance)
{
	LensDataTableUtils::ChangeZoomPoint(FocusPoints, InFocus, InExistingZoom, InNewZoom, InputTolerance);
}

bool FImageCenterTable::AddPoint(float InFocus, float InZoom, const FImageCenterInfo& InData, float InputTolerance,
	bool bIsCalibrationPoint)
{
	return LensDataTableUtils::AddPoint(FocusPoints, InFocus, InZoom, InData, InputTolerance, bIsCalibrationPoint);
}

bool FImageCenterTable::GetPoint(const float InFocus, const float InZoom, FImageCenterInfo& OutData, float InputTolerance) const
{
	if (const FImageCenterFocusPoint* FocalLengthFocusPoint = GetFocusPoint(InFocus, InputTolerance))
	{
		FImageCenterInfo ImageCenterInfo;
		if (FocalLengthFocusPoint->GetPoint(InZoom, ImageCenterInfo, InputTolerance))
		{
			// Copy struct to outer
			OutData = ImageCenterInfo;
			return true;
		}
	}
	
	return false;
}

bool FImageCenterTable::SetPoint(float InFocus, float InZoom, const FImageCenterInfo& InData, float InputTolerance)
{
	return LensDataTableUtils::SetPoint(*this, InFocus, InZoom, InData, InputTolerance);
}
