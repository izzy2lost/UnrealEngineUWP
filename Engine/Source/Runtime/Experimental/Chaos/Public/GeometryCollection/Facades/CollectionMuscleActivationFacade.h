// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/ManagedArrayAccessor.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/Facades/CollectionSelectionFacade.h"

namespace GeometryCollection::Facades
{
	struct FMuscleActivationData
	{
		TArray<int32> MuscleActivationElement;
		FIntVector2 OriginInsertionPair;
		float OriginInsertionRestLength;
		TArray<Chaos::PMatrix33d> FiberDirectionMatrix;
	};

	/** Kinematic Facade */
	class FMuscleActivationFacade
	{
	public:

		static CHAOS_API const FName GroupName;
		static CHAOS_API const FName MuscleActivationElement;
		static CHAOS_API const FName OriginInsertionPair;
		static CHAOS_API const FName OriginInsertionRestLength;
		static CHAOS_API const FName FiberDirectionMatrix;

		CHAOS_API FMuscleActivationFacade(FManagedArrayCollection& InCollection);
		CHAOS_API FMuscleActivationFacade(const FManagedArrayCollection& InCollection);

		/** Create the facade attributes. */
		CHAOS_API void DefineSchema();

		/** Is the facade defined constant. */
		bool IsConst() const { return Collection == nullptr; }

		/** Is the Facade defined on the collection? */
		CHAOS_API bool IsValid() const;

		//
		//  Skeletal Mesh Bone Bindings
		//
		CHAOS_API int32 AddMuscleActivationData(const FMuscleActivationData& InputData);
		CHAOS_API const FMuscleActivationData GetMuscleActivationData(const int32 DataIndex) const;
		int32 NumMuscles() const { return MuscleActivationElementAttribute.Num(); }

	private:
		const FManagedArrayCollection& ConstCollection;
		FManagedArrayCollection* Collection = nullptr;

		TManagedArrayAccessor<TArray<int32>> MuscleActivationElementAttribute;
		TManagedArrayAccessor<FIntVector2> OriginInsertionPairAttribute;
		TManagedArrayAccessor<float> OriginInsertionRestLengthAttribute;
		TManagedArrayAccessor<TArray<Chaos::PMatrix33d>> FiberDirectionMatrixAttribute;
	};
}
