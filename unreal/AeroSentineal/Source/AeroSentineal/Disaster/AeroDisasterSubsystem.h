// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/AeroTypes.h"
#include "AeroDisasterSubsystem.generated.h"

class AAeroDisasterActor;
class AAeroGroundTruthRecorder;
struct FRandomStream;

/**
 * The disaster scenario engine: the single authority on what hazard is happening where.
 *
 * Responsibilities, deliberately narrow:
 *   - own the set of live instances and the scenario they belong to (start instant and seed);
 *   - answer "what is the truth at this world point" for the record writer, the sensors and the RL
 *     observation;
 *   - spawn a seeded scenario so a run needs no manual actor placement and an episode can be reset.
 *
 * It does NOT simulate hazards. Each instance advances itself from the simulation clock, so there is
 * no central update order to get wrong, and an instance added by hand to a level behaves exactly like
 * a spawned one.
 *
 * Queries walk the live instance list rather than a cached spatial index: there are a handful of
 * instances, so there is no invalidation bug to have, and a query returns the same answer in the
 * editor as during play.
 */
UCLASS()
class AEROSENTINEAL_API UAeroDisasterSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin UWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	//~ End UWorldSubsystem

	// ---------------------------------------------------------------- instance registry

	/** Called by an instance on BeginPlay. Adds it to the query set; not owner of its lifetime. */
	void RegisterInstance(AAeroDisasterActor* Instance);

	/** Called by an instance on EndPlay. */
	void UnregisterInstance(AAeroDisasterActor* Instance);

	/** Snapshot of every live instance. */
	UFUNCTION(BlueprintCallable, Category = "Aero|Disaster")
	void GetInstances(TArray<FAeroDisasterInstance>& OutInstances) const;

	UFUNCTION(BlueprintPure, Category = "Aero|Disaster")
	int32 GetInstanceCount() const;

	// ---------------------------------------------------------------- truth queries

	/**
	 * Dominant hazard truth at a world point. Returns false when the point is outside every live
	 * footprint, in which case OutGroundTruth.DisasterType is None and DisasterPhase is Idle.
	 *
	 * A point can sit in more than one footprint (a fire on a floodplain); the highest severity wins,
	 * which is the label a detector should be trained on.
	 */
	UFUNCTION(BlueprintPure, Category = "Aero|Disaster")
	bool GetGroundTruthAtLocation(const FVector& WorldLocation, FAeroGroundTruth& OutGroundTruth) const;

	/** Sum of every instance's atmospheric contribution at a world point. Never fails: no hazard is zero. */
	UFUNCTION(BlueprintCallable, Category = "Aero|Disaster")
	void GetEnvironmentalDeltaAt(const FVector& WorldLocation, FAeroEnvironmentalDelta& OutDelta) const;

	/** Nearest instance that is not Idle or Terminated. */
	UFUNCTION(BlueprintPure, Category = "Aero|Disaster")
	bool FindNearestInstance(const FVector& WorldLocation, FAeroDisasterInstance& OutInstance, float& OutDistanceCm) const;

	// ---------------------------------------------------------------- scenario control

	/**
	 * Destroy any running scenario and spawn a fresh one from Seed. Every instance's trigger delay,
	 * peak severity and position derive from the seed, so the same seed always rebuilds the same
	 * scenario and consecutive reinforcement-learning episodes can be made comparable or varied.
	 */
	UFUNCTION(BlueprintCallable, Category = "Aero|Disaster")
	void StartScenario(int32 Seed);

	UFUNCTION(BlueprintCallable, Category = "Aero|Disaster")
	void StopScenario();

	/** StopScenario followed by StartScenario: the episode boundary. */
	UFUNCTION(BlueprintCallable, Category = "Aero|Disaster")
	void ResetScenario(int32 Seed);

	UFUNCTION(BlueprintPure, Category = "Aero|Disaster")
	bool IsScenarioRunning() const { return bScenarioRunning; }

	/** Simulation instant the running scenario started from, seconds. */
	UFUNCTION(BlueprintPure, Category = "Aero|Disaster")
	double GetScenarioStartSimSeconds() const { return ScenarioStartSimSeconds; }

	UFUNCTION(BlueprintPure, Category = "Aero|Disaster")
	int32 GetScenarioSeed() const { return ScenarioSeed; }

	/** Spawn the seeded scenario automatically when play begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster")
	bool bAutoStartScenario = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster")
	int32 DefaultScenarioSeed = 20260601;

	/** Spawn the ground-truth timeline recorder alongside the scenario. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster")
	bool bSpawnGroundTruthRecorder = true;

private:
	/**
	 * Destroys the spawned instance set. bDestroyRecorder is false when the caller is restarting a
	 * scenario for a new episode: the timeline recorder spans the whole training run, and destroying it
	 * would truncate the run's ground truth at every episode boundary.
	 */
	void DestroyScenarioInstances(bool bDestroyRecorder);

	/** Spawns one concrete hazard. Returns null and logs when the class cannot be created. */
	AAeroDisasterActor* SpawnOne(EAeroDisasterType Type, const FVector& SpawnOrigin, float TriggerDelay, FRandomStream& Stream);

	/**
	 * Where a hazard of this family belongs. Uses the matching zone volume when the level has one, so
	 * hazards land in their own geography; falls back to a ring around the anchor and says so.
	 */
	FVector ResolveSpawnOrigin(EAeroZoneId PreferredZone, FRandomStream& Stream, const FVector& Anchor) const;

	/** Every live instance, in registration order. */
	UPROPERTY()
	TArray<TObjectPtr<AAeroDisasterActor>> Instances;

	/** Only the instances this subsystem spawned, so StopScenario does not destroy hand-placed ones. */
	UPROPERTY()
	TArray<TObjectPtr<AAeroDisasterActor>> SpawnedInstances;

	UPROPERTY()
	TObjectPtr<AAeroGroundTruthRecorder> SpawnedRecorder;

	UPROPERTY()
	bool bScenarioRunning = false;

	UPROPERTY()
	int32 ScenarioSeed = 0;

	UPROPERTY()
	double ScenarioStartSimSeconds = 0.0;
};
