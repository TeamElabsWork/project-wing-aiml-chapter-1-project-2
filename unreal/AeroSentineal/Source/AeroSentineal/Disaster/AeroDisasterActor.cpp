// Copyright AeroSentinel. All Rights Reserved.

#include "Disaster/AeroDisasterActor.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Core/AeroLog.h"
#include "Core/AeroSimClockSubsystem.h"
#include "Disaster/AeroDisasterSubsystem.h"

AAeroDisasterActor::AAeroDisasterActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	SceneRoot->SetMobility(EComponentMobility::Movable);

	// A hazard is something to fly through and measure, never something to collide with: the airframe
	// is kinematic, so a blocking volume would only stop the drone reaching the anomaly it is
	// investigating.
	SetActorEnableCollision(false);
}

void AAeroDisasterActor::BeginPlay()
{
	Super::BeginPlay();

	if (InstanceId.IsEmpty())
	{
		// GetName() is unique within the world, which is what a join key has to be.
		InstanceId = GetName();
	}

	Origin = GetActorLocation();

	if (UWorld* World = GetWorld())
	{
		if (UAeroDisasterSubsystem* Disasters = World->GetSubsystem<UAeroDisasterSubsystem>())
		{
			Disasters->RegisterInstance(this);
			ScenarioStartSimSeconds = Disasters->GetScenarioStartSimSeconds();
		}
	}

	ApplyDisasterState();

	UE_LOG(LogAeroSentinel, Log, TEXT("Disaster %s (%s) armed at %s: trigger +%.0f s, peak severity %.2f"),
		*InstanceId, *AeroDisasterNames::TypeToName(DisasterType), *Origin.ToCompactString(),
		TriggerDelaySimSeconds, PeakSeverity);
}

void AAeroDisasterActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UAeroDisasterSubsystem* Disasters = World->GetSubsystem<UAeroDisasterSubsystem>())
		{
			Disasters->UnregisterInstance(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AAeroDisasterActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateLifecycle();
	ApplyDisasterState();
}

void AAeroDisasterActor::UpdateLifecycle()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// The scenario start moves when an episode is reset, so it is re-read rather than cached at BeginPlay.
	if (const UAeroDisasterSubsystem* Disasters = World->GetSubsystem<UAeroDisasterSubsystem>())
	{
		ScenarioStartSimSeconds = Disasters->GetScenarioStartSimSeconds();
	}

	double Now = 0.0;
	if (const UAeroSimClockSubsystem* Clock = World->GetSubsystem<UAeroSimClockSubsystem>())
	{
		Now = Clock->GetSimulationSeconds();
	}

	ElapsedSimSeconds = Now - ScenarioStartSimSeconds - static_cast<double>(TriggerDelaySimSeconds);

	const float Trigger = FMath::Max(TriggerDurationSimSeconds, 1.0e-3f);
	const float Develop = FMath::Max(DevelopDurationSimSeconds, 1.0e-3f);
	const float PeakHold = FMath::Max(PeakDurationSimSeconds, 0.0f);
	const float Recede = FMath::Max(RecedeDurationSimSeconds, 1.0e-3f);

	if (ElapsedSimSeconds < 0.0)
	{
		Phase = EAeroDisasterPhase::Idle;
		SeverityFraction = 0.0f;
	}
	else if (ElapsedSimSeconds < Trigger)
	{
		// Present but almost no expression yet: the fire has caught, the river has not yet risen.
		Phase = EAeroDisasterPhase::Triggered;
		SeverityFraction = 0.05f * static_cast<float>(ElapsedSimSeconds / Trigger);
	}
	else if (ElapsedSimSeconds < Trigger + Develop)
	{
		Phase = EAeroDisasterPhase::Developing;
		const float LocalFraction = static_cast<float>((ElapsedSimSeconds - Trigger) / Develop);
		SeverityFraction = 0.05f + 0.95f * LocalFraction;
	}
	else if (ElapsedSimSeconds < Trigger + Develop + PeakHold)
	{
		Phase = EAeroDisasterPhase::Peak;
		SeverityFraction = 1.0f;
	}
	else if (ElapsedSimSeconds < Trigger + Develop + PeakHold + Recede)
	{
		Phase = EAeroDisasterPhase::Receding;
		const float LocalFraction = static_cast<float>((ElapsedSimSeconds - Trigger - Develop - PeakHold) / Recede);
		SeverityFraction = 1.0f - LocalFraction;
	}
	else
	{
		Phase = EAeroDisasterPhase::Terminated;
		SeverityFraction = 0.0f;
	}

	SeverityFraction = FMath::Clamp(SeverityFraction, 0.0f, 1.0f);
	Severity = SeverityFraction * FMath::Clamp(PeakSeverity, 0.0f, 1.0f);
	AffectedRadiusCm = FMath::Lerp(InitialRadiusCm, MaxRadiusCm, SeverityFraction);

	if (Phase != LoggedPhase)
	{
		LoggedPhase = Phase;
		UE_LOG(LogAeroSentinel, Log, TEXT("Disaster %s (%s) -> %s at sim %.1f s: severity %.2f, radius %.0f cm"),
			*InstanceId, *AeroDisasterNames::TypeToName(DisasterType),
			*AeroDisasterNames::PhaseToName(Phase), ElapsedSimSeconds, Severity, AffectedRadiusCm);
	}
}

