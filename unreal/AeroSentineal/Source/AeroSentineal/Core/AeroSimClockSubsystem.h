// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Stats/Stats.h"
#include "AeroSimClockSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAeroSimTimeAdvanced, double, SimulationSeconds, float, DeltaSimSeconds);

/**
 * The single authoritative simulation clock.
 *
 * Wall-clock time is deliberately NOT used anywhere else in the simulation: every timestamp in
 * every dataset row is derived from SimulationSeconds, so an accelerated run (1 real second =
 * 60 simulation seconds, or far more) produces a self-consistent UTC timeline, and a headless
 * batch run produces the same records with a different number of rendered frames.
 *
 * SimulationUtc = Settings->GetStartUtc() + SimulationSeconds
 */
UCLASS()
class AEROSENTINEAL_API UAeroSimClockSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin UWorldSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem

	//~ Begin FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject

	// ------------------------------------------------------------- Control

	/** Sim seconds advanced per real second. Takes effect on the next tick. */
	UFUNCTION(BlueprintCallable, Category = "AeroSentinel|Time")
	void SetTimeMultiplier(float NewMultiplier);

	UFUNCTION(BlueprintPure, Category = "AeroSentinel|Time")
	float GetTimeMultiplier() const { return TimeMultiplier; }

	UFUNCTION(BlueprintCallable, Category = "AeroSentinel|Time")
	void SetPaused(bool bNewPaused);

	UFUNCTION(BlueprintPure, Category = "AeroSentinel|Time")
	bool IsPaused() const { return bPaused; }

	/** Returns the clock to zero and re-reads the configured start instant. */
	UFUNCTION(BlueprintCallable, Category = "AeroSentinel|Time")
	void ResetSimulationTime();

	/**
	 * Advance the clock by an explicit amount of simulation time, ignoring real frame time.
	 * Used by the long-horizon batch generator (B2) to walk months of simulated history without
	 * rendering every frame. Still respects pause.
	 */
	UFUNCTION(BlueprintCallable, Category = "AeroSentinel|Time")
	void AdvanceSimulationTime(double DeltaSimSeconds);

	// ------------------------------------------------------------- Queries

	UFUNCTION(BlueprintPure, Category = "AeroSentinel|Time")
	double GetSimulationSeconds() const { return SimulationSeconds; }

	/** Simulated UTC instant, derived from SimulationSeconds. */
	UFUNCTION(BlueprintPure, Category = "AeroSentinel|Time")
	FDateTime GetSimulationUtc() const;

	/** Simulated UTC instant as ISO-8601 with millisecond precision, the dataset timestamp format. */
	UFUNCTION(BlueprintPure, Category = "AeroSentinel|Time")
	FString GetSimulationUtcIso8601() const;

	/** Broadcast after every time advance (tick or manual step). */
	UPROPERTY(BlueprintAssignable, Category = "AeroSentinel|Time")
	FAeroSimTimeAdvanced OnTimeAdvanced;

private:
	/** Total elapsed simulation seconds since the clock was reset. */
	UPROPERTY()
	double SimulationSeconds = 0.0;

	/** Sim seconds per real second. */
	UPROPERTY()
	float TimeMultiplier = 1.0f;

	UPROPERTY()
	bool bPaused = false;

	/** Cached from settings on Initialize/Reset so timestamp queries stay allocation-free. */
	FDateTime StartUtc;
};
