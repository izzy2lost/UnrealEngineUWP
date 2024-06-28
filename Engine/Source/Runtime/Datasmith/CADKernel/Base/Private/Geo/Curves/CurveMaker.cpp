// Copyright Epic Games, Inc. All Rights Reserved.

#include "Geo/Curves/BezierCurve.h"
#include "Geo/Curves/Curve.h"
#include "Geo/Curves/NURBSCurve.h"
#include "Geo/Curves/SplineCurve.h"

namespace UE::CADKernel
{

TSharedPtr<FCurve> FCurve::MakeNurbsCurve(FNurbsCurveData& InNurbsData)
{
	if (FBezierCurve::IsBezier(InNurbsData))
	{
		return FEntity::MakeShared<UE::CADKernel::FBezierCurve>(InNurbsData);
	}
	return FEntity::MakeShared<UE::CADKernel::FNURBSCurve>(InNurbsData);
}

TSharedPtr<FCurve> FCurve::MakeBezierCurve(const TArray<FPoint>& InPoles)
{
	return FEntity::MakeShared<UE::CADKernel::FBezierCurve>(InPoles);
}

TSharedPtr<FCurve> FCurve::MakeSplineCurve(const TArray<FPoint>& InPoles)
{
	return FEntity::MakeShared<UE::CADKernel::FSplineCurve>(InPoles);
}

TSharedPtr<FCurve> FCurve::MakeSplineCurve(const TArray<FPoint>& InPoles, const TArray<FPoint>& InTangents)
{
	return FEntity::MakeShared<UE::CADKernel::FSplineCurve>(InPoles, InTangents);
}

TSharedPtr<FCurve> FCurve::MakeSplineCurve(const TArray<FPoint>& InPoles, const TArray<FPoint>& InArriveTangents, const TArray<FPoint>& InLeaveTangents)
{
	return FEntity::MakeShared<UE::CADKernel::FSplineCurve>(InPoles, InArriveTangents, InLeaveTangents);
}

}