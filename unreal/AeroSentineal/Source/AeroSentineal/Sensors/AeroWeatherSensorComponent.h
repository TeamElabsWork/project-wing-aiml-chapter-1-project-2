// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sensors/AeroSensorComponent.h"
#include "AeroWeatherSensorComponent.generated.h"

/**
 * Weather channel: the atmospheric state at the drone's position.
 *
 * Ground truth comes from the level's AAeroWeatherDirector, which is the same authority that drives the
 * sun, sky light and fog - so a record can never disagree with what the renderer was doing at that
 * instant. Each channel is then reported twice: the truth, and a measurement carrying a fixed bias plus
 * gaussian noise, with an occasional dropout. A model trained on this learns the sensor, not a perfect
 * world.
 *
 * Writes a JSON record per sample and no image; the record shares its header with the camera channels,
 * so a frame and a reading from the same sample index join directly.
 */
UCLASS(ClassGroup = (Aero), meta = (BlueprintSpawnableComponent))
class AEROSENTINEAL_API UAeroWeatherSensorComponent : public UAeroSensorComponent
{
	GENERATED_BODY()

public:
	UAeroWeatherSensorComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	float TemperatureBiasCelsius = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	float TemperatureSigmaCelsius = 0.40f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	float WindSigmaMps = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	float PrecipitationSigmaMmPerHour = 0.05f;

	/** Relative sigma for visibility, which is multiplicative rather than additive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	float VisibilityRelativeSigma = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DropoutProbability = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	bool bWriteRecordsToDisk = true;

	UFUNCTION(BlueprintPure, Category = "Aero|Sensor")
	float GetLastMeasuredTemperatureC() const { return LastMeasuredTemperatureC; }

protected:
	virtual void CaptureSample(double SimulationSeconds) override;

	mutable float LastMeasuredTemperatureC = 0.0f;
};
