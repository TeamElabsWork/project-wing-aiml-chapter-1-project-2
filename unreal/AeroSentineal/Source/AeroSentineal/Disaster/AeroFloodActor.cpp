// Copyright AeroSentinel. All Rights Reserved.

#include "Disaster/AeroFloodActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AAeroFloodActor::AAeroFloodActor()
{
	DisasterType = EAeroDisasterType::Flood;

	TriggerDurationSimSeconds = 10.0f;
	DevelopDurationSimSeconds = 240.0f;
	PeakDurationSimSeconds = 120.0f;
	RecedeDurationSimSeconds = 300.0f;

	InitialRadiusCm = 4000.0f;
	MaxRadiusCm = 14000.0f;

	WaterSurface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaterSurface"));
	WaterSurface->SetupAttachment(SceneRoot);
	WaterSurface->SetMobility(EComponentMobility::Movable);
	WaterSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WaterSurface->SetGenerateOverlapEvents(false);
	WaterSurface->SetCastShadow(false);
	WaterSurface->SetVisibility(false);

	// Engine content is referenced, never duplicated: a 100 x 100 cm plane scaled to the footprint.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneFinder.Succeeded())
	{
		WaterSurface->SetStaticMesh(PlaneFinder.Object);
	}
}

void AAeroFloodActor::ApplyDisasterState()
{
	if (!WaterSurface)
	{
		return;
	}

	const bool bVisible = SeverityFraction > KINDA_SMALL_NUMBER;
	WaterSurface->SetVisibility(bVisible);

	if (!bVisible)
	{
		return;
	}

	// The engine plane is 100 cm across, so a footprint radius of R maps to a scale of R / 50.
	const float RadiusScale = FMath::Max(AffectedRadiusCm, 10.0f) / 50.0f;
	WaterSurface->SetWorldScale3D(FVector(RadiusScale, RadiusScale, 1.0f));

	// The surface height IS the reported depth. There is no separate visual approximation to drift.
	WaterSurface->SetWorldLocation(FVector(Origin.X, Origin.Y, Origin.Z + PeakWaterDepthCm * SeverityFraction));
}

FAeroGroundTruth AAeroFloodActor::GetGroundTruthAtLocation(const FVector& WorldLocation) const
{
	FAeroGroundTruth Truth = Super::GetGroundTruthAtLocation(WorldLocation);

	if (Truth.DisasterType == EAeroDisasterType::Flood)
	{
		Truth.WaterDepthCm = PeakWaterDepthCm * SeverityFraction;
	}

	return Truth;
}

FAeroEnvironmentalDelta AAeroFloodActor::GetEnvironmentalDeltaAt(const FVector& WorldLocation) const
{
	FAeroEnvironmentalDelta Delta;

	const float Falloff = RadialFalloff(WorldLocation);
	if (Falloff <= 0.0f)
	{
		return Delta;
	}

	// Evaporative cooling over open water, strongest over the middle of the inundation.
	Delta.TemperatureDeltaC = -PeakCoolingCelsius * SeverityFraction * Falloff;

	return Delta;
}
