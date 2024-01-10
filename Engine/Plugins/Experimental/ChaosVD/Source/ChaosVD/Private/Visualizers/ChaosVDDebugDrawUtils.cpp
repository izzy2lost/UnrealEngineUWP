// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visualizers/ChaosVDDebugDrawUtils.h"

#include "CanvasItem.h"
#include "ChaosVDEditorSettings.h"
#include "Engine/Engine.h"
#include "SceneView.h"

TQueue<FChaosVDDebugDrawUtils::FChaosVDQueuedTextToDraw> FChaosVDDebugDrawUtils::TexToDrawQueue = TQueue<FChaosVDQueuedTextToDraw>();

void FChaosVDDebugDrawUtils::DrawArrowVector(FPrimitiveDrawInterface* PDI, const FVector& StartLocation, const FVector& EndLocation, FStringView DebugText, const FColor& Color, ESceneDepthPriorityGroup DepthPriority)
{
	if (!PDI)
	{
		return;
	}

	const FVector LineVectorToDraw = EndLocation - StartLocation;

	FVector ArrowDir;
	float ArrowLength;
	LineVectorToDraw.ToDirectionAndLength(ArrowDir, ArrowLength);

	FVector YAxis, ZAxis;
	ArrowDir.FindBestAxisVectors(YAxis,ZAxis);
	const FMatrix ArrowTransformMatrix(ArrowDir, YAxis, ZAxis,StartLocation);

	constexpr float MinTipOfArrowSize = 0.2f;
	constexpr float MaxTipOfArrowSize = 10.0f;
	constexpr float MaxVectorSizeForArrow = 100.0f; // The vector size that is the upper limit after which we just use the max size for the tip of the arrow

	const float ProportionalArrowSize = MaxTipOfArrowSize * (ArrowLength / MaxVectorSizeForArrow);
	const float ArrowSize = FMath::Clamp(ProportionalArrowSize, MinTipOfArrowSize, MaxTipOfArrowSize);
	
	DrawDirectionalArrow(PDI, ArrowTransformMatrix, Color, ArrowLength, ArrowSize, DepthPriority);

	if (!DebugText.IsEmpty())
	{
		// Draw the text in the middle of the vector line
		const FVector TextWorldPosition = StartLocation + LineVectorToDraw  * 0.5f;
		DrawText(DebugText.GetData(), TextWorldPosition , Color);
	}
}

void FChaosVDDebugDrawUtils::DrawPoint(FPrimitiveDrawInterface* PDI, const FVector& Location, FStringView DebugText, const FColor& Color, float Size, ESceneDepthPriorityGroup DepthPriority)
{
	if (!PDI)
	{
		return;
	}

	if (DebugText.IsEmpty())
	{
		return;
	}

	PDI->DrawPoint(Location, Color, Size, DepthPriority);

	if (!DebugText.IsEmpty())
	{
		DrawText(DebugText, Location, Color);
	}
}

void FChaosVDDebugDrawUtils::DrawText(FStringView StringToDraw, const FVector& Location, const FColor& Color)
{
	if (const UChaosVDEditorSettings* CVDEditorSettings = GetDefault<UChaosVDEditorSettings>())
	{
		if (CVDEditorSettings->bShowDebugText)
		{
			TexToDrawQueue.Enqueue({StringToDraw.GetData(), Location, Color });
		}
	}
}

void FChaosVDDebugDrawUtils::DrawCircle(FPrimitiveDrawInterface* PDI, const FVector& Origin, float Radius, int32 Segments, const FColor& Color, float Thickness, const FVector& XAxis, const FVector& YAxis, FStringView DebugText, ESceneDepthPriorityGroup DepthPriority)
{
	if (!PDI)
	{
		return;
	}

	constexpr float DepthBias = 0;
	const bool bScreenSpace = Thickness > 0;

	// Need at least 2 sides
	Segments = FMath::Max(Segments, 2);
	const float	AngleDelta = 2.0f * UE_PI / Segments;
	FVector	LastVertex = Origin + XAxis * Radius;

	for (int32 SideIndex = 0; SideIndex < Segments; SideIndex++)
	{
		const FVector Vertex = Origin + (XAxis * FMath::Cos(AngleDelta * (SideIndex + 1)) + YAxis * FMath::Sin(AngleDelta * (SideIndex + 1))) * Radius;

		PDI->DrawLine(LastVertex, Vertex, Color, DepthPriority, Thickness, DepthBias, bScreenSpace);

		LastVertex = Vertex;
	}
	
	if (!DebugText.IsEmpty())
	{
		DrawText(DebugText, Origin, Color);
	}
}

