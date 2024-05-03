// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "DetourAlloc.h"
#include "Detour/DetourLargeWorldCoordinates.h"

//@UE BEGIN
enum dtNavLinkAction
{
	DT_LINK_ACTION_UNSET,
	DT_LINK_ACTION_JUMP_DOWN,
	DT_LINK_ACTION_JUMP_OVER,
};

struct dtLinkBuilderConfig
{
	dtReal agentRadius = 0;
	dtReal agentHeight = 0;
	dtReal agentClimb = 0;
	dtReal cellSize = 0;
	dtReal cellHeight = 0;
};

struct rcHeightfield;
struct rcCompactHeightfield;

NAVMESH_API struct dtLinkBuilderData
{
	bool generatingLinks = false;
	rcHeightfield* solidHF = nullptr;
	rcCompactHeightfield* compactHF = nullptr;
};

class rcContext;
struct rcConfig;

class dtNavLinkBuilder
{
	dtLinkBuilderConfig m_linkBuilderConfig;

	dtReal m_cs = 0;
	rcHeightfield* m_solid;
	rcCompactHeightfield* m_chf;

	struct Edge
	{
		dtReal sp[3], sq[3];
	};
	Edge* m_edges;
	int m_nedges;
	
public:	
	static constexpr int MAX_SPINE = 8;
	
	struct TrajectorySample
	{
		float x, ymin, ymax;
	};
	
	struct Trajectory2D
	{
		Trajectory2D() : samples(nullptr), nsamples(0), nspine(0) {}
		NAVMESH_API ~Trajectory2D();

		float spine[2*MAX_SPINE];		// [x,y] relative points representing the desired trajectory (2 spines)
		TrajectorySample* samples;		// samples along trajectory slices to check for collision
		
		unsigned short nsamples;
		unsigned char nspine;			// @todo: remove (use direclty MAX_SPINE) or make relative to trajectory type and config
	};

private:	
	enum GroundSampleFlag : unsigned char
	{
		UNSET = 0,
		HAS_GROUND = 1,
		UNRESTRICTED = 4,
	};
	
	struct GroundSample
	{
		dtReal height;
		GroundSampleFlag flags;
	};

	struct PotentialSeg
	{
		unsigned char mark;
		int idx;
		float umin, umax;
		float dmin, dmax;
		float sp[3], sq[3];
	};
	
	struct GroundSegment
	{
		GroundSegment() : gsamples(nullptr), ngsamples(0) {}
		NAVMESH_API ~GroundSegment();

		dtReal p[3], q[3];
		GroundSample* gsamples;
		unsigned short ngsamples;
		unsigned short npass;
	};
	
public:
	NAVMESH_API struct EdgeSampler
	{
		Trajectory2D trajectory;
	
		GroundSegment start;
		GroundSegment end;

		dtReal rigp[3], rigq[3];		// edge
		dtReal ax[3], ay[3], az[3];		// axis along edge

		float groundRange;
		
		dtNavLinkAction action = DT_LINK_ACTION_UNSET;
	};

	enum JumpLinkFlag : unsigned char
	{
		INVALID = 0,
		VALID = 1,
	};
	
	struct JumpLink
	{
		dtReal spine0[MAX_SPINE*3];
		dtReal spine1[MAX_SPINE*3];
		int nspine;
		JumpLinkFlag flags;
		dtNavLinkAction action = DT_LINK_ACTION_UNSET;
	};
	JumpLink* m_links;
	int m_nlinks;
	int m_clinks;

private:
	int m_debugSelectedEdge;

	friend NAVMESH_API void duDebugDrawNavLinkBuilder(struct duDebugDraw* dd, const dtNavLinkBuilder& linkBuilder, unsigned int drawFlags, const EdgeSampler* es);
	
public:
	NAVMESH_API dtNavLinkBuilder();
	NAVMESH_API ~dtNavLinkBuilder();

	// Loops through contours to store edge points in world coordinates.
	NAVMESH_API bool findEdges(rcContext& ctx, const rcConfig& cfg, const dtLinkBuilderConfig& builderConfig,
							   const struct dtTileCacheContourSet& lcset, const dtReal* orig, const dtLinkBuilderData& linkBuilderData);

	// For all edges, sample edges (sampleEdge) and add links to m_links
	NAVMESH_API void buildForAllEdges(const dtLinkBuilderConfig& acfg, dtNavLinkAction action);

	NAVMESH_API void debugBuildEdge(const dtLinkBuilderConfig& acfg, dtNavLinkAction action, int edgeIndex, EdgeSampler& sampler);
	
private:
	void cleanup();
	void initTrajectory(Trajectory2D* tra) const;
	bool isTrajectoryClear(const dtReal* pa, const dtReal* pb, const Trajectory2D* tra) const;
	
	int findPotentialJumpOverEdges(const dtReal* sp, const dtReal* sq,
								   const float depthRange, const float heightRange,
								   dtReal* outSegs, const int maxOutSegs) const;
	
	static void initJumpDownRig(EdgeSampler* es, const dtReal* sp, const dtReal* sq,
						 const float jumpStartDist, const float jumpEndDist,
						 const float jumpDownDist, const float groundRange);

	static void initJumpOverRig(EdgeSampler* es, const dtReal* sp, const dtReal* sq,
						 const float jumpStartDist, const float jumpEndDist,
						 const float jumpHeight, const float groundRange);

	bool getCompactHeightfieldHeight(const dtReal* pt, const float hrange, dtReal* height) const;
	bool checkHeightfieldCollision(const dtReal x, const dtReal ymin, const dtReal ymax, const dtReal z) const;

	void sampleGroundSegment(GroundSegment* seg, const float nsamples, const float groundRange) const;
	
	void sampleAction(const EdgeSampler* es) const;
	
	void filterJumpOverLinks() const;

	bool sampleEdge(dtNavLinkAction desiredAction, const dtReal* sp, const dtReal* sq, dtNavLinkBuilder::EdgeSampler* sampler) const;
	JumpLink* addLink();
	void addEdgeLinks(const dtLinkBuilderConfig& acfg, const EdgeSampler* es);
};
//@UE END