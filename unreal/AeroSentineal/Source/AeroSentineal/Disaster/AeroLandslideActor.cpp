// Copyright AeroSentinel. All Rights Reserved.

#include "Disaster/AeroLandslideActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AAeroLandslideActor::AAeroLandslideActor()
{
	DisasterType = EAeroDisasterType::Landslide;

	// Slope failure is abrupt: a short trigger, a fast develop, and a long tail of settling debris.
	TriggerDurationSimSeconds = 4.0f;
	DevelopDurationSimSeconds = 90.0f;
	PeakDurationSimSeconds = 150.0f;
	RecedeDurationSimSeconds = 260.0f;

	InitialRadiusCm = 1200.0f;
	MaxRadiusCm = 7000.0f;

	DebrisMass = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebrisMass"));
	DebrisMass->SetupAttachment(SceneRoot);
	DebrisMass->SetMobility(EComponentMobility::Movable);
	DebrisMass->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DebrisMass->SetGenerateOverlapEvents(false);
	DebrisMass->SetVisibility(false);

	// Engine content is referenced, never duplicated: a 100 cm cube scaled to the debris pile.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		DebrisMass->SetStaticMesh(CubeFinder.Object);
	}
}

FVector AAeroLandslideActor::GetDebrisFrontLocation() const
{
	const FVector Direction = SlideDirection.GetSafeNormal();
	return Origin + Direction * (SlideDistanceCm * SeverityFraction);
}

FVector AAeroLandslideActor::GetFootprintCentre() const
{
	// The hazard is where the debris is, not where the slope first gave way.
	return GetDebrisFrontLocation();
}

void AAeroLandslideActor::ApplyDisasterState()
{
	if (!DebrisMass)
	{
		return;
	}

	const bool bVisible = SeverityFraction > KINDA_SMALL_NUMBER;
	DebrisMass->SetVisibility(bVisible);

	if (!bVisible)
	{
		return;
	}

	// The engine cube is 100 cm on a side, so a footprint radius of R maps to a scale of R / 50 and
	// the pile height maps as height / 100.
	const float PlanScale = FMath::Max(AffectedRadiusCm, 50.0f) / 50.0f;
	const float HeightScale = FMath::Max(DebrisHeightCm, 10.0f) / 100.0f;
	DebrisMass->SetWorldScale3D(FVector(PlanScale, PlanScale, HeightScale));

	const FVector Front = GetDebrisFrontLocation();
	DebrisMass->SetWorldLocation(Front + FVector(0.0f, 0.0f, DebrisHeightCm * 0.5f));
}

FAeroEnvironmentalDelta AAeroLandslideActor::GetEnvironmentalDeltaAt(const FVector& WorldLocation) const
{
	FAeroEnvironmentalDelta Delta;

	const float Falloff = RadialFalloff(WorldLocation);
	if (Falloff <= 0.0f)
	{
		return Delta;
	}

	// Moving rock and soil raises dust; there is no thermal or combustion signature.
	Delta.Pm25DeltaUgPerM3 = PeakPm25DeltaUgPerM3 * SeverityFraction * Falloff;
	Delta.VisibilityLossFraction = PeakVisibilityLossFraction * SeverityFraction * Falloff;

	return Delta;
}
