// Copyright AeroSentinel. All Rights Reserved.

#include "Sensors/AeroRainfallSensorComponent.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sensors/AeroRecordWriter.h"
#include "World/AeroWeatherDirector.h"

namespace
{
	constexpr int32 SaltNoise = 22;
	constexpr int32 SaltDropout = 93;
}

UAeroRainfallSensorComponent::UAeroRainfallSensorComponent()
{
	SensorName = TEXT("Rainfall");
	CaptureIntervalSimSeconds = 5.0f;
}

void UAeroRainfallSensorComponent::CaptureSample(double SimulationSeconds)
{
	TSharedRef<FJsonObject> Root = AeroRecord::BuildBaseRecord(this, SimulationSeconds);

	float GroundTruthRainfall = 0.0f;
	bool bHasWeatherDirector = false;
	if (const UWorld* World = GetWorld())
	{
		if (const AAeroWeatherDirector* Weather = Cast<AAeroWeatherDirector>(
			UGameplayStatics::GetActorOfClass(World, AAeroWeatherDirector::StaticClass())))
		{
			GroundTruthRainfall = FMath::Max(0.0f, Weather->GetWeatherState().PrecipitationMmPerHour);
			bHasWeatherDirector = true;
		}
	}

	const bool bDropped = AeroRecord::DeterministicFraction(SampleCount, SimulationSeconds, SaltDropout) < DropoutProbability;
	TSharedRef<FJsonObject> Rain = MakeShared<FJsonObject>();
	Rain->SetBoolField(TEXT("dropout"), bDropped);
	Rain->SetBoolField(TEXT("weather_director_available"), bHasWeatherDirector);
	Rain->SetNumberField(TEXT("rainfall_mm_per_hour_ground_truth"), GroundTruthRainfall);
	Rain->SetNumberField(TEXT("sensor_bias_mm_per_hour"), SensorBiasMmPerHour);
	Rain->SetNumberField(TEXT("sensor_sigma_mm_per_hour"), SensorNoiseSigmaMmPerHour);
	Rain->SetNumberField(TEXT("dropout_probability"), DropoutProbability);
	if (!bDropped)
	{
		const float MeasuredRainfall = FMath::Max(0.0f,
			GroundTruthRainfall + SensorBiasMmPerHour
			+ AeroRecord::GaussianSample(SampleCount, SimulationSeconds, SaltNoise) * SensorNoiseSigmaMmPerHour);
		Rain->SetNumberField(TEXT("rainfall_mm_per_hour_measured"), MeasuredRainfall);
	}

	Root->SetObjectField(TEXT("rainfall"), Rain);

	if (bWriteRecordsToDisk)
	{
		AeroRecord::SaveRecordAsync(Root, AeroRecord::BuildSensorDirectory(this), AeroRecord::BuildBaseFileName(this));
	}

	RegisterSample(SimulationSeconds);
}
