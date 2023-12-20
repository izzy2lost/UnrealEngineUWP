// Copyright Epic Games, Inc. All Rights Reserved.

#include "Expressions/Procedural/TG_Expression_Shape.h"
#include "FxMat/MaterialManager.h"
#include "Transform/Mask/T_ShapeMask.h"

void UTG_Expression_Shape::Evaluate(FTG_EvaluationContext* InContext)
{
	Super::Evaluate(InContext);

	auto DesiredDescriptor = Output.Descriptor;

	/// If it's auto then make it grey scale as we don't need more than a single channel output
	if (DesiredDescriptor.TextureFormat == ETG_TextureFormat::Auto)
	{
		DesiredDescriptor.TextureFormat = ETG_TextureFormat::G8;
	}

	T_ShapeMask::FParams Params{
		.Rotation = Orientation * (PI / 180.0f),
		.Size = { Width, Height },
		.Rounding = Rounding,
		.BevelWidth = BevelWidth,
		.BevelCurve = BevelCurve,
		.BlendSDF = ShowSDF
	};
	
	int ShapeTypeInt = (int) ShapeType;
	EShapeMaskType ShaderShapeType = (EShapeMaskType)ShapeTypeInt;
	if (ShapeType == EShapeType::Polygon)
		ShaderShapeType = (EShapeMaskType)PolygonNumSides;

	Output = T_ShapeMask::Create(InContext->Cycle, DesiredDescriptor, ShaderShapeType, Params, InContext->TargetId);
}
