// Copyright AeroSentinel. All Rights Reserved.

#include "Sensors/AeroTelemetryComponent.h"

#include "Dom/JsonObject.h"
#include "Drone/AeroBatteryComponent.h"
#include "Drone/AeroDronePawn.h"
#include "Sensors/AeroRecordWriter.h"

UAeroTelemetryComponent::UAeroTelemetryComponent()
{
	SensorName = TEXT("Telemetry");
	CaptureIntervalSimSeconds = 1.0f;
}

void UAeroTelemetryComponent::CaptureSample(double SimulationSeconds)
{
	TSharedRef<FJsonObject> Root = AeroRecord::BuildBaseRecord(this, SimulationSeconds);
	const AAeroDronePawn* Drone = Cast<AAeroDronePawn>(GetOwner());

	TSharedRef<FJsonObject> Telemetry = MakeShared<FJsonObject>();
	if (Drone)
	{
		const UAeroBatteryComponent* Battery = Drone->GetBattery();
		Telemetry->SetStringField(TEXT("state"), StaticEnum<EAeroDroneState>()->GetNameStringByValue(
			static_cast<int64>(Drone->GetDroneState())));
		Telemetry->SetNumberField(TEXT("velocity_x_cm_per_s"), Drone->CurrentVelocity.X);
		Telemetry->SetNumberField(TEXT("velocity_y_cm_per_s"), Drone->CurrentVelocity.Y);
		Telemetry->SetNumberField(TEXT("velocity_z_cm_per_s"), Drone->CurrentVelocity.Z);
		Telemetry->SetNumberField(TEXT("speed_cm_per_s"), Drone->CurrentVelocity.Size());
		Telemetry->SetNumberField(TEXT("altitude_above_home_cm"),
			Drone->GetActorLocation().Z - Drone->GetHomeLocation().Z);
		Telemetry->SetNumberField(TEXT("battery_fraction"),
			Battery ? Battery->GetStateOfChargeFraction() : 0.0f);
		Telemetry->SetNumberField(TEXT("battery_wh"), Battery ? Battery->RemainingWh : 0.0f);
		Telemetry->SetBoolField(TEXT("external_control"), Drone->IsExternalControlActive());
	}
	else
	{
		Telemetry->SetStringField(TEXT("status"), TEXT("owner_is_not_a_drone"));
	}

	Root->SetObjectField(TEXT("telemetry"), Telemetry);

	if (bWriteRecordsToDisk)
	{
		AeroRecord::SaveRecordAsync(Root, AeroRecord::BuildSensorDirectory(this), AeroRecord::BuildBaseFileName(this));
	}

	RegisterSample(SimulationSeconds);
}
