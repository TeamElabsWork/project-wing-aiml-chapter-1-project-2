// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/AeroTypes.h"
#include "AeroZoneVolume.generated.h"

class UBoxComponent;

/**
 * A named, tagged region of the simulation map.
 *
 * Deliberately not an AVolume/brush: zones are rotated boxes whose world bounds are the only thing
 * that matters, so there is no physics cost and no collision channel to maintain. Every point in
 * the world can be attributed to at most one zone, which is what makes region_id reliable in a
 * dataset record.
 *
 * Overlaps are resolved by Priority: the river/floodplain sits inside the valley floor and wins
 * over the surrounding area zones.
 */
UCLASS(Blueprintable)
class AEROSENTINEAL_API AAeroZoneVolume : public AActor
{
	GENERATED_BODY()

public:
	AAeroZoneVolume();

	/** Region this volume represents. Stored verbatim as region_id in every data record. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Zone")
	EAeroZoneId ZoneId = EAeroZoneId::None;

	/** Human-readable label for the HUD and logs. Falls back to the enum name when empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Zone")
	FString ZoneDisplayName;

	/** Higher wins when two zones contain the same point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Zone")
	int32 Priority = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Zone")
	TObjectPtr<UBoxComponent> ZoneBounds;

	/** True when the point lies inside the rotated, scaled box. */
	UFUNCTION(BlueprintPure, Category = "Aero|Zone")
	bool ContainsLocation(const FVector& WorldLocation) const;

	/** World-space axis-aligned bounds of the zone. */
	UFUNCTION(BlueprintPure, Category = "Aero|Zone")
	FBox GetZoneBounds() const;

	UFUNCTION(BlueprintPure, Category = "Aero|Zone")
	FVector GetZoneCenter() const;

	/** Recomputes the box bounds after the extent was changed from script. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Aero|Zone")
	void RefreshZoneBounds();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
};
