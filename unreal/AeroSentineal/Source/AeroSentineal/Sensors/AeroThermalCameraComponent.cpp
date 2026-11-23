// Copyright AeroSentinel. All Rights Reserved.

#include "Sensors/AeroThermalCameraComponent.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Core/AeroLog.h"
#include "Sensors/AeroRecordWriter.h"
#include "World/AeroWeatherDirector.h"

UAeroThermalCameraComponent::UAeroThermalCameraComponent()
{
	SensorName = TEXT("Thermal");

	// Same cadence as the RGB channel so a thermal frame and an RGB frame of the same instant pair up.
	CaptureIntervalSimSeconds = 5.0f;

	ResolutionX = 320;
	ResolutionY = 240;
}

FLinearColor UAeroThermalCameraComponent::MapLuminanceToPalette(float Luminance) const
{
	const float Clamped = FMath::Clamp(Luminance, 0.0f, 1.0f);
	return Clamped < 0.5f
		? FMath::Lerp(ColdColor, MidColor, Clamped * 2.0f)
		: FMath::Lerp(MidColor, HotColor, (Clamped - 0.5f) * 2.0f);
}

void UAeroThermalCameraComponent::WriteFrameToDisk(const FString& Directory, const FString& BaseFileName)
{
	if (!RenderTarget)
	{
		return;
	}

	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!Resource)
	{
		UE_LOG(LogAeroSentinel, Warning, TEXT("Thermal camera %s has no render target resource"), *GetName());
		return;
	}

	const int32 Width = RenderTarget->SizeX;
	const int32 Height = RenderTarget->SizeY;

	TArray<FColor> SourcePixels;
	if (!Resource->ReadPixels(SourcePixels) || SourcePixels.Num() < Width * Height)
	{
		UE_LOG(LogAeroSentinel, Error, TEXT("Thermal camera %s could not read back its %dx%d frame"), *GetName(), Width, Height);
		return;
	}

	// Rec.601 luma over the sRGB bytes, then the same cold -> mid -> hot ramp the palette describes.
	TArray<FColor> RampedPixels;
	RampedPixels.SetNumUninitialized(Width * Height);
	for (int32 Index = 0; Index < Width * Height; ++Index)
	{
		const FColor& Source = SourcePixels[Index];
		const float Luminance = (0.299f * Source.R + 0.587f * Source.G + 0.114f * Source.B) / 255.0f;
		RampedPixels[Index] = MapLuminanceToPalette(Luminance).ToFColor(true);
	}

	// Encoded here rather than round-tripped through a UTexture2D: ExportTexture2D needs source mip data,
	// which a transient texture does not have, so it writes an empty file.
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const TSharedPtr<IImageWrapper> Wrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!Wrapper.IsValid()
		|| !Wrapper->SetRaw(RampedPixels.GetData(), RampedPixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
	{
		UE_LOG(LogAeroSentinel, Error, TEXT("Thermal camera %s could not encode a %dx%d PNG"), *GetName(), Width, Height);
		return;
	}

	const TArray64<uint8>& PngBytes = Wrapper->GetCompressed();
	const FString FramePath = FPaths::Combine(Directory, BaseFileName + TEXT(".png"));
	if (!FFileHelper::SaveArrayToFile(PngBytes, *FramePath))
	{
		UE_LOG(LogAeroSentinel, Error, TEXT("Thermal camera %s could not write %s"), *GetName(), *FramePath);
	}
}

void UAeroThermalCameraComponent::AddSensorFields(TSharedRef<FJsonObject> Root, double SimulationSeconds) const
{
	// Ground truth: the atmosphere the airframe is actually flying through.
	float GroundTruthCelsius = -1000.0f;
	if (const UWorld* World = GetWorld())
	{
		if (const AAeroWeatherDirector* Weather = Cast<AAeroWeatherDirector>(
				UGameplayStatics::GetActorOfClass(World, AAeroWeatherDirector::StaticClass())))
		{
			GroundTruthCelsius = Weather->GetWeatherState().AirTemperatureC;
		}
	}

	// The shared deterministic draw keeps this channel's noise consistent with the others.
	const float Gaussian = AeroRecord::GaussianSample(SampleCount, SimulationSeconds, 5);
	const bool bDropped = AeroRecord::DeterministicFraction(SampleCount, SimulationSeconds, 95) < DropoutProbability;
	const float MeasuredCelsius = GroundTruthCelsius + SensorBiasCelsius + Gaussian * SensorNoiseSigmaCelsius;

	TSharedRef<FJsonObject> Thermal = MakeShared<FJsonObject>();
	Thermal->SetNumberField(TEXT("ambient_air_c_ground_truth"), GroundTruthCelsius);
	Thermal->SetBoolField(TEXT("dropout"), bDropped);
	if (!bDropped)
	{
		Thermal->SetNumberField(TEXT("ambient_air_c_measured"), MeasuredCelsius);
	}
	Thermal->SetNumberField(TEXT("sensor_bias_c"), SensorBiasCelsius);
	Thermal->SetNumberField(TEXT("sensor_sigma_c"), SensorNoiseSigmaCelsius);
	Thermal->SetNumberField(TEXT("dropout_probability"), DropoutProbability);
	Thermal->SetStringField(TEXT("image_kind"), TEXT("false_colour"));
	Root->SetObjectField(TEXT("thermal"), Thermal);

	if (!bDropped)
	{
		LastMeasuredAmbientC = MeasuredCelsius;
	}
}
