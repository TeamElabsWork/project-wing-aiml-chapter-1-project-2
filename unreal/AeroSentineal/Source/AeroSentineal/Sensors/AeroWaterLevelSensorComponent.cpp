// Copyright AeroSentinel. All Rights Reserved.

#include "Sensors/AeroWaterLevelSensorComponent.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Disaster/AeroDisasterSubsystem.h"
#include "Sensors/AeroRecordWriter.h"

namespace
{
	constexpr int32 SaltNoise = 21;
	constexpr int32 SaltDropout = 92;
}

UAeroWaterLevelSensorComponent::UAeroWaterLevelSensorComponent()
{
	SensorName = TEXT("WaterLevel");
	CaptureIntervalSimSeconds = 5.0f;
}

void UAeroWaterLevelSensorComponent::CaptureSample(double SimulationSeconds)
{
	TSharedRef<FJsonObject> Root = AeroRecord::BuildBaseRecord(this, SimulationSeconds);

	FAeroGroundTruth GroundTruth;
	if (const UWorld* World = GetWorld())
	{
		if (const UAeroDisasterSubsystem* Disasters = World->GetSubsystem<UAeroDisasterSubsystem>())
		{
			Disasters->GetGroundTruthAtLocation(GetComponentLocation(), GroundTruth);
		}
	}

	const float GroundTruthDepthCm = FMath::Max(0.0f, GroundTruth.WaterDepthCm);
	const bool bDropped = AeroRecord::DeterministicFraction(SampleCount, SimulationSeconds, SaltDropout) < DropoutProbability;

	TSharedRef<FJsonObject> Water = MakeShared<FJsonObject>();
	Water->SetBoolField(TEXT("dropout"), bDropped);
	Water->SetNumberField(TEXT("water_depth_cm_ground_truth"), GroundTruthDepthCm);
	Water->SetNumberField(TEXT("sensor_bias_cm"), SensorBiasCm);
	Water->SetNumberField(TEXT("sensor_sigma_cm"), SensorNoiseSigmaCm);
	Water->SetNumberField(TEXT("dropout_probability"), DropoutProbability);
	if (!bDropped)
	{
		const float MeasuredDepthCm = FMath::Max(0.0f,
			GroundTruthDepthCm + SensorBiasCm
			+ AeroRecord::GaussianSample(SampleCount, SimulationSeconds, SaltNoise) * SensorNoiseSigmaCm);
		Water->SetNumberField(TEXT("water_depth_cm_measured"), MeasuredDepthCm);
		Water->SetNumberField(TEXT("confidence"), FMath::Clamp(1.0f - DropoutProbability, 0.0f, 1.0f));
	}

	Root->SetObjectField(TEXT("water_level"), Water);

	if (bWriteRecordsToDisk)
	{
		AeroRecord::SaveRecordAsync(Root, AeroRecord::BuildSensorDirectory(this), AeroRecord::BuildBaseFileName(this));
	}

	RegisterSample(SimulationSeconds);
}
