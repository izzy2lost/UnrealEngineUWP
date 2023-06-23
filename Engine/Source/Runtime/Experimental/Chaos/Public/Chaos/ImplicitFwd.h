// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Chaos/Core.h"

namespace Chaos
{
	template<class T, int d> class TBox;
	template<class T, int d> class TPlane;
	template<class T, int d> class TSphere;
	template<class T> class TTriangle;

	class FConvex;
	class FCapsule;
	class FHeightField;
	class FImplicitObject;

	using FImplicitBox3 = TBox<FReal, 3>;
	using FImplicitCapsule3 = FCapsule;
	using FImplicitConvex3 = FConvex;
	using FImplicitHeightField3 = FHeightField;
	using FImplicitObject3 = FImplicitObject;
	using FImplicitPlane3 = TPlane<FReal, 3>;
	using FImplicitSphere3 = TSphere<FReal, 3>;

	using FTriangle = TTriangle<FReal>;
}