// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Containers/ArrayView.h"

namespace Chaos::Softs
{

/**
 * Simple weight map just converting from per particle to per constraint values. Also holds low/high values, but these are multiplied against the map each time the operator[] is called.
 */
class FPBDFlatWeightMap final
{
public:

	/**
	 * Weightmap particle constructor. 
	 */
	inline explicit FPBDFlatWeightMap(
		const FSolverVec2& InWeightedValue,
		const TConstArrayView<FRealSingle>& Multipliers = TConstArrayView<FRealSingle>(),
		int32 ParticleCount = 0);  // A value of 0 also disables the map  

	/**
	 * Weightmap constraint constructor. 
	 */
	template<int32 Valence>
	inline explicit FPBDFlatWeightMap(
		const FSolverVec2& InWeightedValue,
		const TConstArrayView<FRealSingle>& Multipliers = TConstArrayView<FRealSingle>(),
		const TConstArrayView<TVector<int32, Valence>>& Constraints = TConstArrayView<TVector<int32, Valence>>(),
		int32 ParticleOffset = INDEX_NONE, // Constraints have usually a particle offset added to them compared to the weight maps that always starts at index 0
		int32 ParticleCount = 0,  // A value of 0 also disables the map
		typename TEnableIf<Valence >= 2 && Valence <= 4>::Type* = nullptr);  // Prevents incorrect valence, the value is actually unused

	~FPBDFlatWeightMap() = default;

	FPBDFlatWeightMap(const FPBDFlatWeightMap&) = default;
	FPBDFlatWeightMap(FPBDFlatWeightMap&&) = default;
	FPBDFlatWeightMap& operator=(const FPBDFlatWeightMap&) = default;
	FPBDFlatWeightMap& operator=(FPBDFlatWeightMap&&) = default;

	/** Return the number of values stored in the weight map. */
	int32 Num() const { return MapValues.Num(); }

	/** Return whether this object contains weight map values. */
	bool HasWeightMap() const { return MapValues.Num() > 1 && FMath::Abs(OffsetRange[1]) > UE_KINDA_SMALL_NUMBER; }

	/**
	 * Set the low and high values of the weight map.
	 */
	void SetWeightedValue(const FSolverVec2& InWeightedValue) { OffsetRange[0] = InWeightedValue[0]; OffsetRange[1] = InWeightedValue[1] - InWeightedValue[0]; }

	/**
	 * Return the values set for this map as an Offset and Range (Low = OffsetRange[0], High = OffsetRange[0] + OffsetRange[1])
	 */
	const FSolverVec2& GetOffsetRange() const { return OffsetRange; }

	/**
	 * Lookup for the weighted value at the specified weight map index.
	 * This function will assert if it is called with a non zero index on an empty weight map.
	*/
	FSolverReal operator[](int32 Index) const { return OffsetRange[0] + OffsetRange[1] * MapValues[Index]; }

	/** Return the value at the Low weight. */
	FSolverReal GetLow() const { return OffsetRange[0]; }

	/** Return the value at the High weight. */
	FSolverReal GetHigh() const { return OffsetRange[0] + OffsetRange[1]; }

	/** Return the value when the weight map is not used. */
	explicit operator FSolverReal() const { return GetLow(); }

	/** Return the table of stiffnesses as a read only array. */
	TConstArrayView<FSolverReal> GetMapValues() const { return TConstArrayView<FSolverReal>(MapValues); }

	/** Reorder Indices based on Constraint reordering. */
	inline void ReorderIndices(const TArray<int32>& OrigToReorderedIndices);

protected:

	TArray<FSolverReal> MapValues; 
	FSolverVec2 OffsetRange;
};

inline FPBDFlatWeightMap::FPBDFlatWeightMap(
	const FSolverVec2& InWeightedValue,
	const TConstArrayView<FRealSingle>& Multipliers,
	int32 ParticleCount)
	: OffsetRange(InWeightedValue[0], InWeightedValue[1] - InWeightedValue[0])
{
	if (Multipliers.Num() == ParticleCount && ParticleCount > 0)
	{
		// Copy multipliers with clamp
		MapValues.SetNumUninitialized(ParticleCount);
		for (int32 Index = 0; Index < ParticleCount; ++Index)
		{
			MapValues[Index] = FMath::Clamp(Multipliers[Index], (FRealSingle)0., (FRealSingle)1.);
		}
	}
}

template<int32 Valence>
inline FPBDFlatWeightMap::FPBDFlatWeightMap(
	const FSolverVec2& InWeightedValue,
	const TConstArrayView<FRealSingle>& Multipliers,
	const TConstArrayView<TVector<int32, Valence>>& Constraints,
	int32 ParticleOffset,
	int32 ParticleCount,
	typename TEnableIf<Valence >= 2 && Valence <= 4>::Type*)
	: OffsetRange(InWeightedValue[0], InWeightedValue[1] - InWeightedValue[0])
{
	const int32 ConstraintCount = Constraints.Num();

	if (Multipliers.Num() == ParticleCount && ParticleCount > 0 && ConstraintCount > 0)
	{
		// Average vertex multipliers to constraints.
		MapValues.SetNumUninitialized(ConstraintCount);

		for (int32 ConstraintIndex = 0; ConstraintIndex < ConstraintCount; ++ConstraintIndex)
		{
			const TVector<int32, Valence>& Constraint = Constraints[ConstraintIndex];

			FRealSingle Weight = 0.f;
			for (int32 Index = 0; Index < Valence; ++Index)
			{
				Weight += FMath::Clamp(Multipliers[Constraint[Index] - ParticleOffset], (FRealSingle)0., (FRealSingle)1.);
			}
			Weight /= (FRealSingle)Valence;

			MapValues[ConstraintIndex] = Weight;
		}
	}
}

inline void FPBDFlatWeightMap::ReorderIndices(const TArray<int32>& OrigToReorderedConstraintIndices)
{
	if (MapValues.Num() == OrigToReorderedConstraintIndices.Num())
	{
		TArray<FRealSingle> ReorderedValues;
		ReorderedValues.SetNumUninitialized(MapValues.Num());
		for (int32 OrigConstraintIndex = 0; OrigConstraintIndex < OrigToReorderedConstraintIndices.Num(); ++OrigConstraintIndex)
		{
			const int32 ReorderedConstraintIndex = OrigToReorderedConstraintIndices[OrigConstraintIndex];
			ReorderedValues[ReorderedConstraintIndex] = MapValues[OrigConstraintIndex];
		}
		MapValues = MoveTemp(ReorderedValues);
	}
	else
	{
		check(MapValues.Num() == 0);
	}
}

}  // End namespace Chaos::Softs
