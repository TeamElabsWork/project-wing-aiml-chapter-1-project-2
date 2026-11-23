// Copyright AeroSentinel. All Rights Reserved.

#include "Sensors/AeroRgbCameraComponent.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Dom/JsonObject.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Core/AeroLog.h"
#include "Sensors/AeroRecordWriter.h"

UAeroRgbCameraComponent::UAeroRgbCameraComponent()
{
	SensorName = TEXT("RGB");
	CaptureIntervalSimSeconds = 5.0f;

	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
	SceneCapture->SetupAttachment(this);
	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	SceneCapture->FOVAngle = FOVAngle;
}

void UAeroRgbCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	RenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("RT_Rgb"));
	if (RenderTarget)
	{
		RenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
		RenderTarget->ClearColor = FLinearColor::Black;
		RenderTarget->InitAutoFormat(ResolutionX, ResolutionY);
		RenderTarget->UpdateResourceImmediate(true);
	}

	if (SceneCapture)
	{
		SceneCapture->TextureTarget = RenderTarget;
		SceneCapture->FOVAngle = FOVAngle;
	}

	UE_LOG(LogAeroSentinel, Log, TEXT("Camera sensor %s ready: %dx%d, %.0f deg, writing=%s"),
		*GetName(), ResolutionX, ResolutionY, FOVAngle, bWriteFramesToDisk ? TEXT("yes") : TEXT("no"));
}

void UAeroRgbCameraComponent::CaptureSample(double SimulationSeconds)
{
	if (!SceneCapture || !RenderTarget)
	{
		return;
	}

	SceneCapture->CaptureScene();

	const FString Directory = BuildFrameDirectory();
	const FString BaseFileName = AeroRecord::BuildBaseFileName(this);

	if (bWriteFramesToDisk || bWriteAnnotations)
	{
		IFileManager::Get().MakeDirectory(*Directory, true);
	}

	if (bWriteFramesToDisk)
	{
		WriteFrameToDisk(Directory, BaseFileName);
	}

	if (bWriteAnnotations)
	{
		WriteFrameAnnotation(Directory, BaseFileName, SimulationSeconds);
	}

	RegisterSample(SimulationSeconds);
}

void UAeroRgbCameraComponent::WriteFrameAnnotation(const FString& Directory, const FString& BaseFileName, double SimulationSeconds)
{
	// Identity, timing, pose and region all come from the shared writer, so this record joins cleanly
	// against the scalar sensors sampled at the same instant.
	TSharedRef<FJsonObject> Root = AeroRecord::BuildBaseRecord(this, SimulationSeconds);
	Root->SetStringField(TEXT("frame_file"), BaseFileName + TEXT(".png"));

	TSharedRef<FJsonObject> Camera = MakeShared<FJsonObject>();
	Camera->SetNumberField(TEXT("width"), ResolutionX);
	Camera->SetNumberField(TEXT("height"), ResolutionY);
	Camera->SetNumberField(TEXT("fov_deg"), FOVAngle);
	Root->SetObjectField(TEXT("camera"), Camera);

	AddSensorFields(Root, SimulationSeconds);

	AeroRecord::SaveRecord(Root, Directory, BaseFileName);
}

void UAeroRgbCameraComponent::WriteFrameToDisk(const FString& Directory, const FString& BaseFileName)
{
	UKismetRenderingLibrary::ExportRenderTarget(this, RenderTarget, Directory, BaseFileName + TEXT(".png"));
}

FString UAeroRgbCameraComponent::BuildFrameDirectory() const
{
	return AeroRecord::BuildSensorDirectory(this);
}
