// Copyright AeroSentinel. All Rights Reserved.

#include "World/AeroZoneVolume.h"

#include "Components/BoxComponent.h"

AAeroZoneVolume::AAeroZoneVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	// 100 cm half-extent by default; the level scales the actor (or sets BoxExtent) to the region.
	ZoneBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneBounds"));
	SetRootComponent(ZoneBounds);
	ZoneBounds->InitBoxExtent(FVector(100.0f, 100.0f, 100.0f));
	ZoneBounds->SetMobility(EComponentMobility::Static);
	ZoneBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneBounds->SetGenerateOverlapEvents(false);
	ZoneBounds->ShapeColor = FColor(60, 140, 200);
	ZoneBounds->SetHiddenInGame(true);
}

void AAeroZoneVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshZoneBounds();
}

void AAeroZoneVolume::RefreshZoneBounds()
{
	if (ZoneBounds != nullptr)
	{
		ZoneBounds->UpdateBounds();
	}
}

FBox AAeroZoneVolume::GetZoneBounds() const
{
	if (ZoneBounds == nullptr)
	{
		return FBox(ForceInit);
	}

	// Bounds already folds in the actor's rotation and scale, so a zone placed as a scaled box is
	// still queried correctly without the caller knowing about either.
	return ZoneBounds->Bounds.GetBox();
}

FVector AAeroZoneVolume::GetZoneCenter() const
{
	return GetZoneBounds().GetCenter();
}

bool AAeroZoneVolume::ContainsLocation(const FVector& WorldLocation) const
{
	if (ZoneBounds == nullptr)
	{
		return false;
	}

	// Oriented-box test: pull the point into the box's local frame and compare against the authored
	// extent. Testing the component's cached bounds instead would test the axis-aligned box that
	// encloses a rotated zone - for the 45-degree river band that box is roughly four times too
	// large and would swallow the neighbouring zones.
	const FTransform& BoxTransform = ZoneBounds->GetComponentTransform();
	const FVector Local = BoxTransform.InverseTransformPosition(WorldLocation);
	const FVector Extent = ZoneBounds->GetUnscaledBoxExtent();

	return FMath::Abs(Local.X) <= Extent.X
		&& FMath::Abs(Local.Y) <= Extent.Y
		&& FMath::Abs(Local.Z) <= Extent.Z;
}
