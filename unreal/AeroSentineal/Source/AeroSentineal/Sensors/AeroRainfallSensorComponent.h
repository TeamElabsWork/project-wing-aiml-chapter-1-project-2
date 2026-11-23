// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sensors/AeroSensorComponent.h"
#include "AeroRainfallSensorComponent.generated.h"

/**
 * Measures precipitation intensity from the active weather director.
 */
UCLASS(ClassGroup = (Aero), meta = (BlueprintSpawnableComponent))
class AEROSENTINEAL_API UAeroRainfallSensorComponent : public UAeroSensorComponent
{
	GENERATED_BODY()

public:
	UAeroRainfallSensorComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	float SensorBiasMmPerHour = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	float SensorNoiseSigmaMmPerHour = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DropoutProbability = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	bool bWriteRecordsToDisk = true;

protected:
	virtual void CaptureSample(double SimulationSeconds) override;
};
