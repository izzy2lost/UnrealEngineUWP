// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelingToolsManagerActions.h"
#include "ModelingToolsEditorModeStyle.h"
#include "ModelingToolsEditorModeSettings.h"
#include "Styling/ISlateStyle.h"

#define LOCTEXT_NAMESPACE "ModelingToolsManagerCommands"



FModelingToolsManagerCommands::FModelingToolsManagerCommands() :
	TCommands<FModelingToolsManagerCommands>(
		"ModelingToolsManagerCommands", // Context name for fast lookup
		NSLOCTEXT("Contexts", "ModelingToolsToolCommands", "Modeling Mode - Tools"), // Localized context name for displaying
		NAME_None, // Parent
		FModelingToolsEditorModeStyle::Get()->GetStyleSetName() // Icon Style Set
		)
{
}



TSharedPtr<FUICommandInfo> FModelingToolsManagerCommands::FindToolByName(FString Name, bool& bFound) const
{
	bFound = false;
	for (const FStartToolCommand& Command : RegisteredTools)
	{
		if (Command.ToolUIName.Equals(Name, ESearchCase::IgnoreCase)
		 || (Command.ToolCommand.IsValid() && Command.ToolCommand->GetLabel().ToString().Equals(Name, ESearchCase::IgnoreCase)))
		{
			bFound = true;
			return Command.ToolCommand;
		}
	}
	return TSharedPtr<FUICommandInfo>();
}

FText FModelingToolsManagerCommands::GetCommandLabel(TSharedPtr<const FUICommandInfo> Command, bool bShortName) const
{
	if (!ensure(Command.IsValid()))
	{
		return FText();
	}
	if (const int32* Index = CommandToRegisteredToolsIndex.Find(Command.Get()))
	{
		if (RegisteredTools.IsValidIndex(*Index) && !RegisteredTools[*Index].ShortName.IsEmpty())
		{
			return RegisteredTools[*Index].ShortName;
		}
	}
	return Command->GetLabel();
}


