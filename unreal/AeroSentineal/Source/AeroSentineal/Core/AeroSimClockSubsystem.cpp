// Copyright AeroSentinel. All Rights Reserved.

#include "Core/AeroSimClockSubsystem.h"
#include "Core/AeroSimSettings.h"
#include "Core/AeroLog.h"

namespace
{
	/** A single hitch must not teleport the simulation forward by seconds of sim time. */
	constexpr float MaxRealDeltaTime = 0.25f;
}

void UAeroSimClockSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ResetSimulationTime();
}

void UAeroSimClockSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

bool UAeroSimClockSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Simulation time only exists where gameplay exists. Editor and preview worlds get no clock.
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE
		|| WorldType == EWorldType::GamePreview;
}

TStatId UAeroSimClockSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAeroSimClockSubsystem, STATGROUP_Tickables);
}

void UAeroSimClockSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bPaused || DeltaTime <= 0.0f)
	{
		return;
	}

	const float ClampedRealDelta = FMath::Min(DeltaTime, MaxRealDeltaTime);
	AdvanceSimulationTime(static_cast<double>(ClampedRealDelta) * static_cast<double>(TimeMultiplier));
}

void UAeroSimClockSubsystem::SetTimeMultiplier(float NewMultiplier)
{
	TimeMultiplier = FMath::Clamp(NewMultiplier, 0.01f, 3600.0f);
	UE_LOG(LogAeroSentinel, Log, TEXT("Simulation time multiplier set to x%.2f"), TimeMultiplier);
}

void UAeroSimClockSubsystem::SetPaused(bool bNewPaused)
{
	if (bPaused == bNewPaused)
	{
		return;
	}

	bPaused = bNewPaused;
	UE_LOG(LogAeroSentinel, Log, TEXT("Simulation %s at %s"),
		bPaused ? TEXT("paused") : TEXT("resumed"), *GetSimulationUtcIso8601());
}

void UAeroSimClockSubsystem::ResetSimulationTime()
{
	const UAeroSimSettings* Settings = GetDefault<UAeroSimSettings>();

	StartUtc = Settings ? Settings->GetStartUtc() : FDateTime(2026, 6, 1, 6, 0, 0);
	TimeMultiplier = Settings ? FMath::Clamp(Settings->DefaultTimeMultiplier, 0.01f, 3600.0f) : 1.0f;
	SimulationSeconds = 0.0;
	bPaused = false;

	UE_LOG(LogAeroSentinel, Log, TEXT("Simulation clock reset. Start=%s Multiplier=x%.2f"),
		*GetSimulationUtcIso8601(), TimeMultiplier);
}

void UAeroSimClockSubsystem::AdvanceSimulationTime(double DeltaSimSeconds)
{
	if (bPaused || DeltaSimSeconds <= 0.0)
	{
		return;
	}

	SimulationSeconds += DeltaSimSeconds;
	OnTimeAdvanced.Broadcast(SimulationSeconds, static_cast<float>(DeltaSimSeconds));
}

FDateTime UAeroSimClockSubsystem::GetSimulationUtc() const
{
	return StartUtc + FTimespan::FromSeconds(SimulationSeconds);
}

FString UAeroSimClockSubsystem::GetSimulationUtcIso8601() const
{
	return GetSimulationUtc().ToIso8601();
}
