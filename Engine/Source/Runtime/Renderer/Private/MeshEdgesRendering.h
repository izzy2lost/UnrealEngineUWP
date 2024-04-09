// Copyright Epic Games, Inc. All Rights Reserved.

class FRDGBuilder;
class FSceneView;
class FViewInfo;
struct FCompositePrimitiveInputs;
struct FScreenPassRenderTarget;

struct FMeshEdgesViewSettings
{
	float Opacity = 1.0;
};

const FMeshEdgesViewSettings& GetMeshEdgesViewSettings(const FSceneView& View);
FMeshEdgesViewSettings& GetMeshEdgesViewSettings(FSceneView& View);

void ComposeMeshEdges(FRDGBuilder& GraphBuilder,
						const FViewInfo& View,
						FScreenPassRenderTarget& EditorPrimitivesColor,
						FScreenPassRenderTarget& EditorPrimitivesDepth);
