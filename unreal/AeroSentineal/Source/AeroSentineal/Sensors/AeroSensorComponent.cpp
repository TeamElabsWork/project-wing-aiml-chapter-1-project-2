// Copyright AeroSentinel. All Rights Reserved.

#include "Sensors/AeroSensorComponent.h"

#include "Core/AeroLog.h"
#include "Core/AeroSimClockSubsystem.h"

UAeroSensorComponent::UAeroSensorComponent()
{
	// Sensors are driven by the simulation clock, never by frame tick.
	PrimaryComponentTick.bCanEverTick = false;
}

void UAeroSensorComponent::BeginPlay()
{
	Super::BeginPlay();

	PendingSimSeconds = 0.0f;

	if (UWorld* World = GetWorld())
	{
		if (UAeroSimClockSubsystem* Clock = World->GetSubsystem<UAeroSimClockSubsystem>())
		{
			Clock->OnTimeAdvanced.AddDynamic(this, &UAeroSensorComponent::HandleSimTimeAdvanced);
			UE_LOG(LogAeroSentinel, Log, TEXT("Sensor %s bound to the sim clock, interval %.2f sim s"),
				*SensorName, CaptureIntervalSimSeconds);
			return;
		}
	}

	UE_LOG(LogAeroSentinel, Warning, TEXT("Sensor %s could not bind to the sim clock and will not sample"), *SensorName);
}

void UAeroSensorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UAeroSimClockSubsystem* Clock = World->GetSubsystem<UAeroSimClockSubsystem>())
		{
			Clock->OnTimeAdvanced.RemoveDynamic(this, &UAeroSensorComponent::HandleSimTimeAdvanced);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UAeroSensorComponent::SetSensorEnabled(bool bEnabled)
{
	bSensorEnabled = bEnabled;
	PendingSimSeconds = 0.0f;
}

void UAeroSensorComponent::HandleSimTimeAdvanced(double SimulationSeconds, float DeltaSimSeconds)
{
	if (!bSensorEnabled || DeltaSimSeconds <= 0.0f)
	{
		return;
	}

	const float Interval = FMath::Max(CaptureIntervalSimSeconds, 0.01f);
	PendingSimSeconds += DeltaSimSeconds;

	// A large sim step (accelerated run or a manual clock jump) can cross several intervals; each one
	// gets its own sample so the timeline stays evenly spaced instead of collapsing into one frame.
	while (PendingSimSeconds >= Interval)
	{
		PendingSimSeconds -= Interval;
		CaptureSample(SimulationSeconds);
	}
}

void UAeroSensorComponent::RegisterSample(double SimulationSeconds)
{
	LastSampleSimSeconds = SimulationSeconds;
	++SampleCount;
	OnSampleTaken.Broadcast(SampleCount, SimulationSeconds);
}
