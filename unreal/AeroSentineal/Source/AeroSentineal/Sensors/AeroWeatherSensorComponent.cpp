// Copyright AeroSentinel. All Rights Reserved.

#include "Sensors/AeroWeatherSensorComponent.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Core/AeroLog.h"
#include "Core/AeroTypes.h"
#include "Disaster/AeroDisasterSubsystem.h"
#include "Sensors/AeroRecordWriter.h"
#include "World/AeroWeatherDirector.h"

namespace
{
	// Per-channel salts keep the noise of one channel independent of the next.
	constexpr int32 SaltTemperature = 1;
	constexpr int32 SaltWind = 2;
	constexpr int32 SaltPrecipitation = 3;
	constexpr int32 SaltVisibility = 4;
	constexpr int32 SaltDropout = 90;
}

UAeroWeatherSensorComponent::UAeroWeatherSensorComponent()
{
	SensorName = TEXT("Weather");
	CaptureIntervalSimSeconds = 5.0f;
}

void UAeroWeatherSensorComponent::CaptureSample(double SimulationSeconds)
{
	const UWorld* World = GetWorld();
	const AAeroWeatherDirector* Weather = World
		? Cast<AAeroWeatherDirector>(UGameplayStatics::GetActorOfClass(World, AAeroWeatherDirector::StaticClass()))
		: nullptr;

	TSharedRef<FJsonObject> Root = AeroRecord::BuildBaseRecord(this, SimulationSeconds);

	if (!Weather)
	{
		// No director in the level: say so rather than inventing an atmosphere.
		Root->SetStringField(TEXT("weather_status"), TEXT("no_weather_director"));
		UE_LOG(LogAeroSentinel, Warning, TEXT("Weather sensor %s found no AAeroWeatherDirector in the world"), *GetName());
	}
	else
	{
		const FAeroWeatherState& State = Weather->GetWeatherState();

		// Live hazards change the parcel of air the sensor is sitting in. The delta is added to the
		// ambient ground truth rather than to the reading, so measured-vs-truth stays a genuine
		// sensor-error pair instead of comparing a reading against an atmosphere that no longer holds.
		FAeroEnvironmentalDelta HazardDelta;
		if (World)
		{
			if (const UAeroDisasterSubsystem* Disasters = World->GetSubsystem<UAeroDisasterSubsystem>())
			{
				Disasters->GetEnvironmentalDeltaAt(GetComponentLocation(), HazardDelta);
			}
		}

		const float GroundTruthTemperature = State.AirTemperatureC + HazardDelta.TemperatureDeltaC;
		const float GroundTruthVisibility = FMath::Max(0.0f,
			State.VisibilityMeters * (1.0f - HazardDelta.VisibilityLossFraction));

		const bool bDropped = AeroRecord::DeterministicFraction(SampleCount, SimulationSeconds, SaltDropout) < DropoutProbability;
		const float Gaussian = AeroRecord::GaussianSample(SampleCount, SimulationSeconds, SaltTemperature);

		const float MeasuredTemperature = GroundTruthTemperature + TemperatureBiasCelsius + Gaussian * TemperatureSigmaCelsius;
		const float MeasuredWind = FMath::Max(0.0f, State.WindSpeedMps
			+ AeroRecord::GaussianSample(SampleCount, SimulationSeconds, SaltWind) * WindSigmaMps);
		const float MeasuredPrecipitation = FMath::Max(0.0f, State.PrecipitationMmPerHour
			+ AeroRecord::GaussianSample(SampleCount, SimulationSeconds, SaltPrecipitation) * PrecipitationSigmaMmPerHour);
		const float MeasuredVisibility = FMath::Max(0.0f, GroundTruthVisibility
			* (1.0f + AeroRecord::GaussianSample(SampleCount, SimulationSeconds, SaltVisibility) * VisibilityRelativeSigma));

		TSharedRef<FJsonObject> WeatherBlock = MakeShared<FJsonObject>();
		WeatherBlock->SetBoolField(TEXT("dropout"), bDropped);

		if (!bDropped)
		{
			WeatherBlock->SetNumberField(TEXT("air_temperature_c_ground_truth"), GroundTruthTemperature);
			WeatherBlock->SetNumberField(TEXT("air_temperature_c_measured"), MeasuredTemperature);
			WeatherBlock->SetNumberField(TEXT("wind_speed_mps_ground_truth"), State.WindSpeedMps);
			WeatherBlock->SetNumberField(TEXT("wind_speed_mps_measured"), MeasuredWind);
			WeatherBlock->SetNumberField(TEXT("precipitation_mm_per_hour_ground_truth"), State.PrecipitationMmPerHour);
			WeatherBlock->SetNumberField(TEXT("precipitation_mm_per_hour_measured"), MeasuredPrecipitation);
			WeatherBlock->SetNumberField(TEXT("visibility_m_ground_truth"), GroundTruthVisibility);
			WeatherBlock->SetNumberField(TEXT("visibility_m_measured"), MeasuredVisibility);
		}

		// The hazard contribution is reported separately so a record can be decomposed back into
		// "what the weather director was doing" and "what the disaster was doing".
		TSharedRef<FJsonObject> HazardBlock = MakeShared<FJsonObject>();
		HazardBlock->SetNumberField(TEXT("temperature_delta_c"), HazardDelta.TemperatureDeltaC);
		HazardBlock->SetNumberField(TEXT("visibility_loss_fraction"), HazardDelta.VisibilityLossFraction);
		WeatherBlock->SetObjectField(TEXT("hazard_delta"), HazardBlock);

		// Channels the sensor does not measure are still worth carrying as truth for the sample.
		TSharedRef<FJsonObject> NotMeasured = MakeShared<FJsonObject>();
		NotMeasured->SetNumberField(TEXT("cloud_coverage"), State.CloudCoverage);
		NotMeasured->SetNumberField(TEXT("fog_density"), State.FogDensity);
		NotMeasured->SetNumberField(TEXT("sun_intensity"), State.SunIntensity);
		NotMeasured->SetNumberField(TEXT("sky_light_intensity"), State.SkyLightIntensity);
		WeatherBlock->SetObjectField(TEXT("atmosphere_ground_truth"), NotMeasured);

		WeatherBlock->SetNumberField(TEXT("sensor_bias_c"), TemperatureBiasCelsius);
		WeatherBlock->SetNumberField(TEXT("sensor_sigma_c"), TemperatureSigmaCelsius);
		WeatherBlock->SetNumberField(TEXT("dropout_probability"), DropoutProbability);

		Root->SetObjectField(TEXT("weather"), WeatherBlock);

		if (!bDropped)
		{
			LastMeasuredTemperatureC = MeasuredTemperature;
		}
	}

	if (bWriteRecordsToDisk)
	{
		AeroRecord::SaveRecordAsync(Root, AeroRecord::BuildSensorDirectory(this), AeroRecord::BuildBaseFileName(this));
	}

	RegisterSample(SimulationSeconds);
}
