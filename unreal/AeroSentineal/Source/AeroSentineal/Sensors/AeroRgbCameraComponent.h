// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sensors/AeroSensorComponent.h"
#include "AeroRgbCameraComponent.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

/**
 * The visible-light camera.
 *
 * Renders through a SceneCapture2D into a render target and writes each sample to
 * <DatasetRoot>/RGB/<DroneId>/ as a PNG named after the simulated UTC timestamp, so a frame on disk
 * can be matched to a telemetry row without reading any index.
 *
 * CaptureScene() is called synchronously on the clock tick: at simulation time T the image must be
 * the image of frame T, and a deferred capture would land on a later frame once the airframe has
 * already moved.
 *
 * Frame writing here is the bootstrap path. Phase 7 replaces it with UAeroDataSubsystem's async
 * image writer and this flag turns off.
 */
UCLASS(ClassGroup = (Aero), meta = (BlueprintSpawnableComponent))
class AEROSENTINEAL_API UAeroRgbCameraComponent : public UAeroSensorComponent
{
	GENERATED_BODY()

public:
	UAeroRgbCameraComponent();

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Sensor")
	TObjectPtr<USceneCaptureComponent2D> SceneCapture;

	UPROPERTY(BlueprintReadOnly, Category = "Aero|Sensor")
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor", meta = (ClampMin = "64", ClampMax = "4096"))
	int32 ResolutionX = 640;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor", meta = (ClampMin = "64", ClampMax = "4096"))
	int32 ResolutionY = 480;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor", meta = (ClampMin = "10.0", ClampMax = "170.0"))
	float FOVAngle = 60.0f;

	/** Write each sample to disk. Off once the Phase 7 data subsystem owns the write path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	bool bWriteFramesToDisk = true;

	/**
	 * Write a JSON annotation beside each frame: pose in both world and WGS84 space, camera intrinsics,
	 * sim timestamp, drone context. Without it a frame on disk is an unlabelled picture. Off once the
	 * Phase 7 data subsystem owns the write path.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Sensor")
	bool bWriteAnnotations = true;

	UFUNCTION(BlueprintPure, Category = "Aero|Sensor")
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

protected:
	virtual void CaptureSample(double SimulationSeconds) override;

	FString BuildFrameDirectory() const;

	/** Produce the frame file for one sample. Subclasses may replace the image (the thermal channel does). */
	virtual void WriteFrameToDisk(const FString& Directory, const FString& BaseFileName);

	/** Sidecar for one frame, written next to the PNG as <BaseFileName>.json. */
	void WriteFrameAnnotation(const FString& Directory, const FString& BaseFileName, double SimulationSeconds);

	/** Subclasses add their own measurement block to the frame record. */
	virtual void AddSensorFields(TSharedRef<class FJsonObject> Root, double SimulationSeconds) const {}

};
