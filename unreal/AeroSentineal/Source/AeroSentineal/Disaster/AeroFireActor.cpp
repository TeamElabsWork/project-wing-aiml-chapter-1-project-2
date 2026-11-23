// Copyright AeroSentinel. All Rights Reserved.

#include "Disaster/AeroFireActor.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AAeroFireActor::AAeroFireActor()
{
	DisasterType = EAeroDisasterType::ForestFire;

	// A fire front moves far faster than a flood: minutes to build, a couple of minutes at peak.
	TriggerDurationSimSeconds = 6.0f;
	DevelopDurationSimSeconds = 180.0f;
	PeakDurationSimSeconds = 120.0f;
	RecedeDurationSimSeconds = 240.0f;

	InitialRadiusCm = 800.0f;
	MaxRadiusCm = 15000.0f;

	FireCore = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FireCore"));
	FireCore->SetupAttachment(SceneRoot);
	FireCore->SetMobility(EComponentMobility::Movable);
	FireCore->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FireCore->SetGenerateOverlapEvents(false);
	FireCore->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFinder.Succeeded())
	{
		FireCore->SetStaticMesh(SphereFinder.Object);
	}

	SmokeColumn = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SmokeColumn"));
	SmokeColumn->SetupAttachment(SceneRoot);
	SmokeColumn->SetMobility(EComponentMobility::Movable);
	SmokeColumn->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SmokeColumn->SetGenerateOverlapEvents(false);
	SmokeColumn->SetCastShadow(false);
	SmokeColumn->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded())
	{
		SmokeColumn->SetStaticMesh(CylinderFinder.Object);
	}

	FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	FireLight->SetupAttachment(SceneRoot);
	FireLight->SetMobility(EComponentMobility::Movable);
	FireLight->SetIntensity(0.0f);
	FireLight->SetAttenuationRadius(2000.0f);
	FireLight->SetVisibility(false);
}

void AAeroFireActor::ApplyDisasterState()
{
	const bool bVisible = SeverityFraction > KINDA_SMALL_NUMBER;

	if (FireCore)
	{
		FireCore->SetVisibility(bVisible);
	}
	if (SmokeColumn)
	{
		SmokeColumn->SetVisibility(bVisible);
	}
	if (FireLight)
	{
		FireLight->SetVisibility(bVisible);
	}

	if (!bVisible)
	{
		if (FireLight)
		{
			FireLight->SetIntensity(0.0f);
		}
		return;
	}

	const float FootprintRadiusCm = FMath::Max(AffectedRadiusCm, 100.0f);

	// The engine sphere is 100 cm across, so a radius of R maps to a scale of R / 50.
	if (FireCore)
	{
		const float CoreRadiusCm = FMath::Max(100.0f, FootprintRadiusCm * 0.08f);
		const float CoreScale = CoreRadiusCm / 50.0f;
		FireCore->SetWorldScale3D(FVector(CoreScale, CoreScale, CoreScale));
		FireCore->SetWorldLocation(Origin + FVector(0.0f, 0.0f, CoreRadiusCm * 0.6f));
	}

	if (SmokeColumn)
	{
		const float ColumnRadiusCm = FMath::Max(150.0f, FootprintRadiusCm * 0.25f);
		const float ColumnHeightCm = 400.0f + 2600.0f * SeverityFraction;

		// The engine cylinder is 100 cm across and 100 cm tall.
		SmokeColumn->SetWorldScale3D(FVector(
			ColumnRadiusCm / 50.0f,
			ColumnRadiusCm / 50.0f,
			ColumnHeightCm / 100.0f));
		SmokeColumn->SetWorldLocation(Origin + FVector(0.0f, 0.0f, ColumnHeightCm * 0.5f));
	}

	if (FireLight)
	{
		// Flicker is a function of simulation time, so a replayed episode is bit-identical.
		const float Flicker = 0.85f + 0.15f * FMath::Sin(static_cast<float>(ElapsedSimSeconds) * 11.0f);
		FireLight->SetIntensity(PeakLightIntensityLumens * SeverityFraction * Flicker);
		FireLight->SetAttenuationRadius(FMath::Max(FootprintRadiusCm * 2.0f, 200.0f));
	}
}

FAeroGroundTruth AAeroFireActor::GetGroundTruthAtLocation(const FVector& WorldLocation) const
{
	FAeroGroundTruth Truth = Super::GetGroundTruthAtLocation(WorldLocation);

	if (Truth.DisasterType == EAeroDisasterType::ForestFire)
	{
		Truth.FireIntensity = SeverityFraction;
	}

	return Truth;
}

FAeroEnvironmentalDelta AAeroFireActor::GetEnvironmentalDeltaAt(const FVector& WorldLocation) const
{
	FAeroEnvironmentalDelta Delta;

	if (Phase == EAeroDisasterPhase::Idle || Phase == EAeroDisasterPhase::Terminated)
	{
		return Delta;
	}

	// Heat and combustion products are confined to the burning footprint.
	const float HeatFalloff = RadialFalloff(WorldLocation);
	if (HeatFalloff > 0.0f)
	{
		Delta.TemperatureDeltaC = PeakTemperatureDeltaC * SeverityFraction * HeatFalloff;
		Delta.Co2DeltaPpm = PeakCo2DeltaPpm * SeverityFraction * HeatFalloff;
	}

	// Smoke drifts well downwind of the flames, so it gets its own, wider footprint.
	const float SmokeRadiusCm = FMath::Max(AffectedRadiusCm * SmokeRadiusMultiplier, 1.0f);
	const float DistanceCm = FVector::Dist2D(WorldLocation, GetFootprintCentre());

	if (DistanceCm <= SmokeRadiusCm)
	{
		const float SmokeFalloff = FMath::Clamp(1.0f - DistanceCm / SmokeRadiusCm, 0.0f, 1.0f);
		Delta.Pm25DeltaUgPerM3 = PeakPm25DeltaUgPerM3 * SeverityFraction * SmokeFalloff;
		Delta.VisibilityLossFraction = PeakVisibilityLossFraction * SeverityFraction * SmokeFalloff;
	}

	return Delta;
}
