// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SVGPath.h"

/*
 * A set of graphical elements (lines, rectangles, circles, etc)
 * implemented as SVG paths (all derive from FSVGPath)
 */

struct SVGIMPORTER_API FSVGLine : public FSVGPath
{
public:
	FSVGLine(float InPathLength, float InX1, float InY1, float InX2, float InY2);
	
	float X1 = 0.0f;
	float Y1 = 0.0f;
	float X2 = 0.0f;
	float Y2 = 0.0f;
};

struct SVGIMPORTER_API FSVGPolyLine : public FSVGPath
{
public:
	FSVGPolyLine(float InPathLength, const TArray<FVector2D>& InPoints);
	
	TArray<FVector2D> Points;
};

struct SVGIMPORTER_API FSVGPolygon : public FSVGPolyLine
{
public:
	FSVGPolygon(float InPathLength, const TArray<FVector2D>& InPoints);
};

struct SVGIMPORTER_API FSVGRectangle : public FSVGPath
{
public:	
	FSVGRectangle(float InX, float InY, float InWidth, float InHeight, float InRx, float InRy);
	
	float X;
	float Y;
	float Width;
	float Height;
	float Rx;
	float Ry;
};

struct SVGIMPORTER_API FSVGCircle : public FSVGPath
{
public:	
	FSVGCircle(float InCx, float InCy, float InR);
	
	float Cx;
	float Cy;
	float R;
};

struct SVGIMPORTER_API FSVGEllipse : public FSVGPath
{
public:
	FSVGEllipse(float InCx, float InCy, float InRx, float InRy);
	
	float Cx;
	float Cy;
	float Rx;
	float Ry;
};
