// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AeroGroundTruthRecorder.generated.h"

/**
 * Appends the disaster ground-truth timeline to disk, one JSON object per instance per interval.
 *
 * The per-record label inside a sensor record answers "what was happening where this sample was
 * taken". This timeline answers the other question a training pipeline needs: "what was the whole
 * event doing, and when". Together they let a detector be scored against the true hazard trajectory
 * rather than only against the instant it happened to photograph.
 *
 * Driven by the simulation clock, exactly like the sensors, so a 60x run and a 1x run produce the same
 * timeline in simulated time.
 */
UCLASS(Blueprintable)
class AEROSENTINEAL_API AAeroGroundTruthRecorder : public AActor
{
	GENERATED_BODY()

public:
	AAeroGroundTruthRecorder();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Seconds of SIMULATION time between timeline samples. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster", meta = (ClampMin = "0.1"))
	float SampleIntervalSimSeconds = 5.0f;

	/** Absolute path of the JSONL timeline, resolved on BeginPlay. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	FString TimelinePath;

	/** Number of JSON lines appended so far. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	int32 WrittenLineCount = 0;

	/** Append one line per live instance at the current simulation instant. */
	UFUNCTION(BlueprintCallable, Category = "Aero|Disaster")
	void WriteTimelineSample();

private:
	UFUNCTION()
	void HandleSimTimeAdvanced(double SimulationSeconds, float DeltaSimSeconds);

	/** Sim seconds accumulated since the last written sample. */
	float PendingSimSeconds = 0.0f;
};
