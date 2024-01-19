// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshDescription.h"

class FMorphTargetVertexAttributesConstRef;

class FMorphTargetVertexAttributesRef
{
	friend class FSkeletalMeshAttributes;
	friend class FMorphTargetVertexAttributesConstRef;

public:
	FMorphTargetVertexAttributesRef() = default;
	FMorphTargetVertexAttributesRef(const FMorphTargetVertexAttributesRef&) = default;
	FMorphTargetVertexAttributesRef(FMorphTargetVertexAttributesRef&&) = default;
	FMorphTargetVertexAttributesRef& operator=(const FMorphTargetVertexAttributesRef&) = default;
	FMorphTargetVertexAttributesRef& operator=(FMorphTargetVertexAttributesRef&&) = default;

	bool IsValid() const
	{
		return AttributesRef.IsValid();
	}

	void Copy(const FMorphTargetVertexAttributesConstRef& InSourceAttribute);

	FVector3f GetPositionDelta(const FVertexID InVertexIndex) const
	{
		return AttributesRef.Get(InVertexIndex)[0]; 
	}

	FVector3f GetTangentZDelta(const FVertexID InVertexIndex) const
	{
		return AttributesRef.Get(InVertexIndex)[1]; 
	}

	void SetPositionDelta(
		const FVertexID InVertexIndex,
		FVector3f InPositionDelta
		)
	{
		AttributesRef.Get(InVertexIndex)[0] = InPositionDelta;
	}
	
	void SetTangentZDelta(
		const FVertexID InVertexIndex,
		FVector3f InTangentZDelta
		)
	{
		AttributesRef.Get(InVertexIndex)[1] = InTangentZDelta;
	}
	
	void SetPositionAndTangentZDelta(
		const FVertexID InVertexIndex,
		FVector3f InPositionDelta,
		FVector3f InTangentZDelta
		)
	{
		AttributesRef.SetArrayView(InVertexIndex, {InPositionDelta, InTangentZDelta});
	}

protected:
	FMorphTargetVertexAttributesRef(TVertexAttributesRef<TArrayView<FVector3f>> InAttributesRef)
		: AttributesRef(InAttributesRef)
	{}

private:
	TVertexAttributesRef<TArrayView<FVector3f>> AttributesRef;
};


class FMorphTargetVertexAttributesConstRef
{
public:
	FMorphTargetVertexAttributesConstRef() = default;
	FMorphTargetVertexAttributesConstRef(const FMorphTargetVertexAttributesConstRef&) = default;
	FMorphTargetVertexAttributesConstRef(FMorphTargetVertexAttributesConstRef&&) = default;
	FMorphTargetVertexAttributesConstRef& operator=(const FMorphTargetVertexAttributesConstRef&) = default;
	FMorphTargetVertexAttributesConstRef& operator=(FMorphTargetVertexAttributesConstRef&&) = default;

	// Converting constructors from the non-const variant
	explicit FMorphTargetVertexAttributesConstRef(const FMorphTargetVertexAttributesRef& InAttributesRef) :
		AttributesConstRef(InAttributesRef.AttributesRef)
	{}
	
	FMorphTargetVertexAttributesConstRef& operator=(const FMorphTargetVertexAttributesRef& InAttributesRef)
	{
		AttributesConstRef = InAttributesRef.AttributesRef;
		return *this;
	}
	
	bool IsValid() const
	{
		return AttributesConstRef.IsValid();
	}

	FVector3f GetPositionDelta(const FVertexID InVertexIndex) const
	{
		return AttributesConstRef.Get(InVertexIndex)[0]; 
	}

	FVector3f GetTangentZDelta(const FVertexID InVertexIndex) const
	{
		return AttributesConstRef.Get(InVertexIndex)[1]; 
	}

protected:
	friend class FMorphTargetVertexAttributesRef;
	friend class FSkeletalMeshAttributes;
	friend class FSkeletalMeshAttributesShared;

	FMorphTargetVertexAttributesConstRef(TVertexAttributesRef<TArrayView<FVector3f>> InAttributesConstRef)
		: AttributesConstRef(InAttributesConstRef)
	{}

	FMorphTargetVertexAttributesConstRef(TVertexAttributesRef<TArrayView<const FVector3f>> InAttributesConstRef)
		: AttributesConstRef(InAttributesConstRef)
	{}
	
private:
	TVertexAttributesConstRef<TArrayView<FVector3f>> AttributesConstRef;
};


inline void FMorphTargetVertexAttributesRef::Copy(const FMorphTargetVertexAttributesConstRef& InSourceAttribute)
{
	AttributesRef.Copy(InSourceAttribute.AttributesConstRef);
}