void FModelingToolsManagerCommands::RegisterCommands()
{
	const UModelingToolsEditorModeSettings* Settings = GetDefault<UModelingToolsEditorModeSettings>();

	// this has to be done with a compile-time macro because UI_COMMAND expands to LOCTEXT macros
#define REGISTER_MODELING_TOOL_COMMAND(ToolCommandInfo, ToolName, ToolTip) \
		UI_COMMAND(ToolCommandInfo, ToolName, ToolTip, EUserInterfaceActionType::ToggleButton, FInputChord()); \
		RegisteredTools.Add(FStartToolCommand{ ToolName, ToolCommandInfo });
	// If using short names, we can't use the same command with a different ToolName, because that will
	//  generate colliding LOCTEXT keys. Instead we keep the short name in a map.
#define REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(ToolCommandInfo, ToolName, ShortName, ToolTip) \
		UI_COMMAND(ToolCommandInfo, ToolName, ToolTip, EUserInterfaceActionType::ToggleButton, FInputChord()); \
		RegisteredTools.Add(FStartToolCommand{ ToolName, ToolCommandInfo, LOCTEXT(ToolName"_short", ShortName) });

	// this has to be done with a compile-time macro because UI_COMMAND expands to LOCTEXT macros
#define REGISTER_MODELING_ACTION_COMMAND(ToolCommandInfo, ToolName, ToolTip) \
		UI_COMMAND(ToolCommandInfo, ToolName, ToolTip, EUserInterfaceActionType::Button, FInputChord()); \
		RegisteredTools.Add(FStartToolCommand{ ToolName, ToolCommandInfo });
#define REGISTER_MODELING_ACTION_COMMAND_WITH_SHORTNAME(ToolCommandInfo, ToolName, ShortName, ToolTip) \
		UI_COMMAND(ToolCommandInfo, ToolName, ToolTip, EUserInterfaceActionType::Button, FInputChord()); \
		RegisteredTools.Add(FStartToolCommand{ ToolName, ToolCommandInfo, LOCTEXT(ToolName"_short", ShortName) });

	// Shapes
	REGISTER_MODELING_TOOL_COMMAND(BeginAddBoxPrimitiveTool, "Box", "Create new box objects");
	REGISTER_MODELING_TOOL_COMMAND(BeginAddSpherePrimitiveTool, "Sphere", "Create new sphere objects");
	REGISTER_MODELING_TOOL_COMMAND(BeginAddConePrimitiveTool, "Cone", "Create new cone objects");
	REGISTER_MODELING_TOOL_COMMAND(BeginAddTorusPrimitiveTool, "Torus", "Create new torus objects");
	REGISTER_MODELING_TOOL_COMMAND(BeginAddArrowPrimitiveTool, "Arrow", "Create new arrow objects");
	REGISTER_MODELING_TOOL_COMMAND(BeginAddDiscPrimitiveTool, "Disc", "Create new disc objects");
	REGISTER_MODELING_TOOL_COMMAND(BeginAddStairsPrimitiveTool, "Stairs", "Create new stairs objects");
	REGISTER_MODELING_TOOL_COMMAND(BeginAddCapsulePrimitiveTool, "Capsule", "Create new capsule objects");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginAddCylinderPrimitiveTool, "Cylinder", "Cyl", "Create new cylinder objects");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginAddRectanglePrimitiveTool, "Rectangle", "Rect", "Create new rectangle objects");


	// Create
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginDrawPolygonTool, "Extrude Polygon", "PolyExt", "Draw and extrude 2D Polygons to create new objects");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginDrawPolyPathTool, "Extrude Path", "PathExt", "Draw and extrude 2D Paths to create new objects");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginDrawAndRevolveTool, "Revolve Path", "PathRev", "Draw and revolve 2D Paths to create new objects");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginRevolveSplineTool, "Revolve Spline", "SplnRev", "Revolve splines to create new objects");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginRevolveBoundaryTool, "Revolve Boundary", "BdryRev", "Revolve Mesh boundary loops to create new objects");
	REGISTER_MODELING_TOOL_COMMAND(BeginCombineMeshesTool, "Merge", "Merge multiple Meshes to create new objects");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginDuplicateMeshesTool, "Duplicate", "Dupe", "Duplicate single Meshes to create new objects");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginDrawSplineTool, "Draw Spline", "DrwSpln", "Draw a spline in the level");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginTriangulateSplinesTool, "Mesh Splines", "MshSpln", "Triangulate the Spline Components of selected actors to create new objects");

	// PolyModel
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginPolyEditTool, "PolyGroup Edit", "PolyEd", "Edit Meshes via PolyGroups");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginPolyDeformTool, "Deform PolyGroups", "PolyDef", "Deform Meshes via PolyGroups");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginCubeGridTool, "CubeGrid", "CubeGr", "Create block out Meshes using a repositionable grid");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginMeshBooleanTool, "Boolean", "MshBool", "Apply Boolean operations to the two selected Meshes");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginCutMeshWithMeshTool, "Mesh Cut", "MshCut", "Split one Mesh into parts using a second Mesh");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginSubdividePolyTool, "Subdivide", "SubDiv", "Subdivide the selected Mesh via PolyGroups or triangles");

	// TriModel
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginMeshSelectionTool, "Tri Select", "TriSel", "Select and edit Mesh triangles with a brush interface");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginTriEditTool, "Triangle Edit", "TriEd", "Select and Edit the Mesh vertices, edges, and triangles");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginHoleFillTool, "Fill Holes", "HFill", "Fill holes in the selected Mesh");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginPlaneCutTool, "Plane Cut", "PlnCut", "Cut the selected Meshes with a 3D plane");
	REGISTER_MODELING_TOOL_COMMAND(BeginMirrorTool, "Mirror", "Mirror the selected Meshes");
	REGISTER_MODELING_TOOL_COMMAND(BeginPolygonCutTool, "PolyCut", "Cut the selected Mesh with an extruded polygon");
	REGISTER_MODELING_TOOL_COMMAND(BeginMeshTrimTool, "Trim", "Trim/Cut the selected Mesh with the second selected Mesh");

	// Deform
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginSculptMeshTool, "Vertex Sculpt", "VSclpt", "Vertex sculpting");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginRemeshSculptMeshTool, "Dynamic Sculpt", "DSclpt", "Dynamic mesh sculpting");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginDisplaceMeshTool, "Displace", "Displce", "Tessellate and Displace the selected Mesh");
	REGISTER_MODELING_TOOL_COMMAND(BeginSmoothMeshTool, "Smooth", "Smooth the shape of the selected Mesh");
	REGISTER_MODELING_TOOL_COMMAND(BeginOffsetMeshTool, "Offset", "Offset the surface of the selected Mesh");
	REGISTER_MODELING_TOOL_COMMAND(BeginMeshSpaceDeformerTool, "Warp", "Reshape the selected Mesh using space deformers");
	REGISTER_MODELING_TOOL_COMMAND(BeginLatticeDeformerTool, "Lattice", "Deform the selected Mesh using a 3D lattice/grid");

	// Transform
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginTransformMeshesTool, "Transform", "XForm", "Transform the selected Meshes");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginEditPivotTool, "Edit Pivot", "Pivot", "Edit the pivot points of the selected Meshes");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginAddPivotActorTool, "Pivot Actor", "PivotAct", "Add actor to act as a pivot for child components");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginBakeTransformTool, "Bake Transform", "BakeRS", "Bake rotation and scale into the selected Meshes");
	REGISTER_MODELING_TOOL_COMMAND(BeginAlignObjectsTool, "Align", "Align the selected Objects");
	REGISTER_MODELING_TOOL_COMMAND(BeginTransferMeshTool, "Transfer", "Copy the first selected Mesh to the second selected Mesh");
	REGISTER_MODELING_TOOL_COMMAND(BeginConvertMeshesTool, "Convert", "Convert the selected Meshes to a different type of Mesh Object");
	REGISTER_MODELING_TOOL_COMMAND(BeginSplitMeshesTool, "Split", "Split the selected Meshes into separate parts based on connectivity, selection, material ID or PolyGroup");
	REGISTER_MODELING_TOOL_COMMAND(BeginPatternTool, "Pattern", "Create patterns of Meshes");
	REGISTER_MODELING_TOOL_COMMAND(BeginHarvestInstancesTool, "Harvest Instances", "Extract a set of InstancedStaticMeshComponents from a set of Actors");
	

	// MeshOps
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginSimplifyMeshTool, "Simplify", "Simplfy", "Simplify the selected Mesh");
	REGISTER_MODELING_TOOL_COMMAND(BeginRemeshMeshTool, "Remesh", "Re-triangulate the selected Mesh");
	REGISTER_MODELING_TOOL_COMMAND(BeginWeldEdgesTool, "Weld", "Weld overlapping Mesh edges");
	REGISTER_MODELING_TOOL_COMMAND(BeginRemoveOccludedTrianglesTool, "Jacket", "Remove hidden triangles from the selected Meshes");
	REGISTER_MODELING_TOOL_COMMAND(BeginSelfUnionTool, "Union", "Boolean Union the selected Meshes, including Self-Union to resolve self-intersections");
	REGISTER_MODELING_TOOL_COMMAND(BeginProjectToTargetTool, "Project", "Map/re-mesh the first selected Mesh onto the second selected Mesh");


	// VoxOps
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginVoxelSolidifyTool, "Voxel Wrap", "VoxWrap", "Wrap the selected Meshes using SDFs/voxels");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginVoxelBlendTool, "Voxel Blend", "VoxBlnd", "Blend the selected Meshes using SDFs/voxels");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginVoxelMorphologyTool, "Voxel Offset", "VoxMrph", "Offset/Inset the selected Meshes using SDFs/voxels");
#if WITH_PROXYLOD
	// The ProxyLOD plugin is currently only available on Windows. Without it, the following tools do not work as expected.
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginVoxelBooleanTool, "Voxel Boolean", "VoxBool", "Boolean the selected Meshes using SDFs/voxels");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginVoxelMergeTool, "Voxel Merge", "VoxMrg", "Merge the selected Meshes using SDFs/voxels");
#endif	// WITH_PROXYLOD

	// Attributes
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginMeshInspectorTool, "Inspect", "Inspct", "Inspect Mesh attributes");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginEditNormalsTool, "Normals", "Nrmls", "Recompute or Repair Normals");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginEditTangentsTool, "Tangents", "Tngnts", "Recompute Tangents");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginPolyGroupsTool, "Generate PolyGroups", "GrpGen", "Generate new PolyGroups");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginMeshGroupPaintTool, "Paint PolyGroups", "GrpPnt", "Paint new PolyGroups");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginMeshVertexPaintTool, "Paint Vertex Colors", "VtxPnt", "Paint Mesh Vertex Colors");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginAttributeEditorTool, "Edit Attributes", "AttrEd", "Edit/configure Mesh attributes");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginMeshAttributePaintTool, "Paint Maps", "MapPnt", "Paint attribute maps");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginEditMeshMaterialsTool, "Edit Materials", "MatEd", "Assign materials to selected triangles");

	// UVs
	REGISTER_MODELING_TOOL_COMMAND(BeginGlobalUVGenerateTool, "AutoUV", "Automatically unwrap and pack UVs");
	// This is done directly, not with the REGISTER_ macro, since we don't want it added to the tool list or use a toggle button
	UI_COMMAND(LaunchUVEditor, "UVEditor", "Launch UV asset editor", EUserInterfaceActionType::Button, FInputChord());
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginGroupUVGenerateTool, "UV Unwrap", "Unwrap", "Recompute UVs for existing UV islands or PolyGroups");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginUVProjectionTool, "Project UVs", "Project", "Compute UVs via projecting to simple shapes");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginUVSeamEditTool, "Edit UV Seams", "SeamEd", "Add UV seams");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginTransformUVIslandsTool, "Transform UVs", "XFormUV", "Transform UV islands in UV space");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginUVLayoutTool, "Layout UVs", "Layout", "Transform and Repack existing UVs");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginUVTransferTool, "Transfer UVs", "Transfer", "Transfer UVs from a simplified mesh to a higher resolution mesh");

	// Baking
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginBakeMeshAttributeMapsTool, "Bake Textures", "BakeTx", "Bake textures for a target Mesh");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginBakeMultiMeshAttributeMapsTool, "Bake All", "BakeAll", "Bake textures for a target Mesh from multiple source Meshes");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginBakeMeshAttributeVertexTool, "Bake Vertex Colors", "BakeVtx", "Bake vertex colors for a target Mesh");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginBakeRenderCaptureTool, "Bake RC", "BakeRC", "Bake renders into new textures for a target Mesh from multiple source Meshes");

	// Volumes
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginVolumeToMeshTool, "Volume To Mesh", "Vol2Msh", "Convert a Volume to a new Mesh Object");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginMeshToVolumeTool, "Mesh To Volume", "Msh2Vol", "Convert a Mesh to a Volume");
	if (!Settings->InRestrictiveMode())
	{
		REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginBspConversionTool, "Convert BSPs", "BSPConv", "Convert BSP to a new Mesh Object");
	}
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginPhysicsInspectorTool, "Inspect Collision", "PInspct", "Inspect the physics/collision geometry for selected Meshes");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginSimpleCollisionEditorTool, "Simple Collision Editor", "SCollEdit", "Edit the simple collision shapes for the selected Mesh");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginSetCollisionGeometryTool, "Mesh To Collision", "Msh2Coll", "Convert selected Meshes to Simple Collision Geometry (for last selected)");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginExtractCollisionGeometryTool, "Collision To Mesh", "Coll2Msh", "Convert Simple Collision Geometry to a new Mesh Object");

	// LODs
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginLODManagerTool, "LOD Manager", "LODMgr", "Inspect the LODs of a Static Mesh Asset");
	REGISTER_MODELING_TOOL_COMMAND(BeginGenerateStaticMeshLODAssetTool, "AutoLOD", "Automatically generate a simplified LOD with baked Textures/Materials for a Mesh");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginISMEditorTool, "ISM Editor", "ISMEd", "Edit the Instances of Instanced Static Mesh Components");

	REGISTER_MODELING_TOOL_COMMAND(BeginAddPatchTool, "Patch", "Add Patch");
	REGISTER_MODELING_TOOL_COMMAND(BeginShapeSprayTool, "Spray", "Shape Spray");
	REGISTER_MODELING_TOOL_COMMAND(BeginEditCollisionGeometryTool, "EditPhys", "Edit Simple Collision Geometry for selected Mesh");
	
	REGISTER_MODELING_TOOL_COMMAND(BeginSkinWeightsPaintTool, "Edit Weights", "Tune the per-vertex skin weights.");
	REGISTER_MODELING_TOOL_COMMAND(BeginSkinWeightsBindingTool, "Bind Skin", "Create default weights by binding the skin to bones.");

	REGISTER_MODELING_TOOL_COMMAND(BeginSkeletonEditingTool, "Edit Skeleton", "Add, Remove, Reparent, Move and Rename bones.");

	REGISTER_MODELING_TOOL_COMMAND(BeginPolyModelTool_Inset, "Inset", "Inset the current Selected Faces");
	REGISTER_MODELING_TOOL_COMMAND(BeginPolyModelTool_Outset, "Outset", "Outset the current Selected Faces");
	REGISTER_MODELING_TOOL_COMMAND(BeginPolyModelTool_CutFaces, "Cut", "Cut the current Selected Faces");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginPolyModelTool_ExtrudeEdges, "Extrude Edges", "ExtEdg", "Extrude selected boundary edges.");
	REGISTER_MODELING_TOOL_COMMAND(BeginPolyModelTool_PushPull, "PushPull", "Push/Pull the current Selected Faces");
	REGISTER_MODELING_TOOL_COMMAND(BeginPolyModelTool_Bevel, "Bevel", "Bevel the current Mesh Selection (Edges or Faces)");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginPolyModelTool_InsertEdgeLoop, "Insert Loops", "ELoop", "Insert Edge Loops into the Selected Mesh");

	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginPolyModelTool_PolyEd, "PolyGroup Edit", "PolyEd", "Select / Edit the current Mesh via PolyGroups");
	REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME(BeginPolyModelTool_TriSel, "Tri Select", "TriSel", "Select / Edit the current Mesh triangles with a brush interface");


	REGISTER_MODELING_TOOL_COMMAND(MeshSelectionModeAction_NoSelection, "None", "Disable Mesh Element Selection (Select Objects Only)");
	REGISTER_MODELING_TOOL_COMMAND(MeshSelectionModeAction_MeshTriangles, "Tris", "Select Mesh Triangles");
	REGISTER_MODELING_TOOL_COMMAND(MeshSelectionModeAction_MeshVertices, "Verts", "Select Mesh Vertices");
	REGISTER_MODELING_TOOL_COMMAND(MeshSelectionModeAction_MeshEdges, "Edges", "Select Mesh Edges");
	REGISTER_MODELING_TOOL_COMMAND(MeshSelectionModeAction_GroupFaces, "Groups", "Select Mesh PolyGroups");
	REGISTER_MODELING_TOOL_COMMAND(MeshSelectionModeAction_GroupCorners, "Corners", "Select Mesh PolyGroup Corners/Vertices");
	REGISTER_MODELING_TOOL_COMMAND(MeshSelectionModeAction_GroupEdges, "Borders", "Select Mesh PolyGroup Borders/Edges");

	REGISTER_MODELING_ACTION_COMMAND(BeginSelectionAction_Delete, "Delete", "Delete the current Mesh Selection");
	REGISTER_MODELING_ACTION_COMMAND(BeginSelectionAction_Retriangulate, "Clean", "Retriangulate the current Mesh or Mesh Selection");
	REGISTER_MODELING_ACTION_COMMAND_WITH_SHORTNAME(BeginSelectionAction_Disconnect, "Disconnect", "Discon", "Disconnect the current Mesh Selection");

	REGISTER_MODELING_TOOL_COMMAND(BeginSelectionAction_Extrude, "Extrude", "Extrude the current Mesh Selection");
	REGISTER_MODELING_TOOL_COMMAND(BeginSelectionAction_Offset, "Offset", "Offset the current Mesh Selection");

	REGISTER_MODELING_ACTION_COMMAND(BeginSelectionAction_SelectAll, "Select All", "Select All Elements");
	REGISTER_MODELING_ACTION_COMMAND(BeginSelectionAction_ExpandToConnected, "Expand To Connected", "Expand Selection to Geometrically-Connected Elements");
	REGISTER_MODELING_ACTION_COMMAND(BeginSelectionAction_Invert, "Invert Selection", "Invert the current Selection");
	REGISTER_MODELING_ACTION_COMMAND(BeginSelectionAction_InvertConnected, "Invert Connected", "Invert the current Selection to Geometrically-Connected Elements");
	REGISTER_MODELING_ACTION_COMMAND(BeginSelectionAction_Expand, "Expand Selection", "Expand the current Selection by a ring of elements");
	REGISTER_MODELING_ACTION_COMMAND(BeginSelectionAction_Contract, "Contract Selection", "Contract the current Selection by a ring of elements");



	UI_COMMAND(AddToFavorites, "Add to Favorites", "Add to Favorites", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RemoveFromFavorites, "Remove from Favorites", "Remove from Favorites", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(LoadFavoritesTools, "Faves", "Favorites", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadSelectionTools, "Select", "Edit Mesh Selections", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadShapesTools, "Shapes", "Shapes", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadCreateTools, "Create", "Create New Shapes", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadPolyTools, "Model", "Shape Modeling", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadTriTools, "Process", "Mesh Processing", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadDeformTools, "Deform", "Deformations", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadTransformTools, "XForm", "Transforms & Conversion", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadMeshOpsTools, "Mesh", "Mesh Processing", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadVoxOpsTools, "Voxel", "Voxel Processing", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadAttributesTools, "Attribs", "Mesh Attributes", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadUVsTools, "UVs", "Create & Edit UVs", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadBakingTools, "Bake", "Bake Textures & Colors", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadVolumeTools, "Volumes", "Volumes", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadLodsTools, "Misc", "Additional Utility Tools", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadSkinTools, "Skin", "Edit Skin Weights", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LoadSkeletonTools, "Skeleton", "Edit Bones", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(AcceptActiveTool, "Accept", "Accept the active Tool", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CancelActiveTool, "Cancel", "Cancel the active Tool", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CompleteActiveTool, "Done", "Complete the active Tool", EUserInterfaceActionType::Button, FInputChord());

	// Note that passing a chord into one of these calls hooks the key press to the respective action. 
	UI_COMMAND(AcceptOrCompleteActiveTool, "Accept or Complete", "Accept or Complete the active Tool", EUserInterfaceActionType::Button, FInputChord(EKeys::Enter));
	UI_COMMAND(CancelOrCompleteActiveTool, "Cancel or Complete", "Cancel or Complete the active Tool", EUserInterfaceActionType::Button, FInputChord(EKeys::Escape));
	
	for (int32 i = 0; i < RegisteredTools.Num(); ++i)
	{
		CommandToRegisteredToolsIndex.Add(RegisteredTools[i].ToolCommand.Get(), i);
	}
#undef REGISTER_MODELING_TOOL_COMMAND
#undef REGISTER_MODELING_TOOL_COMMAND_WITH_SHORTNAME
#undef REGISTER_MODELING_ACTION_COMMAND
#undef REGISTER_MODELING_ACTION_COMMAND_WITH_SHORTNAME
}


TSharedPtr<FUICommandInfo> FModelingToolsManagerCommands::RegisterExtensionPaletteCommand(
	FName Name,
	const FText& Label,
	const FText& Tooltip,
	const FSlateIcon& Icon)
{
	if (Instance.IsValid())
	{
		TSharedPtr<FModelingToolsManagerCommands> Commands = Instance.Pin();

		for (FDynamicExtensionCommand& ExtensionCommand : Commands->ExtensionPaletteCommands)
		{
			if (ExtensionCommand.RegistrationName == Name)
			{
				return ExtensionCommand.Command;
			}
		}

		TSharedPtr<FUICommandInfo> NewCommandInfo;

		FUICommandInfo::MakeCommandInfo(
			Commands->AsShared(),
			NewCommandInfo,
			Name, Label, Tooltip, Icon,
			EUserInterfaceActionType::RadioButton,
			FInputChord() );

		FDynamicExtensionCommand NewExtensionCommand;
		NewExtensionCommand.RegistrationName = Name;
		NewExtensionCommand.Command = NewCommandInfo;
		Commands->ExtensionPaletteCommands.Add(NewExtensionCommand);
		return NewCommandInfo;
	};

	return TSharedPtr<FUICommandInfo>();
}



#undef LOCTEXT_NAMESPACE
