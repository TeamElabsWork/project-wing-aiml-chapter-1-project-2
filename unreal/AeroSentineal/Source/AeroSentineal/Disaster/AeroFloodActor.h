// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Disaster/AeroDisasterActor.h"
#include "AeroFloodActor.generated.h"

class UStaticMeshComponent;

/**
 * River / floodplain inundation.
 *
 * The rendered water surface IS the ground truth: its world Z is Origin.Z plus the standing depth the
 * record reports, so an RGB frame of the flood and the water_depth_cm in its record cannot disagree.
 *
 * A flood is the slow hazard of the four - it develops over minutes and recedes over more, which is
 * what makes it the natural target for a policy that has to patrol a large area and come back later
 * to see whether a river has broken its banks.
 */
UCLASS(Blueprintable)
class AEROSENTINEAL_API AAeroFloodActor : public AAeroDisasterActor
{
	GENERATED_BODY()

public:
	AAeroFloodActor();

	/** Standing water depth at full severity, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster|Flood")
	float PeakWaterDepthCm = 400.0f;

	/** Cooling the water puts on the air above it at full severity, Celsius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster|Flood")
	float PeakCoolingCelsius = 4.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Disaster|Flood")
	TObjectPtr<UStaticMeshComponent> WaterSurface;

	virtual FAeroGroundTruth GetGroundTruthAtLocation(const FVector& WorldLocation) const override;
	virtual FAeroEnvironmentalDelta GetEnvironmentalDeltaAt(const FVector& WorldLocation) const override;

protected:
	virtual void ApplyDisasterState() override;
};
