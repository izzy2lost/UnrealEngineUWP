// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

/** Configuration for generated jump down links. */
struct dtNavLinkBuilderJumpDownConfig
{
	bool enabled = true;
	float jumpLength = 150.f; 
	float jumpDistanceFromEdge = 10.f; 
	float jumpMaxDepth = 150.f;
	float jumpEndsHeightTolerance = 50.f;
	float samplingSeparationFactor = 1.f;	
};

/** Configuration for generated jump over links. */
struct dtNavLinkBuilderJumpOverConfig
{
	bool enabled = true;
	float jumpLength = 200.f; 
	float jumpDistanceFromEdge = 100.f; 
	float jumpHeight = 100.f; 
	float jumpHeightTolerance = 100.f; 
	float jumpEndsHeightTolerance = 50.f;
	float samplingSeparationFactor = 1.f;
};
