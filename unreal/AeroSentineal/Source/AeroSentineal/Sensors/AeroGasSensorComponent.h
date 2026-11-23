// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sensors/AeroSensorComponent.h"
#include "AeroGasSensorComponent.generated.h"

/**
 * Gas channel: CO2 and PM2.5 concentration at the drone's position.
 *
 * Ground truth is the configured ambient background. That is a modelled physical baseline, not a
 * fabricated reading: nothing emits yet, because Phase 4's disaster model is what will raise these
 * values, and until it exists the honest ground truth is the background it would be measured against.
 * Each channel is reported as truth plus a measurement carrying bias, gaussian noise and dropout.
 */
UCLASS(ClassGroup = (Aero), meta = (BlueprintSpawnableComponent))
class AEROSENTINEAL_API UAeroGasSensorComponent : public UAeroSensorComponent
{
	GENERATED_BODY()

public:
	UAeroGasSensorComponent();

	/** Background CO2 concentration, ppm. Global ambient is roughly 420 ppm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	float AmbientCo2Ppm = 420.0f;

	/** Background PM2.5 concentration, micrograms per cubic metre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	float AmbientPm25UgPerM3 = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	float Co2BiasPpm = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	float Co2SigmaPpm = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	float Pm25BiasUgPerM3 = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	float Pm25SigmaUgPerM3 = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DropoutProbability = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	bool bWriteRecordsToDisk = true;

protected:
	virtual void CaptureSample(double SimulationSeconds) override;
};