void AAeroDisasterActor::ApplyDisasterState()
{
	// The base hazard owns no geometry. Every concrete hazard drives its own representation.
}

FVector AAeroDisasterActor::GetFootprintCentre() const
{
	return Origin;
}

float AAeroDisasterActor::RadialFalloff(const FVector& WorldLocation) const
{
	if (Phase == EAeroDisasterPhase::Idle || Phase == EAeroDisasterPhase::Terminated)
	{
		return 0.0f;
	}

	const float Radius = FMath::Max(AffectedRadiusCm, 1.0f);
	const float Distance = FVector::Dist2D(WorldLocation, GetFootprintCentre());

	if (Distance > Radius)
	{
		return 0.0f;
	}

	return FMath::Clamp(1.0f - Distance / Radius, 0.0f, 1.0f);
}

FAeroGroundTruth AAeroDisasterActor::GetGroundTruthAtLocation(const FVector& WorldLocation) const
{
	FAeroGroundTruth Truth;

	if (Phase == EAeroDisasterPhase::Idle || Phase == EAeroDisasterPhase::Terminated)
	{
		// Nothing has happened here yet, or any more. Reporting None is the truth, not a default.
		return Truth;
	}

	const float DistanceCm = FVector::Dist2D(WorldLocation, GetFootprintCentre());
	if (DistanceCm > AffectedRadiusCm)
	{
		return Truth;
	}

	Truth.DisasterType = DisasterType;
	Truth.DisasterPhase = Phase;
	Truth.InstanceId = InstanceId;
	Truth.Severity = Severity;
	Truth.DistanceToOriginCm = DistanceCm;
	Truth.AffectedRadiusCm = AffectedRadiusCm;

	return Truth;
}

FAeroEnvironmentalDelta AAeroDisasterActor::GetEnvironmentalDeltaAt(const FVector& WorldLocation) const
{
	// A hazard with no atmospheric signature contributes nothing to a measurement.
	return FAeroEnvironmentalDelta();
}

FAeroDisasterInstance AAeroDisasterActor::MakeInstanceSnapshot() const
{
	FAeroDisasterInstance Snapshot;
	Snapshot.InstanceId = InstanceId;
	Snapshot.DisasterType = DisasterType;
	Snapshot.Phase = Phase;
	Snapshot.Origin = GetFootprintCentre();
	Snapshot.Severity = Severity;
	Snapshot.AffectedRadiusCm = AffectedRadiusCm;
	Snapshot.ElapsedSimSeconds = ElapsedSimSeconds;

	// Ask the instance itself at its own centre, so the type-specific quantities come from the same
	// code path a sensor would use rather than being re-derived here.
	const FAeroGroundTruth SelfTruth = GetGroundTruthAtLocation(GetFootprintCentre());
	Snapshot.WaterDepthCm = SelfTruth.WaterDepthCm;
	Snapshot.FireIntensity = SelfTruth.FireIntensity;

	return Snapshot;
}

void AAeroDisasterActor::ResetForEpisode()
{
	Phase = EAeroDisasterPhase::Idle;
	Severity = 0.0f;
	SeverityFraction = 0.0f;
	AffectedRadiusCm = InitialRadiusCm;
	ElapsedSimSeconds = 0.0;
	LoggedPhase = EAeroDisasterPhase::Idle;

	ApplyDisasterState();
}
