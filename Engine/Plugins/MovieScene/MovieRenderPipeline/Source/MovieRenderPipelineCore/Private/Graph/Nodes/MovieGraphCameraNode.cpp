// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphCameraNode.h"

#include "Styling/AppStyle.h"

#if WITH_EDITOR
FText UMovieGraphCameraSettingNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText NodeName = NSLOCTEXT("MovieGraphNodes", "NodeName_CameraSettings", "Camera Settings");
	return NodeName;
}

FText UMovieGraphCameraSettingNode::GetMenuCategory() const
{
	static const FText NodeCategory_Globals = NSLOCTEXT("MovieGraphNodes", "NodeCategory_Globals", "Globals");
	return NodeCategory_Globals;
}

FLinearColor UMovieGraphCameraSettingNode::GetNodeTitleColor() const
{
	static const FLinearColor NodeColor = FLinearColor(0.65f, 0.60f, 0.75f);
	return NodeColor;
}

FSlateIcon UMovieGraphCameraSettingNode::GetIconAndTint(FLinearColor& OutColor) const
{
	static const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.CameraComponent");

	OutColor = FLinearColor::White;
	return Icon;
}
#endif // WITH_EDITOR