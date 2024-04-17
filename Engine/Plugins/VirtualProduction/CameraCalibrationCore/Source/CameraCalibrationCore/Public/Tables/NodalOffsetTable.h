// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/RichCurve.h"
#include "LensData.h"
#include "Tables/BaseLensTable.h"

#include "NodalOffsetTable.generated.h"


/**
 * Focus point for nodal offset curves
 */
USTRUCT()
struct CAMERACALIBRATIONCORE_API FNodalOffsetFocusPoint : public FBaseFocusPoint
{
	GENERATED_BODY()

	using PointType = FNodalPointOffset;
	
public:
	//~ Begin FBaseFocusPoint Interface
	virtual float GetFocus() const override { return Focus; }
	virtual int32 GetNumPoints() const override;
	virtual float GetZoom(int32 Index) const override;
	//~ End FBaseFocusPoint Interface

	/** Returns data type copy value for a given float */
	bool GetPoint(float InZoom, FNodalPointOffset& OutData, float InputTolerance = KINDA_SMALL_NUMBER) const;

	/** Adds a new point at InZoom. Updates existing one if tolerance is met */
	bool AddPoint(float InZoom, const FNodalPointOffset& InData, float InputTolerance, bool bIsCalibrationPoint);

	/** Sets an existing point at InZoom. Updates existing one if tolerance is met */
	bool SetPoint(float InZoom, const FNodalPointOffset& InData, float InputTolerance = KINDA_SMALL_NUMBER);
	
	/** Gets whether the point at InZoom is a calibration point. */
	bool IsCalibrationPoint(float InZoom, float InputTolerance = KINDA_SMALL_NUMBER) { return false; }
	
	/** Removes a point corresponding to specified zoom */
	void RemovePoint(float InZoomValue);

	/** Returns true if there are no points */
	bool IsEmpty() const;

public:

	/** Dimensions of our location offset curves */
	static constexpr uint32 LocationDimension = 3;
	
	/** Dimensions of our rotation offset curves */
	static constexpr uint32 RotationDimension = 3;

	/** Input focus for this point */
	UPROPERTY()
	float Focus = 0.0f;

	/** XYZ offsets curves mapped to zoom */
	UPROPERTY()
	FRichCurve LocationOffset[LocationDimension];

	/** Yaw, Pitch and Roll offset curves mapped to zoom */
	UPROPERTY()
	FRichCurve RotationOffset[RotationDimension];
};

/**
 * Table containing nodal offset mapping to focus and zoom
 */
USTRUCT()
struct CAMERACALIBRATIONCORE_API FNodalOffsetTable : public FBaseLensTable
{
	GENERATED_BODY()

	using FocusPointType = FNodalOffsetFocusPoint;

	/** Wrapper for indices of specific parameters for the nodal offset table  */
	struct FParameters
	{
		static constexpr int32 Location = 0;
		static constexpr int32 Rotation = 1;

		/** Composes the parameter and axis indices into a single value */
		static int32 Compose(int32 InParameterIndex, EAxis::Type Axis) { return InParameterIndex * 3 + (Axis - 1); }

		/** Composes a combined index into a parameter index and an axis */
		static void Decompose(int32 InComposedIndex, int32& OutParameterIndex, EAxis::Type& OutAxis)
		{
			OutParameterIndex = InComposedIndex / 3;
			OutAxis = (EAxis::Type)(InComposedIndex % 3 + 1);
		}
		
