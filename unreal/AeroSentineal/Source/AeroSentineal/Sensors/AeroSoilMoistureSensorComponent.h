// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sensors/AeroSensorComponent.h"
#include "AeroSoilMoistureSensorComponent.generated.h"

/**
 * Estimates volumetric soil moisture from accumulated rainfall and live flood state.
 *
 * This is a deterministic environmental model, not a claim of a physical soil solver.
 */
UCLASS(ClassGroup = (Aero), meta = (BlueprintSpawnableComponent))
class AEROSENTINEAL_API UAeroSoilMoistureSensorComponent : public UAeroSensorComponent
{
	GENERATED_BODY()

public:
	UAeroSoilMoistureSensorComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DrySoilFraction = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RainSaturationGainPerMm = 0.012f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	float SensorBiasFraction = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	float SensorNoiseSigmaFraction = 0.025f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DropoutProbability = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	bool bWriteRecordsToDisk = true;

protected:
	virtual void CaptureSample(double SimulationSeconds) override;
};
