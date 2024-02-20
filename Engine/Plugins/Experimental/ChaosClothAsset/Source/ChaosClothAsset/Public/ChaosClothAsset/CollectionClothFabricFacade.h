// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

namespace UE::Chaos::ClothAsset
{
	/**
	 * Cloth Asset collection fabric facade class to access cloth fabric data.
	 * Constructed from FCollectionClothConstFacade.
	 * Const access (read only) version.
	 */
	class CHAOSCLOTHASSET_API FCollectionClothFabricConstFacade
	{
	public:
		FCollectionClothFabricConstFacade() = delete;

		FCollectionClothFabricConstFacade(const FCollectionClothFabricConstFacade&) = delete;
		FCollectionClothFabricConstFacade& operator=(const FCollectionClothFabricConstFacade&) = delete;

		FCollectionClothFabricConstFacade(FCollectionClothFabricConstFacade&&) = default;
		FCollectionClothFabricConstFacade& operator=(FCollectionClothFabricConstFacade&&) = default;

		virtual ~FCollectionClothFabricConstFacade() = default;

		/** Anisotropic fabric datas structure (Weft,Warp,Bias) */
		struct CHAOSCLOTHASSET_API FAnisotropicData
		{
			FAnisotropicData(const float WeftValue, const float WarpValue, const float BiasValue);
			FAnisotropicData(const FVector3f& VectorDatas);
			FAnisotropicData(const float& FloatDatas);
			
			FVector3f GetVectorDatas() const;
			
			
			float Weft;
			float Warp;
			float Bias;
		};

		/** Return the anisotropic bending stiffness */
		FAnisotropicData GetXPBDAnisoBendingStiffness() const;

		/** Return the buckling ratio */
		float GetXPBDAnisoBucklingRatio() const;

		/** Return the anisotropic buckling stiffness */
		FAnisotropicData GetXPBDAnisoBucklingStiffness() const;

		/** Return the anisotropic stretch stiffness */
		FAnisotropicData GetXPBDAnisoSpringStiffness() const;

		/** Return the cloth density */
		float GetDensityWeighted() const;

		/** Return the cloth thickness */
		float GetCollisionThickness() const;

		/** Return the cloth damping */
		float GetXPBDAnisoDamping() const;

		/** Return the cloth friction */
		float GetFrictionCoefficient() const;

	protected:
		friend class FCollectionClothFabricFacade;  // For other instances access
		friend class FCollectionClothConstFacade;
		FCollectionClothFabricConstFacade(const TSharedRef<const class FClothCollection>& InClothCollection, int32 InFabricIndex);

		static constexpr int32 GetBaseElementIndex() { return 0; }

		/** Get the global element index */
		int32 GetElementIndex() const { return GetBaseElementIndex() + FabricIndex; }

		/** Cloth collection modified by  the fabric facade */
		TSharedRef<const class FClothCollection> ClothCollection;

		/** Fabric index that will be referred in the sim patterns */
		int32 FabricIndex;
	};

	/**
	 * Cloth Asset collection fabric facade class to access cloth fabric data.
	 * Constructed from FCollectionClothFacade.
	 * Non-const access (read/write) version.
	 */
	class CHAOSCLOTHASSET_API FCollectionClothFabricFacade final : public FCollectionClothFabricConstFacade
	{
	public:
		FCollectionClothFabricFacade() = delete;

		FCollectionClothFabricFacade(const FCollectionClothFabricFacade&) = delete;
		FCollectionClothFabricFacade& operator=(const FCollectionClothFabricFacade&) = delete;

		FCollectionClothFabricFacade(FCollectionClothFabricFacade&&) = default;
		FCollectionClothFabricFacade& operator=(FCollectionClothFabricFacade&&) = default;

		virtual ~FCollectionClothFabricFacade() override = default;

		/** Initialize the cloth fabric with simulation parameters. */
		void Initialize(const FAnisotropicData& BendingStiffness, const float BucklingRatio,
			const FAnisotropicData& BucklingStiffness, const FAnisotropicData& StretchStiffness,
			const float ClothDensity, const float ClothFriction,
			const float ClothDamping, const float ClothThickness);

		/** Initialize the cloth fabric with another one. */
		void Initialize(const FCollectionClothFabricConstFacade& OtherFabricFacade);

	private:
		friend class FCollectionClothFacade;
		FCollectionClothFabricFacade(const TSharedRef<class FClothCollection>& InClothCollection, int32 InFabricIndex);

		/** Set default values to the fabric properties */
		void SetDefaults();

		/** Reset the fabric values properties */
		void Reset();

		/** Get the non const cloth collection */
		TSharedRef<class FClothCollection> GetClothCollection() { return ConstCastSharedRef<class FClothCollection>(ClothCollection); }
	};
}  // End namespace UE::Chaos::ClothAsset
