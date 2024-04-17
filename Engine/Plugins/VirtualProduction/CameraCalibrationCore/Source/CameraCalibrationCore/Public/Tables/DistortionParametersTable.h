// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/RichCurve.h"
#include "LensData.h"
#include "Tables/BaseLensTable.h"

#include "DistortionParametersTable.generated.h"


/**
 * Distortion parameters associated to a zoom value
 */
USTRUCT()
struct CAMERACALIBRATIONCORE_API FDistortionZoomPoint
{
	GENERATED_BODY()

public:

	/** Input zoom value for this point */
	UPROPERTY()
	float Zoom = 0.0f;

	/** Distortion parameters for this point */
	UPROPERTY(EditAnywhere, Category = "Distortion")
	FDistortionInfo DistortionInfo;
};

/**
 * Contains list of distortion parameters points associated to zoom value
 */
USTRUCT()
struct CAMERACALIBRATIONCORE_API FDistortionFocusPoint : public FBaseFocusPoint
{
	GENERATED_BODY()

	using PointType = FDistortionInfo;
	
public:
	//~ Begin FBaseFocusPoint Interface
	virtual float GetFocus() const override { return Focus; }
	virtual int32 GetNumPoints() const override;
	virtual float GetZoom(int32 Index) const override;
	//~ End FBaseFocusPoint Interface

	void RemovePoint(float InZoomValue);

	/** Returns data type copy value for a given float */
	bool GetPoint(float InZoom, FDistortionInfo& OutData, float InputTolerance = KINDA_SMALL_NUMBER) const;

	/** Adds a new point at InZoom. Updates existing one if tolerance is met */
	bool AddPoint(float InZoom, const FDistortionInfo& InData, float InputTolerance, bool bIsCalibrationPoint);

	/** Sets an existing point at InZoom. Updates existing one if tolerance is met */
	bool SetPoint(float InZoom, const FDistortionInfo& InData, float InputTolerance = KINDA_SMALL_NUMBER);
	
	/** Gets whether the point at InZoom is a calibration point. */
	bool IsCalibrationPoint(float InZoom, float InputTolerance = KINDA_SMALL_NUMBER) { return false; }
	
	/** Returns true if this point is empty */
	bool IsEmpty() const;
	
	void SetParameterValue(int32 InZoomIndex, float InZoomValue, int32 InParameterIndex, float InParameterValue);

public:

	/** Input focus value for this point */
	UPROPERTY()
	float Focus = 0.0f;

	/** Curves describing desired blending between resulting displacement maps */
	UPROPERTY()
	FRichCurve MapBlendingCurve;

	/** List of zoom points */
	UPROPERTY()
	TArray<FDistortionZoomPoint> ZoomPoints;
};

/**
 * Distortion table containing list of points for each focus and zoom input
 */
USTRUCT()
struct CAMERACALIBRATIONCORE_API FDistortionTable : public FBaseLensTable
{
	GENERATED_BODY()

	using FocusPointType = FDistortionFocusPoint;

	/** Wrapper for indices of specific parameters for the distortion table  */
	struct FParameters
	{
		static constexpr int32 Aggregate = INDEX_NONE;
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
	virtual bool CanEditCurveKeyPositions(int32 InParameterIndex) const override;
	virtual bool CanEditCurveKeyAttributes(int32 InParameterIndex) const override;
	virtual FText GetParameterValueLabel(int32 InParameterIndex) const override;
	//~ End FBaseDataTable Interface

	/** Returns const point for a given focus */
	const FDistortionFocusPoint* GetFocusPoint(float InFocus, float InputTolerance = KINDA_SMALL_NUMBER) const;
	
	/** Returns point for a given focus */
	FDistortionFocusPoint* GetFocusPoint(float InFocus, float InputTolerance = KINDA_SMALL_NUMBER);

	/** Returns all focus points */
	TConstArrayView<FDistortionFocusPoint> GetFocusPoints() const;

	/** Returns all focus points */
	TArray<FDistortionFocusPoint>& GetFocusPoints();
	
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
	bool AddPoint(float InFocus, float InZoom, const FDistortionInfo& InData, float InputTolerance, bool bIsCalibrationPoint);

	/** Get the point from the table */
	bool GetPoint(const float InFocus, const float InZoom, FDistortionInfo& OutData, float InputTolerance = KINDA_SMALL_NUMBER) const;

	/** Set a new point into the table */
	bool SetPoint(float InFocus, float InZoom, const FDistortionInfo& InData, float InputTolerance = KINDA_SMALL_NUMBER);

public:

	/** Lists of focus points */
	UPROPERTY()
	TArray<FDistortionFocusPoint> FocusPoints;
};