		/** Returns if a composed parameter index is valid */
		static bool IsValidComposed(int32 InComposedIndex) { return InComposedIndex >= 0 && InComposedIndex < 6; }
	};
	
protected:
	//~ Begin FBaseDataTable Interface
	virtual TMap<ELensDataCategory, FLinkPointMetadata> GetLinkedCategories() const override;
	virtual bool DoesFocusPointExists(float InFocus, float InputTolerance = KINDA_SMALL_NUMBER) const override;
	virtual bool DoesZoomPointExists(float InFocus, float InZoom, float InputTolerance = KINDA_SMALL_NUMBER) const override;
	virtual const FBaseFocusPoint* GetBaseFocusPoint(int32 InIndex) const override;
	//~ End FBaseDataTable Interface
	
public:
	//~ Begin FBaseDataTable Interface
	virtual void ForEachPoint(FFocusPointCallback InCallback) const override;
	virtual int32 GetFocusPointNum() const override { return FocusPoints.Num(); }
	virtual int32 GetTotalPointNum() const override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual bool BuildParameterCurveAtFocus(float InFocus, int32 InParameterIndex, FRichCurve& OutCurve) const override;
	virtual bool BuildParameterCurveAtZoom(float InZoom, int32 InParameterIndex, FRichCurve& OutCurve) const override;
	virtual void SetParameterCurveKeysAtFocus(float InFocus, int32 InParameterIndex, const FRichCurve& InSourceCurve, TArrayView<const FKeyHandle> InKeys) override;
	virtual void SetParameterCurveKeysAtZoom(float InZoom, int32 InParameterIndex, const FRichCurve& InSourceCurve, TArrayView<const FKeyHandle> InKeys) override;
	virtual bool CanEditCurveKeyPositions(int32 InParameterIndex) const override { return true; }
	virtual bool CanEditCurveKeyAttributes(int32 InParameterIndex) const override { return true; }
	virtual FText GetParameterValueLabel(int32 InParameterIndex) const override;
	virtual FText GetParameterValueUnitLabel(int32 InParameterIndex) const override;
	//~ End FBaseDataTable Interface

	/** Returns const point for a given focus */
	const FNodalOffsetFocusPoint* GetFocusPoint(float InFocus, float InputTolerance = KINDA_SMALL_NUMBER) const;

	/** Returns point for a given focus */
	FNodalOffsetFocusPoint* GetFocusPoint(float InFocus, float InputTolerance = KINDA_SMALL_NUMBER);

	/** Returns all focus points */
	TConstArrayView<FNodalOffsetFocusPoint> GetFocusPoints() const;

	/** Returns all focus points */
	TArray<FNodalOffsetFocusPoint>& GetFocusPoints();

	/** Removes a focus point identified as InFocusIdentifier */
	void RemoveFocusPoint(float InFocus);

	/** Checks to see if there exists a focus point matching the specified focus value */
	bool HasFocusPoint(float InFocus, float InputTolerance = KINDA_SMALL_NUMBER) const;

	/** Changes the value of a focus point */
	void ChangeFocusPoint(float InExistingFocus, float InNewFocus, float InputTolerance = KINDA_SMALL_NUMBER);

	/** Merges the points in the specified source focus into the specified destination focus */
	void MergeFocusPoint(float InSrcFocus, float InDestFocus, bool bReplaceExistingZoomPoints, float InputTolerance = KINDA_SMALL_NUMBER);
	
	/** Removes a zoom point from a focus point*/
	void RemoveZoomPoint(float InFocus, float InZoom);

	/** Checks to see if there exists a zoom point matching the specified zoom and focus values */
	bool HasZoomPoint(float InFocus, float InZoom, float InputTolerance = KINDA_SMALL_NUMBER);

	/** Changes the value of a zoom point */
	void ChangeZoomPoint(float InFocus, float InExistingZoom, float InNewZoom, float InputTolerance = KINDA_SMALL_NUMBER);
	
	/** Adds a new point in the table */
	bool AddPoint(float InFocus, float InZoom, const FNodalPointOffset& InData,  float InputTolerance, bool bIsCalibrationPoint);

	/** Get the point from the table */
	bool GetPoint(const float InFocus, const float InZoom, FNodalPointOffset& OutData, float InputTolerance = KINDA_SMALL_NUMBER) const;

	/** Set a new point into the table */
	bool SetPoint(float InFocus, float InZoom, const FNodalPointOffset& InData, float InputTolerance = KINDA_SMALL_NUMBER);
	
public:

	/** Lists of focus points */
	UPROPERTY()
	TArray<FNodalOffsetFocusPoint> FocusPoints;
};

