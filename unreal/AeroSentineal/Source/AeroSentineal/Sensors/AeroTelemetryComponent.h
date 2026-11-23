// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sensors/AeroSensorComponent.h"
#include "AeroTelemetryComponent.generated.h"

/**
 * Writes the airframe pose, velocity, state, and battery at a fixed simulation-time cadence.
 */
UCLASS(ClassGroup = (Aero), meta = (BlueprintSpawnableComponent))
class AEROSENTINEAL_API UAeroTelemetryComponent : public UAeroSensorComponent
{
	GENERATED_BODY()

public:
	UAeroTelemetryComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	bool bWriteRecordsToDisk = true;

protected:
	virtual void CaptureSample(double SimulationSeconds) override;
};
