// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/ImplicitFwd.h"
#include "Containers/Queue.h"
#include "SceneManagement.h"

class FChaosVDGeometryBuilder;

/** Utility methods that allows Debug draw into the Chaos VD Editor */
class FChaosVDDebugDrawUtils
{
public:
	//TODO: Create a common args struct that can be passed as parameter to all debug draw methods

	static void DrawArrowVector(FPrimitiveDrawInterface* PDI, const FVector& StartLocation, const FVector& EndLocation, FStringView DebugText, const FColor& Color, ESceneDepthPriorityGroup DepthPriority = SDPG_World);
	static void DrawPoint(FPrimitiveDrawInterface* PDI, const FVector& Location, FStringView DebugText, const FColor& Color, float Size = 10.0f, ESceneDepthPriorityGroup DepthPriority = SDPG_World);
	static void DrawText(FStringView StringToDraw, const FVector& Location, const FColor& Color);
	static void DrawCircle(FPrimitiveDrawInterface* PDI, const FVector& Origin, float Radius, int32 Segments, const FColor& Color, float Thickness, const FVector& XAxis, const FVector& YAxis, FStringView DebugText, ESceneDepthPriorityGroup DepthPriority = SDPG_World);
	static void DrawBox(FPrimitiveDrawInterface* PDI, const FVector& InExtents, const FColor& InColor, const FTransform& InTransform, FStringView DebugText, ESceneDepthPriorityGroup DepthPriority = SDPG_World);
	static void DrawLine(FPrimitiveDrawInterface* PDI, const FVector& InStartPosition, const FVector& InEndPosition, const FColor& InColor, FStringView DebugText, ESceneDepthPriorityGroup DepthPriority = SDPG_World);
	static void DrawImplicitObject(FPrimitiveDrawInterface* PDI, const TSharedPtr<FChaosVDGeometryBuilder>& GeometryGenerator, const Chaos::FConstImplicitObjectPtr& ImplicitObject, const FTransform& InWorldTransform, const FColor& InColor, FStringView DebugText, ESceneDepthPriorityGroup DepthPriority = SDPG_World);

	static void DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas);

private:
	struct FChaosVDQueuedTextToDraw
	{
		FString Text;
		FVector WorldPosition;
		FLinearColor Color;
	};

	static TQueue<FChaosVDQueuedTextToDraw> TexToDrawQueue;
};
