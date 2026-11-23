// Copyright AeroSentinel. All Rights Reserved.

#include "Sensors/AeroSoilMoistureSensorComponent.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Disaster/AeroDisasterSubsystem.h"
#include "Sensors/AeroRecordWriter.h"
#include "World/AeroWeatherDirector.h"

namespace
{
	constexpr int32 SaltNoise = 23;
	constexpr int32 SaltDropout = 94;
}

UAeroSoilMoistureSensorComponent::UAeroSoilMoistureSensorComponent()
{
	SensorName = TEXT("SoilMoisture");
	CaptureIntervalSimSeconds = 5.0f;
}

void UAeroSoilMoistureSensorComponent::CaptureSample(double SimulationSeconds)
{
	TSharedRef<FJsonObject> Root = AeroRecord::BuildBaseRecord(this, SimulationSeconds);

	float RainfallMmPerHour = 0.0f;
	if (const UWorld* World = GetWorld())
	{
		if (const AAeroWeatherDirector* Weather = Cast<AAeroWeatherDirector>(
			UGameplayStatics::GetActorOfClass(World, AAeroWeatherDirector::StaticClass())))
		{
			RainfallMmPerHour = FMath::Max(0.0f, Weather->GetWeatherState().PrecipitationMmPerHour);
		}
	}

	FAeroGroundTruth FloodTruth;
	if (const UWorld* World = GetWorld())
	{
		if (const UAeroDisasterSubsystem* Disasters = World->GetSubsystem<UAeroDisasterSubsystem>())
		{
			Disasters->GetGroundTruthAtLocation(GetComponentLocation(), FloodTruth);
		}
	}

	const float FloodContribution = FMath::Clamp(FloodTruth.WaterDepthCm / 1000.0f, 0.0f, 1.0f);
	const float GroundTruthMoisture = FMath::Clamp(
		DrySoilFraction + RainfallMmPerHour * RainSaturationGainPerMm + FloodContribution * 0.5f,
		0.0f, 1.0f);
	const bool bDropped = AeroRecord::DeterministicFraction(SampleCount, SimulationSeconds, SaltDropout) < DropoutProbability;

	TSharedRef<FJsonObject> Soil = MakeShared<FJsonObject>();
	Soil->SetBoolField(TEXT("dropout"), bDropped);
	Soil->SetNumberField(TEXT("soil_moisture_fraction_ground_truth"), GroundTruthMoisture);
	Soil->SetNumberField(TEXT("rainfall_mm_per_hour_ground_truth"), RainfallMmPerHour);
	Soil->SetNumberField(TEXT("flood_water_depth_cm_ground_truth"), FloodTruth.WaterDepthCm);
	Soil->SetNumberField(TEXT("sensor_bias_fraction"), SensorBiasFraction);
	Soil->SetNumberField(TEXT("sensor_sigma_fraction"), SensorNoiseSigmaFraction);
	Soil->SetNumberField(TEXT("dropout_probability"), DropoutProbability);
	if (!bDropped)
	{
		const float MeasuredMoisture = FMath::Clamp(
			GroundTruthMoisture + SensorBiasFraction
			+ AeroRecord::GaussianSample(SampleCount, SimulationSeconds, SaltNoise) * SensorNoiseSigmaFraction,
			0.0f, 1.0f);
		Soil->SetNumberField(TEXT("soil_moisture_fraction_measured"), MeasuredMoisture);
	}

	Root->SetObjectField(TEXT("soil_moisture"), Soil);

	if (bWriteRecordsToDisk)
	{
		AeroRecord::SaveRecordAsync(Root, AeroRecord::BuildSensorDirectory(this), AeroRecord::BuildBaseFileName(this));
	}

	RegisterSample(SimulationSeconds);
}
