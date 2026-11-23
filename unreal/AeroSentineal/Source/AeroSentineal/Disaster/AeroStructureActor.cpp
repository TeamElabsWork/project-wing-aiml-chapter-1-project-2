// Copyright AeroSentinel. All Rights Reserved.

#include "Disaster/AeroStructureActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AAeroStructureActor::AAeroStructureActor()
{
	DisasterType = EAeroDisasterType::StructuralDamage;

	// Structural failure escalates over a long time and does not heal, so the recede phase is long and
	// shallow: the building never returns to Normal within one scenario.
	TriggerDurationSimSeconds = 12.0f;
	DevelopDurationSimSeconds = 300.0f;
	PeakDurationSimSeconds = 180.0f;
	RecedeDurationSimSeconds = 400.0f;

	InitialRadiusCm = 900.0f;
	MaxRadiusCm = 1800.0f;

	StructureMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StructureMesh"));
	StructureMesh->SetupAttachment(SceneRoot);
	StructureMesh->SetMobility(EComponentMobility::Movable);
	StructureMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StructureMesh->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		StructureMesh->SetStaticMesh(CubeFinder.Object);
	}
}

EAeroDamageState AAeroStructureActor::DamageStateForSeverity(float InSeverityFraction)
{
	if (InSeverityFraction < 0.15f)
	{
		return EAeroDamageState::Normal;
	}
	if (InSeverityFraction < 0.40f)
	{
		return EAeroDamageState::Minor;
	}
	if (InSeverityFraction < 0.65f)
	{
		return EAeroDamageState::Moderate;
	}
	if (InSeverityFraction < 0.88f)
	{
		return EAeroDamageState::Severe;
	}
	return EAeroDamageState::Collapse;
}

void AAeroStructureActor::ApplyDisasterState()
{
	DamageState = DamageStateForSeverity(SeverityFraction);

	if (!StructureMesh)
	{
		return;
	}

	// The building exists before the scenario starts and stays after it ends: an intact building is
	// not something to hide, it is the Normal class of the classification target.
	StructureMesh->SetVisibility(true);

	// The engine cube is 100 cm on a side.
	StructureMesh->SetWorldScale3D(FVector(
		FMath::Max(StructureExtentCm.X, 50.0f) / 100.0f,
		FMath::Max(StructureExtentCm.Y, 50.0f) / 100.0f,
		FMath::Max(StructureExtentCm.Z, 50.0f) / 100.0f));

	const float Tilt = MaxTiltDegrees * SeverityFraction;
	const float Settlement = MaxSettlementCm * SeverityFraction;

	StructureMesh->SetWorldLocation(Origin + FVector(0.0f, 0.0f, StructureExtentCm.Z * 0.5f - Settlement));
	StructureMesh->SetWorldRotation(FRotator(0.0f, 0.0f, Tilt));
}

FAeroEnvironmentalDelta AAeroStructureActor::GetEnvironmentalDeltaAt(const FVector& WorldLocation) const
{
	FAeroEnvironmentalDelta Delta;

	const float Falloff = RadialFalloff(WorldLocation);
	if (Falloff <= 0.0f)
	{
		return Delta;
	}

	// Collapse dust only appears once the structure is actually breaking up.
	if (SeverityFraction <= 0.15f)
	{
		return Delta;
	}

	Delta.Pm25DeltaUgPerM3 = PeakPm25DeltaUgPerM3 * SeverityFraction * Falloff;
	Delta.VisibilityLossFraction = PeakVisibilityLossFraction * SeverityFraction * Falloff;

	return Delta;
}
