// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/AeroTypes.h"
#include "AeroDisasterActor.generated.h"

class USceneComponent;

/**
 * One evolving hazard instance.
 *
 * The life cycle is a pure function of the simulation clock, never of frame time or of a timer:
 *
 *   Idle -> Triggered -> Developing -> Peak -> Receding -> Terminated
 *
 * so the same scenario seed replayed at 1x and at 60x produces the same severity at the same
 * simulated instant. That is the whole point of a ground-truth generator: a record written at
 * sim_seconds 400 must carry the same label in every run.
 *
 * An instance is two things at once and they are deliberately separated:
 *   - a physical footprint (AffectedRadiusCm, and a height/position the derived class owns), which
 *     the sensors can see rendered;
 *   - a truth block (GetGroundTruthAtLocation / GetEnvironmentalDeltaAt), which is what the dataset
 *     is labelled with and what the virtual sensors measure. The truth is produced here, not
 *     inferred from the rendered pixels, so a smoke plume that is culled for performance still
 *     reports the PM2.5 a real sensor would have read.
 *
 * Times are relative to the containing scenario's start (UAeroDisasterSubsystem), so resetting the
 * scenario for a new reinforcement-learning episode restarts every instance in step.
 */
UCLASS(Abstract, Blueprintable)
class AEROSENTINEAL_API AAeroDisasterActor : public AActor
{
	GENERATED_BODY()

public:
	AAeroDisasterActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// ---------------------------------------------------------------- identity

	/** Unique id stamped into every record this instance labels. Defaults to the actor name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster")
	FString InstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Disaster")
	EAeroDisasterType DisasterType = EAeroDisasterType::None;

	// ---------------------------------------------------------------- life cycle, in SIMULATION seconds

	/** Sim seconds after the scenario start before this instance triggers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster")
	float TriggerDelaySimSeconds = 0.0f;

	/** Length of the Triggered phase: the hazard exists but has barely any expression yet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster")
	float TriggerDurationSimSeconds = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster")
	float DevelopDurationSimSeconds = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster")
	float PeakDurationSimSeconds = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster")
	float RecedeDurationSimSeconds = 180.0f;

	/** Severity held during the Peak phase, 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PeakSeverity = 1.0f;

	// ---------------------------------------------------------------- footprint

	/** Footprint radius the instant it triggers, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster")
	float InitialRadiusCm = 1500.0f;

	/** Footprint radius at full severity, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster")
	float MaxRadiusCm = 9000.0f;

	// ---------------------------------------------------------------- live state

	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	EAeroDisasterPhase Phase = EAeroDisasterPhase::Idle;

	/** PeakSeverity scaled by the position in the life cycle. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	float Severity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	float AffectedRadiusCm = 0.0f;

	/** Sim seconds since the instance triggered. Negative before it triggers. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	double ElapsedSimSeconds = 0.0;

	UFUNCTION(BlueprintPure, Category = "Aero|Disaster")
	FVector GetOrigin() const { return Origin; }

	/** 0..1 progress through the event, before PeakSeverity is applied. */
	UFUNCTION(BlueprintPure, Category = "Aero|Disaster")
	float GetSeverityFraction() const { return SeverityFraction; }

	/** Flattened copy of the live state, for queries and for the ground-truth timeline. */
	UFUNCTION(BlueprintPure, Category = "Aero|Disaster")
	FAeroDisasterInstance MakeInstanceSnapshot() const;

	/**
	 * Centre of the live footprint, cm. The base hazard never moves; a landslide overrides this with
	 * the position its debris front has actually reached, so the truth follows the mass.
	 */
	virtual FVector GetFootprintCentre() const;

	/** Truth this instance contributes at a world point. DisasterType is None outside its footprint. */
	virtual FAeroGroundTruth GetGroundTruthAtLocation(const FVector& WorldLocation) const;

	/** Atmosphere this instance adds at a world point. Zero outside its influence. */
	virtual FAeroEnvironmentalDelta GetEnvironmentalDeltaAt(const FVector& WorldLocation) const;

	/** Push the life cycle back to Idle so the instance can serve a fresh episode. */
	virtual void ResetForEpisode();

protected:
	/** Move the geometry and lights to match Phase/Severity. Runs every tick while the actor exists. */
	virtual void ApplyDisasterState();

	/** 1 at the footprint centre, 0 at its edge, 0 outside. Ignores PeakSeverity. */
	float RadialFalloff(const FVector& WorldLocation) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Disaster")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Spawn location, cached once: the actor itself never moves. */
	FVector Origin = FVector::ZeroVector;

	/** 0..1 progress through the event, before PeakSeverity is applied. */
	float SeverityFraction = 0.0f;

	/** Scenario origin in simulation seconds, re-read each tick because a new episode moves it. */
	double ScenarioStartSimSeconds = 0.0;

private:
	/** Advances Phase / Severity / AffectedRadiusCm from the simulation clock. */
	void UpdateLifecycle();

	/** Last phase written to the log, so a transition is reported once rather than every tick. */
	EAeroDisasterPhase LoggedPhase = EAeroDisasterPhase::Idle;
};
