// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/Deformable/GaussSeidelMasterConstraint.h"

namespace Chaos::Softs
{
	template <typename T, typename ParticleType>
	void FGaussSeidelMasterConstraint<T, ParticleType>::AddStaticConstraints(const TArray<TArray<int32>>& ExtraConstraints, TArray<TArray<int32>>& ExtraIncidentElements, TArray<TArray<int32>>& ExtraIncidentElementsLocal)
	{	
		if (!IsClean(ExtraConstraints, ExtraIncidentElements, ExtraIncidentElementsLocal))
		{
			ExtraIncidentElements = Chaos::Utilities::ComputeIncidentElements(ExtraConstraints, &ExtraIncidentElementsLocal);
		}

		int32 Offset = StaticConstraints.Num();
		StaticConstraints += ExtraConstraints;
		for (int32 i = 0; i < ExtraIncidentElements.Num(); i++)
		{
			if (ExtraIncidentElements[i].Num() > 0)
			{
				TArray<int32> ExtraIncidentElementsWithOffset = ExtraIncidentElements[i];
				for (int32 j = 0; j < ExtraIncidentElementsWithOffset.Num(); j++)
				{
					ExtraIncidentElementsWithOffset[j] += Offset;
				}
				StaticIncidentElements[i] += ExtraIncidentElementsWithOffset;
				StaticIncidentElementsLocal[i] += ExtraIncidentElementsLocal[i];
			}
		}
		if (StaticIncidentElementsOffsets.Num() > 0)
		{
			StaticIncidentElementsOffsets.RemoveAt(StaticIncidentElementsOffsets.Num() - 1);
		}
		StaticIncidentElementsOffsets.Add(Offset);
		StaticIncidentElementsOffsets.Add(StaticConstraints.Num());
	}

	template <typename T, typename ParticleType>
	void FGaussSeidelMasterConstraint<T, ParticleType>::AddDynamicConstraints(const TArray<TArray<int32>>& ExtraConstraints, TArray<TArray<int32>>& ExtraIncidentElements, TArray<TArray<int32>>& ExtraIncidentElementsLocal, bool CheckIncidentElements)
	{
		if (CheckIncidentElements)
		{
			if (!IsClean(ExtraConstraints, ExtraIncidentElements, ExtraIncidentElementsLocal))
			{
				ExtraIncidentElements = Chaos::Utilities::ComputeIncidentElements(ExtraConstraints, &ExtraIncidentElementsLocal);
			}
		}

		int32 Offset = DynamicConstraints.Num();
		DynamicConstraints += ExtraConstraints;
		for (int32 i = 0; i < ExtraIncidentElements.Num(); i++)
		{
			if (ExtraIncidentElements[i].Num() > 0)
			{
				TArray<int32> ExtraIncidentElementsWithOffset = ExtraIncidentElements[i];
				for (int32 j = 0; j < ExtraIncidentElementsWithOffset.Num(); j++)
				{
					ExtraIncidentElementsWithOffset[j] += Offset;
				}
				DynamicIncidentElements[i] += ExtraIncidentElementsWithOffset;
				DynamicIncidentElementsLocal[i] += ExtraIncidentElementsLocal[i];
			}
		}
		if (DynamicIncidentElementsOffsets.Num() > 0)
		{
			DynamicIncidentElementsOffsets.RemoveAt(StaticIncidentElementsOffsets.Num() - 1);
		}
		DynamicIncidentElementsOffsets.Add(Offset);
		DynamicIncidentElementsOffsets.Add(DynamicConstraints.Num());
	
	}

}


template CHAOS_API void Chaos::Softs::FGaussSeidelMasterConstraint<Chaos::FRealSingle, Chaos::Softs::FSolverParticles>::AddStaticConstraints(const TArray<TArray<int32>>& ExtraConstraints, TArray<TArray<int32>>& ExtraIncidentElements, TArray<TArray<int32>>& ExtraIncidentElementsLocal);

template CHAOS_API void Chaos::Softs::FGaussSeidelMasterConstraint<Chaos::FRealDouble, Chaos::TDynamicParticles<Chaos::FRealDouble, 3>>::AddStaticConstraints(const TArray<TArray<int32>>& ExtraConstraints, TArray<TArray<int32>>& ExtraIncidentElements, TArray<TArray<int32>>& ExtraIncidentElementsLocal);

template CHAOS_API void Chaos::Softs::FGaussSeidelMasterConstraint<Chaos::FRealSingle, Chaos::Softs::FSolverParticles>::AddDynamicConstraints(const TArray<TArray<int32>>& ExtraConstraints, TArray<TArray<int32>>& ExtraIncidentElements, TArray<TArray<int32>>& ExtraIncidentElementsLocal, bool CheckIncidentElements);

template CHAOS_API void Chaos::Softs::FGaussSeidelMasterConstraint<Chaos::FRealDouble, Chaos::TDynamicParticles<Chaos::FRealDouble, 3>>::AddDynamicConstraints(const TArray<TArray<int32>>& ExtraConstraints, TArray<TArray<int32>>& ExtraIncidentElements, TArray<TArray<int32>>& ExtraIncidentElementsLocal, bool CheckIncidentElements);
