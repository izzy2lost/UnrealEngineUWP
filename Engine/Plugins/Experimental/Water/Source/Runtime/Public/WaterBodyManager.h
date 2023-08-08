// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class UWaterBodyComponent;
class AWaterZone;
class FWaterViewExtension;

class WATER_API FWaterBodyManager
{
public:
	void Initialize(UWorld* World);
	void Deinitialize();

	/** 
	 * Register any water body component upon addition to the world
	 * @param InWaterBodyComponent
	 * @return int32 the unique sequential index assigned to this water body component
	 */
	int32 AddWaterBodyComponent(UWaterBodyComponent* InWaterBodyComponent);

	/** Unregister any water body upon removal to the world */
	void RemoveWaterBodyComponent(UWaterBodyComponent* InWaterBodyComponent);

	int32 AddWaterZone(AWaterZone* InWaterZone);
	void RemoveWaterZone(AWaterZone* InWaterZone);

	/** Recomputes water gpu data whenever it changes on one of the managed water types. */
	void RequestGPUDataRebuild();

	/** Recomputes wave-related data whenever it changes on one of water bodies. */
	void RequestWaveDataRebuild();

	/** Returns the maximum of all MaxWaveHeight : */
	float GetGlobalMaxWaveHeight() const { return GlobalMaxWaveHeight; }

	/** Execute a predicate function on each valid water body. Predicate should return false for early exit. */
	void ForEachWaterBodyComponent (TFunctionRef<bool(UWaterBodyComponent*)> Pred) const;

	/** Execute a predicate function on each valid water body. Predicate should return false for early exit. */
	static void ForEachWaterBodyComponent (const UWorld* World, TFunctionRef<bool(UWaterBodyComponent*)> Pred);

	void ForEachWaterZone(TFunctionRef<bool(AWaterZone*)> Pred) const;
	static void ForEachWaterZone(const UWorld* World, TFunctionRef<bool(AWaterZone*)> Pred);

	bool HasAnyWaterBodies() const { return WaterBodyComponents.Num > 0; }

	int32 NumWaterBodies() const { return WaterBodyComponents.Num; }

	int32 NumWaterZones() const { return WaterZones.Num; }

	FWaterViewExtension* GetWaterViewExtension() { return WaterViewExtension.Get(); }

private:

	/**
	* TWaterContainer<T> wraps a TArray<T> to support reusing dead indices while always maintaining stability for existing indices.
	*
	* The Elements array may contain nullptr entries.
	*/
	template <typename T>
	class TWaterContainer 
	{
	public:
		int32 Register(T* InElement)
		{
			int32 Index = INDEX_NONE;
			if (UnusedIndices.Num())
			{
				Index = UnusedIndices.Pop(/*bAllowShrinking = */false);
				check(Elements[Index] == nullptr);
				Elements[Index] = InElement;
			}
			else
			{
				Index = Elements.Add(InElement);
			}

			++Num;
			check(Num <= Elements.Num());

			check(Index != INDEX_NONE);
			return Index;
		}

		void Unregister(const T* InElement, int32 OldIndex)
		{
			check(OldIndex != INDEX_NONE);
			UnusedIndices.Add(OldIndex);
			Elements[OldIndex] = nullptr;

			--Num;
			check(Num >= 0);

			// Empty all arrays once there are no more elements
			if (UnusedIndices.Num() == Elements.Num())
			{
				UnusedIndices.Empty();
				Elements.Empty();
			}
		}

		TArray<T*> Elements;
		TArray<int32> UnusedIndices;
		int32 Num = 0;
	};

	/** List of components registered to this manager. */
	TWaterContainer<UWaterBodyComponent> WaterBodyComponents;

	/** List of Water zones registered to this manager. */
	TWaterContainer<AWaterZone> WaterZones;

	float GlobalMaxWaveHeight = 0.0f;

	TSharedPtr<FWaterViewExtension, ESPMode::ThreadSafe> WaterViewExtension;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
