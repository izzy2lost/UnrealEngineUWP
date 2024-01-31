// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "AvalanchePlaylistComponent.generated.h"

class UAvalanchePlaylist;

/**
 * Add this actor component to blueprint actor to expose the API to control an
 * Motion Design playlist in game.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Motion Design Media", meta = (BlueprintSpawnableComponent))
class AVALANCHEMEDIA_API UAvalanchePlaylistComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAvalanchePlaylistComponent(const FObjectInitializer& ObjectInitializer);
	
	UPROPERTY(EditAnywhere, Category = "Motion Design Media", meta = (DisplayName = "Motion Design Playlist"))
	TObjectPtr<UAvalanchePlaylist> Playlist = nullptr;
	
	UFUNCTION(BlueprintCallable, Category = "Motion Design Media")
	bool PlayPage(int32 InPageId);

	UFUNCTION(BlueprintCallable, Category = "Motion Design Media")
	bool StopPage(int32 InPageId);

	UFUNCTION(BlueprintCallable, Category = "Motion Design Media")
	int32 GetNumberOfPages() const;
	
	UFUNCTION(BlueprintCallable, Category = "Motion Design Media")
	int32 GetPageIdForIndex(int32 InPageIndex) const;

	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;

protected:
	void OnWorldBeginTearDown(UWorld* InWorld);
};
