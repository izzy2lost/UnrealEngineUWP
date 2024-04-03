// Copyright Epic Games, Inc.All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/Collision/ContactPoint.h"
#include "Chaos/Collision/ConvexFeature.h"

namespace Chaos::Private
{
	// A contact point between two convex features
	template<typename T>
	class TConvexContactPoint
	{
	public:
		using FRealType = T;

		TConvexContactPoint()
		{
			Reset();
		}

		void Reset()
		{
			Phi = InvalidPhi<FRealType>();
		}

		bool IsSet() const
		{
			return Phi != InvalidPhi<FRealType>();
		}

		EContactPointType GetContactPointType() const
		{
			if (IsSet())
			{
				if ((Features[0].FeatureType == EConvexFeatureType::Plane) && (Features[1].FeatureType == EConvexFeatureType::Vertex))
				{
					return EContactPointType::PlaneVertex;
				}
				if ((Features[0].FeatureType == EConvexFeatureType::Vertex) && (Features[1].FeatureType == EConvexFeatureType::Plane))
				{
					return EContactPointType::VertexPlane;
				}
				if ((Features[0].FeatureType == EConvexFeatureType::Edge) && (Features[1].FeatureType == EConvexFeatureType::Edge))
				{
					return EContactPointType::EdgeEdge;
				}
			}
			return EContactPointType::Unknown;
		}

		FConvexFeature Features[2];
		TVec3<FRealType> ShapeContactPoints[2];
		TVec3<FRealType> ShapeContactNormal;
		FRealType Phi;
	};

	using FConvexContactPoint = TConvexContactPoint<FReal>;
	using FConvexContactPointf = TConvexContactPoint<FRealSingle>;
}