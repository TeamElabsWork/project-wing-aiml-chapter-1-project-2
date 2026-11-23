// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sensors/AeroRgbCameraComponent.h"
#include "AeroThermalCameraComponent.generated.h"

/**
 * Simulated thermal channel.
 *
 * Shares the RGB capture pipeline (same cadence, same annotation writer) and differs in two ways:
 *
 *  1. The image. After the scene capture runs, the frame's pixels are read back and each pixel's
 *     luminance is mapped onto a cold/mid/hot palette, so the exported PNG reads as a false-colour
 *     thermal frame. The mapping is done here rather than through a post-process material because a
 *     post-process blendable is not reliably present in what a scene capture records - it has to
 *     survive every capture-source permutation, and a CPU remap cannot be bypassed.
 *
 *     This is a FALSE-COLOUR mapping, not a radiometric one: per-pixel temperature needs the Phase 4
 *     disaster model to supply real surface temperatures. Until then the image conveys contrast,
 *     not degrees.
 *
 *  2. The reading. Every sample records ambient air temperature as ground truth (from the weather
 *     director) plus a simulated measurement: ground truth + a fixed sensor bias + gaussian noise,
 *     with an occasional dropout. The noise is seeded from the sample index and the simulation
 *     instant, so a run reproduces exactly - which is what makes a measured/ground-truth pair
 *     usable as a training label.
 */
UCLASS(ClassGroup = (Aero), meta = (BlueprintSpawnableComponent))
class AEROSENTINEAL_API UAeroThermalCameraComponent : public UAeroRgbCameraComponent
{
	GENERATED_BODY()

public:
	UAeroThermalCameraComponent();

	/** Palette entry for the coldest end of the range (linear). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	FLinearColor ColdColor = FLinearColor(0.00335f, 0.01298f, 0.10462f, 1.0f);

	/** Palette entry for mid luminance (linear). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	FLinearColor MidColor = FLinearColor(0.2706f, 0.0242f, 0.1471f, 1.0f);

	/** Palette entry for the hottest end of the range (linear). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	FLinearColor HotColor = FLinearColor(1.0f, 0.8961f, 0.3515f, 1.0f);

	/** Constant offset of the simulated sensor reading, Celsius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	float SensorBiasCelsius = 0.25f;

	/** Standard deviation of the simulated sensor noise, Celsius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	float SensorNoiseSigmaCelsius = 0.8f;

	/** Probability that a sample is lost entirely, 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DropoutProbability = 0.02f;

	/** Most recent non-dropped ambient reading, Celsius. */
	UFUNCTION(BlueprintPure, Category = "Aero|Sensor")
	float GetLastMeasuredAmbientC() const { return LastMeasuredAmbientC; }

protected:
	virtual void WriteFrameToDisk(const FString& Directory, const FString& BaseFileName) override;

	virtual void AddSensorFields(TSharedRef<class FJsonObject> Root, double SimulationSeconds) const override;

	/** Luminance 0..1 -> palette colour. */
	FLinearColor MapLuminanceToPalette(float Luminance) const;

	mutable float LastMeasuredAmbientC = 0.0f;
};
