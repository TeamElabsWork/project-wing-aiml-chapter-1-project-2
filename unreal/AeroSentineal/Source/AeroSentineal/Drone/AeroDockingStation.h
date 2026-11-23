// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AeroDockingStation.generated.h"

class UStaticMeshComponent;
class AAeroDronePawn;

/**
 * Home base for one airframe: a pad the drone takes off from and lands back on.
 *
 * Owns the spawn and the mission hand-off so a level only has to contain a station, not a drone.
 * The pad is deliberately collision-free: in a training environment nothing should be able to bump
 * the airframe off its dock between runs.
 */
UCLASS(Blueprintable)
class AEROSENTINEAL_API AAeroDockingStation : public AActor
{
	GENERATED_BODY()

public:
	AAeroDockingStation();

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Spawn the airframe on the pad and register it. Returns the existing drone if already placed. */
	UFUNCTION(BlueprintCallable, Category = "Aero|Dock")
	AAeroDronePawn* SpawnDrone();

	/** World transform the drone is spawned at: pad centre, half a pad above the deck. */
	UFUNCTION(BlueprintPure, Category = "Aero|Dock")
	FTransform GetDroneSpawnTransform() const;

	/** Airframe class to deploy. Falls back to AAeroDronePawn when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Dock")
	TSubclassOf<AAeroDronePawn> DroneClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Dock")
	TObjectPtr<UStaticMeshComponent> Pad;

	/** Half-extents of the pad deck, cm. Drives the visual scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Dock")
	FVector PadExtent = FVector(300.0f, 300.0f, 20.0f);

	/** Deploy an airframe as soon as the level starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Dock")
	bool bSpawnDroneOnBeginPlay = true;

	/** Hand the spawned drone to the local player so PIE looks through its gimbal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Dock")
	bool bPossessSpawnedDrone = true;

	/** Start the mission (spin-up, climb, patrol) immediately after spawning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Dock")
	bool bLaunchMissionOnBeginPlay = true;

	UPROPERTY(BlueprintReadOnly, Category = "Aero|Dock")
	TObjectPtr<AAeroDronePawn> DockedDrone;

protected:
	/** Keep the pad mesh scaled to PadExtent when the value is edited in the level. */
	void ApplyPadScale();
};
