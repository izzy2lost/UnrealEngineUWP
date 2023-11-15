// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/ArrayCollectionArray.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/Particles.h"
#include "Chaos/Rotation.h"
#include "UObject/FortniteValkyrieBranchObjectVersion.h"


namespace Chaos
{
	template<class T, int d>
	class TSimpleGeometryParticles : public TParticles<T, d>
	{
	public:

		using TArrayCollection::Size;
		using TParticles<T,d>::X;
		
		TSimpleGeometryParticles()
		    : TParticles<T, d>()
		{
			TArrayCollection::AddArray(&MR);
			TArrayCollection::AddArray(&MGeometry);
		}
		TSimpleGeometryParticles(const TSimpleGeometryParticles<T, d>& Other) = delete;
		TSimpleGeometryParticles(TSimpleGeometryParticles<T, d>&& Other)
		    : TParticles<T, d>(MoveTemp(Other))
			, MR(MoveTemp(Other.MR))
			, MGeometry(MoveTemp(Other.MGeometry))
		{
			TArrayCollection::AddArray(&MR);
			TArrayCollection::AddArray(&MGeometry);
		}
		TSimpleGeometryParticles& operator=(const TSimpleGeometryParticles<T, d>& Other) = delete;
		TSimpleGeometryParticles& operator=(TSimpleGeometryParticles<T, d>&& Other) = delete;

		TSimpleGeometryParticles(TParticles<T, d>&& Other)
		    : TParticles<T, d>(MoveTemp(Other))
		{
			TArrayCollection::AddArray(&MR);
			TArrayCollection::AddArray(&MGeometry);
		}

		virtual ~TSimpleGeometryParticles() override
		{}

		FORCEINLINE const TRotation<T, d>& R(const int32 Index) const { return MR[Index]; }
		FORCEINLINE TRotation<T, d>& R(const int32 Index) { return MR[Index]; }
		const TArrayCollectionArray<TRotation<T, d>>& GetR() const { return MR; }
		TArrayCollectionArray<TRotation<T, d>>& GetR() { return MR; }

		FORCEINLINE const FImplicitObjectPtr& GetGeometry(const int32 Index) const { return MGeometry[Index]; }
		void SetGeometry(const int32 Index, const FImplicitObjectPtr& InGeometry)
		{
			SetGeometryImpl(Index, InGeometry);
		}

		FORCEINLINE const TArray<FImplicitObjectPtr>& GetAllGeometry() const { return MGeometry; }
		FORCEINLINE TArray<TRotation<T, d>>& AllR() { return MR; }

		virtual void Serialize(FChaosArchive& Ar)
		{
			TParticles<T, d>::Serialize(Ar);
			
			Ar.UsingCustomVersion(FFortniteValkyrieBranchObjectVersion::GUID);
			if (Ar.CustomVer(FFortniteValkyrieBranchObjectVersion::GUID) < FFortniteValkyrieBranchObjectVersion::RefCountedOImplicitObjects)
			{
				TArrayCollectionArray<TSerializablePtr<FImplicitObject>> LGeometry;
				TArrayCollectionArray<TUniquePtr<Chaos::FImplicitObject>> LDynamicGeometry;
				Ar << LGeometry << LDynamicGeometry;

				if(Ar.IsLoading())
				{
					MGeometry.SetNumUninitialized(LGeometry.Num());
					uint32 ImplicitIndex = 0;
					for(const TSerializablePtr<FImplicitObject>& ImplicitObjectPtr : LGeometry)
					{
						MGeometry[ImplicitIndex++] = ImplicitObjectPtr->CopyGeometry();
					}
				}
			}
			else
			{
				Ar << MGeometry;
			}
			Ar << MR;
		}

	protected:
		virtual void SetGeometryImpl(const int32 Index, const FImplicitObjectPtr& InGeometry)
		{
			MGeometry[Index] = InGeometry;
		}

	private:
		TArrayCollectionArray<TRotation<T, d>> MR;
		// MGeometry contains raw ptrs to every entry in both MSharedGeometry and MDynamicGeometry.
		// It may also contain raw ptrs to geometry which is managed outside of Chaos.
		TArrayCollectionArray<FImplicitObjectPtr> MGeometry;
	};

	template <typename T, int d>
	FChaosArchive& operator<<(FChaosArchive& Ar, TSimpleGeometryParticles<T, d>& Particles)
	{
		Particles.Serialize(Ar);
		return Ar;
	}
}

