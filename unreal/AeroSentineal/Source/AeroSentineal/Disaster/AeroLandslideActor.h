// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Disaster/AeroDisasterActor.h"
#include "AeroLandslideActor.generated.h"

class UStaticMeshComponent;

/**
 * Slope failure.
 *
 * The one hazard whose truth moves. The debris mass travels downslope as severity rises, so the
 * footprint centre is the debris front rather than the trigger point - a drone surveying the original
 * scar would otherwise be told there was nothing there, which is exactly the failure mode a static
 * decoration has.
 */
UCLASS(Blueprintable)
class AEROSENTINEAL_API AAeroLandslideActor : public AAeroDisasterActor
{
	GENERATED_BODY()

public:
	AAeroLandslideActor();

	/** Downslope direction in world space. Normalised on use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster|Landslide")
	FVector SlideDirection = FVector(-1.0f, 0.0f, 0.0f);

	/** How far the debris front travels between trigger and full severity, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster|Landslide")
	float SlideDistanceCm = 6000.0f;

	/** Depth of the debris pile, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster|Landslide")
	float DebrisHeightCm = 400.0f;

	/** Dust raised at the centre at full severity, ug/m3 of PM2.5. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster|Landslide")
	float PeakPm25DeltaUgPerM3 = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster|Landslide")
	float PeakVisibilityLossFraction = 0.5f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Disaster|Landslide")
	TObjectPtr<UStaticMeshComponent> DebrisMass;

	/** World position the debris front has reached, cm. */
	UFUNCTION(BlueprintPure, Category = "Aero|Disaster|Landslide")
	FVector GetDebrisFrontLocation() const;

	virtual FVector GetFootprintCentre() const override;
	virtual FAeroEnvironmentalDelta GetEnvironmentalDeltaAt(const FVector& WorldLocation) const override;

protected:
	virtual void ApplyDisasterState() override;
};
