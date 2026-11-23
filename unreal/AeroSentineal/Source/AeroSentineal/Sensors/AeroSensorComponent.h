// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "AeroSensorComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAeroSensorSampleSignature, int32, SampleIndex, double, SimulationSeconds);

/**
 * Base class for every virtual sensor on the airframe.
 *
 * A sensor does NOT tick. It subscribes to UAeroSimClockSubsystem::OnTimeAdvanced and accumulates
 * simulation seconds, so capture cadence is defined in simulation time and is identical whether the
 * run is real-time or accelerated 60x. Capturing on frame time would put a different number of
 * frames in the dataset depending on the machine's frame rate, which is exactly what a training set
 * must not do.
 *
 * Derived classes implement CaptureSample() and call RegisterSample() once the measurement exists;
 * the sample index and the simulation timestamp are therefore stamped in one place.
 */
UCLASS(Abstract, ClassGroup = (Aero), meta = (BlueprintSpawnableComponent))
class AEROSENTINEAL_API UAeroSensorComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UAeroSensorComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Folder name this sensor writes into under the dataset root. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	FString SensorName = TEXT("Sensor");

	/** Seconds of SIMULATION time between samples. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor", meta = (ClampMin = "0.01"))
	float CaptureIntervalSimSeconds = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	bool bSensorEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "Aero|Sensor")
	int32 SampleCount = 0;

	/** Simulation time of the most recent sample, seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Sensor")
	double LastSampleSimSeconds = 0.0;

	/** Fired after every successful sample. The data layer subscribes to this rather than polling. */
	UPROPERTY(BlueprintAssignable, Category = "Aero|Sensor")
	FAeroSensorSampleSignature OnSampleTaken;

	UFUNCTION(BlueprintCallable, Category = "Aero|Sensor")
	void SetSensorEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Aero|Sensor")
	bool IsSensorEnabled() const { return bSensorEnabled; }

protected:
	/** Called once per elapsed capture interval. Implement the measurement here. */
	virtual void CaptureSample(double SimulationSeconds) PURE_VIRTUAL(UAeroSensorComponent::CaptureSample, );

	/** Stamp and broadcast a completed sample. Derived classes call this last. */
	void RegisterSample(double SimulationSeconds);

	UFUNCTION()
	void HandleSimTimeAdvanced(double SimulationSeconds, float DeltaSimSeconds);

	/** Sim seconds accumulated since the last sample. */
	float PendingSimSeconds = 0.0f;
};
