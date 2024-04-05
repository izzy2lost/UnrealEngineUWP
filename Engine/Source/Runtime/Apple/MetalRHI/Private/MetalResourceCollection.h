// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHICoreResourceCollection.h"
#include "MetalRHIPrivate.h"

using FMetalResourceCollection = UE::RHICore::FGenericResourceCollection;

template<>
struct TMetalResourceTraits<FRHIResourceCollection>
{
	using TConcreteType = FMetalResourceCollection;
};