void FChaosVDDebugDrawUtils::DrawBox(FPrimitiveDrawInterface* PDI, const FVector& InExtents, const FColor& InColor, const FTransform& InTransform, FStringView DebugText, ESceneDepthPriorityGroup DepthPriority)
{
	if (!PDI)
	{
		return;
	}

	constexpr int32 MaxBoxLines = 12;

	// Array for direction offsets for the start/end point of each line
	static TPair<FVector, FVector> VertexOffsetDirectionFromOrigin[MaxBoxLines] =
	{
		{FVector(1,1,1), FVector(1,-1,1)},
		{FVector(1,-1,1) ,FVector(-1,-1,1)},
		{FVector(-1,-1,1), FVector(-1,1,1)},
		{FVector(-1,1,1), FVector(1,1,1)},
		{FVector(1,1,-1), FVector(1,-1,-1)},
		{FVector(1, -1,-1), FVector(-1,-1,-1)},
		{FVector(-1,-1,-1), FVector(-1,1,-1)},
		{FVector(-1,1,-1), FVector(1,1,-1)},
		{FVector(1,1,1), FVector(1,1,-1)},
		{FVector(1,-1,1), FVector(1,-1,-1)},
		{FVector(-1,-1,1), FVector(-1,-1,-1)},
		{FVector(-1, 1,1), FVector(-1,1,-1)},
	};

	constexpr float Thickness = 2.0f;
	constexpr float DepthBias = 0;
	constexpr bool bScreenSpace = Thickness > 0;

	for (int32 BoxLineIndex = 0; BoxLineIndex < MaxBoxLines; BoxLineIndex++)
	{
		FVector LineStart = InTransform.TransformPosition(InExtents * VertexOffsetDirectionFromOrigin[BoxLineIndex].Key);
		FVector LineEnd = InTransform.TransformPosition(InExtents * VertexOffsetDirectionFromOrigin[BoxLineIndex].Value);

		PDI->DrawLine(LineStart, LineEnd, InColor, DepthPriority, Thickness,DepthBias, bScreenSpace);
	}
	
	if (!DebugText.IsEmpty())
	{
		DrawText(DebugText, InTransform.GetLocation(), InColor);
	}
}

void FChaosVDDebugDrawUtils::DrawLine(FPrimitiveDrawInterface* PDI, const FVector& InStartPosition, const FVector& InEndPosition, const FColor& InColor, FStringView DebugText, ESceneDepthPriorityGroup DepthPriority)
{
	if (!PDI)
	{
		return;
	}

	constexpr float Thickness = 2.0f;
	constexpr float DepthBias = 0;
	constexpr bool bScreenSpace = Thickness > 0;

	PDI->DrawLine(InStartPosition, InEndPosition, InColor, DepthPriority, Thickness, DepthBias, bScreenSpace);

	if (!DebugText.IsEmpty())
	{
		// Draw the text in the middle of the line
		const FVector TextWorldPosition = InStartPosition + ((InEndPosition - InStartPosition)  * 0.5f);
		DrawText(DebugText, TextWorldPosition, InColor);
	}
}

void FChaosVDDebugDrawUtils::DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas)
{
	if (!GEngine)
	{
		return;
	}

	while (!TexToDrawQueue.IsEmpty())
	{
		FChaosVDQueuedTextToDraw TextToDraw;
		if (TexToDrawQueue.Dequeue(TextToDraw))
		{
			FVector2D PixelLocation;
			if (View.WorldToPixel(TextToDraw.WorldPosition, PixelLocation))
			{
				FCanvasTextItem TextItem(PixelLocation, FText::AsCultureInvariant(TextToDraw.Text), GEngine->GetSmallFont(), TextToDraw.Color);
				TextItem.Scale = FVector2D::UnitVector;
				TextItem.EnableShadow(FLinearColor::Black);
				TextItem.Draw(&Canvas);
			}
		}
	}
}
