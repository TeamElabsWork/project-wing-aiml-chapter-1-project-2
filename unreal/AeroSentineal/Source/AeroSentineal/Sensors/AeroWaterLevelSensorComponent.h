// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sensors/AeroSensorComponent.h"
#include "AeroWaterLevelSensorComponent.generated.h"

/**
 * Measures standing water depth at the sensor location from live flood ground truth.
 */
UCLASS(ClassGroup = (Aero), meta = (BlueprintSpawnableComponent))
class AEROSENTINEAL_API UAeroWaterLevelSensorComponent : public UAeroSensorComponent
{
	GENERATED_BODY()

public:
	UAeroWaterLevelSensorComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	float SensorBiasCm = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	float SensorNoiseSigmaCm = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DropoutProbability = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	bool bWriteRecordsToDisk = true;

protected:
	virtual void CaptureSample(double SimulationSeconds) override;
};
