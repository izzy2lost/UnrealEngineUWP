// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "GeometryCollectionExternalRenderInterface.generated.h"

class UGeometryCollection;
class UGeometryCollectionComponent;

UINTERFACE(MinimalAPI)
class UGeometryCollectionExternalRenderInterface : public UInterface
{
	GENERATED_BODY()
};

class IGeometryCollectionExternalRenderInterface
{
	GENERATED_BODY()

public:
	enum EStateFlags
	{
		EState_Visible		= 1 << 0,
		EState_Broken		= 1 << 1,
		EState_ForcedBroken	= 1 << 2,
	};

	virtual void OnRegisterGeometryCollection(UGeometryCollectionComponent const& InComponent) = 0;
	virtual void OnUnregisterGeometryCollection() = 0;
	virtual void UpdateState(UGeometryCollection const& InGeometryCollection, FTransform const& InComponentTransform, uint32 InStateFlags) = 0;
	virtual void UpdateRootTransform(UGeometryCollection const& InGeometryCollection, FTransform const& InRootTransform) = 0;
	virtual void UpdateTransforms(UGeometryCollection const& InGeometryCollection, TArrayView<const FTransform3f> InTransforms) = 0;

	// todo: Maybe move this to a separate ISMPool renderer interface? But maybe that is overengineering?
	virtual void SetCustomInstanceData(int32 CustomFloatIndex, float CustomFloatValue) {}

	UE_DEPRECATED(5.4, "Use flags version of UpdateState instead")
	virtual void UpdateState(UGeometryCollection const& InGeometryCollection, FTransform const& InComponentTransform, bool bInIsBroken, bool bInIsVisible) 
	{
		uint32 StateFlags = 0;
		StateFlags |= bInIsVisible ? IGeometryCollectionExternalRenderInterface::EState_Visible : 0;
		StateFlags |= bInIsBroken ? IGeometryCollectionExternalRenderInterface::EState_Broken : 0;
		return UpdateState(InGeometryCollection, InComponentTransform, StateFlags);
	};
	UE_DEPRECATED(5.3, "Use FTransform version of UpdateTransforms instead")
	virtual void UpdateTransforms(UGeometryCollection const& InGeometryCollection, TArrayView<const FMatrix> InMatrices) {};
};
