// Copyright AeroSentinel. All Rights Reserved.

#include "Sensors/AeroGasSensorComponent.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Disaster/AeroDisasterSubsystem.h"
#include "Sensors/AeroRecordWriter.h"

namespace
{
	constexpr int32 SaltCo2 = 11;
	constexpr int32 SaltPm25 = 12;
	constexpr int32 SaltDropout = 91;
}

UAeroGasSensorComponent::UAeroGasSensorComponent()
{
	SensorName = TEXT("Gas");
	CaptureIntervalSimSeconds = 5.0f;
}

void UAeroGasSensorComponent::CaptureSample(double SimulationSeconds)
{
	TSharedRef<FJsonObject> Root = AeroRecord::BuildBaseRecord(this, SimulationSeconds);

	const bool bDropped = AeroRecord::DeterministicFraction(SampleCount, SimulationSeconds, SaltDropout) < DropoutProbability;

	// Combustion and dust from live hazards add to the configured ambient background. With no hazard in
	// range this is exactly the background, which is why the source string below distinguishes the two.
	FAeroEnvironmentalDelta HazardDelta;
	if (const UWorld* World = GetWorld())
	{
		if (const UAeroDisasterSubsystem* Disasters = World->GetSubsystem<UAeroDisasterSubsystem>())
		{
			Disasters->GetEnvironmentalDeltaAt(GetComponentLocation(), HazardDelta);
		}
	}

	const float GroundTruthCo2 = AmbientCo2Ppm + HazardDelta.Co2DeltaPpm;
	const float GroundTruthPm25 = AmbientPm25UgPerM3 + HazardDelta.Pm25DeltaUgPerM3;

	TSharedRef<FJsonObject> Gas = MakeShared<FJsonObject>();
	Gas->SetBoolField(TEXT("dropout"), bDropped);

	if (!bDropped)
	{
		const float MeasuredCo2 = FMath::Max(0.0f, GroundTruthCo2 + Co2BiasPpm
			+ AeroRecord::GaussianSample(SampleCount, SimulationSeconds, SaltCo2) * Co2SigmaPpm);
		const float MeasuredPm25 = FMath::Max(0.0f, GroundTruthPm25 + Pm25BiasUgPerM3
			+ AeroRecord::GaussianSample(SampleCount, SimulationSeconds, SaltPm25) * Pm25SigmaUgPerM3);

		Gas->SetNumberField(TEXT("co2_ppm_ground_truth"), GroundTruthCo2);
		Gas->SetNumberField(TEXT("co2_ppm_measured"), MeasuredCo2);
		Gas->SetNumberField(TEXT("pm25_ug_per_m3_ground_truth"), GroundTruthPm25);
		Gas->SetNumberField(TEXT("pm25_ug_per_m3_measured"), MeasuredPm25);
	}

	Gas->SetNumberField(TEXT("co2_bias_ppm"), Co2BiasPpm);
	Gas->SetNumberField(TEXT("co2_sigma_ppm"), Co2SigmaPpm);
	Gas->SetNumberField(TEXT("pm25_bias_ug_per_m3"), Pm25BiasUgPerM3);
	Gas->SetNumberField(TEXT("pm25_sigma_ug_per_m3"), Pm25SigmaUgPerM3);
	Gas->SetNumberField(TEXT("dropout_probability"), DropoutProbability);

	const bool bHazardContribution =
		!FMath::IsNearlyZero(HazardDelta.Co2DeltaPpm)
		|| !FMath::IsNearlyZero(HazardDelta.Pm25DeltaUgPerM3);

	Gas->SetStringField(TEXT("ground_truth_source"),
		bHazardContribution ? TEXT("ambient_background_plus_live_hazard_emission") : TEXT("configured_ambient_background"));
	Gas->SetNumberField(TEXT("hazard_co2_delta_ppm"), HazardDelta.Co2DeltaPpm);
	Gas->SetNumberField(TEXT("hazard_pm25_delta_ug_per_m3"), HazardDelta.Pm25DeltaUgPerM3);

	Root->SetObjectField(TEXT("gas"), Gas);

	if (bWriteRecordsToDisk)
	{
		AeroRecord::SaveRecordAsync(Root, AeroRecord::BuildSensorDirectory(this), AeroRecord::BuildBaseFileName(this));
	}

	RegisterSample(SimulationSeconds);
}
