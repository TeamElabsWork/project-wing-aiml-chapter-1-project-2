// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Disaster/AeroDisasterActor.h"
#include "AeroFireActor.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * Forest fire.
 *
 * The only hazard whose influence reaches well past its own footprint: the flames are confined to the
 * burning area, but the smoke plume carries PM2.5 and kills visibility over a much wider radius. That
 * asymmetry is the point - a policy can detect the fire from clear air downwind long before it can
 * see flames, exactly as a real sensor package would.
 *
 * The rendered flame is a lit core plus a point light rather than a particle system, so the thermal
 * capture sees a genuinely hot object in the scene instead of a sprite composited afterwards.
 */
UCLASS(Blueprintable)
class AEROSENTINEAL_API AAeroFireActor : public AAeroDisasterActor
{
	GENERATED_BODY()

public:
	AAeroFireActor();

	/** Air temperature added at the centre of the flames at full severity, Celsius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster|Fire")
	float PeakTemperatureDeltaC = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster|Fire")
	float PeakCo2DeltaPpm = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster|Fire")
	float PeakPm25DeltaUgPerM3 = 850.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster|Fire")
	float PeakVisibilityLossFraction = 0.85f;

	/** How far the smoke reaches, as a multiple of the burning footprint radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster|Fire")
	float SmokeRadiusMultiplier = 2.2f;

	/** Light emitted at the centre at full severity, lumens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster|Fire")
	float PeakLightIntensityLumens = 20000.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Disaster|Fire")
	TObjectPtr<UStaticMeshComponent> FireCore;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Disaster|Fire")
	TObjectPtr<UStaticMeshComponent> SmokeColumn;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Disaster|Fire")
	TObjectPtr<UPointLightComponent> FireLight;

	virtual FAeroGroundTruth GetGroundTruthAtLocation(const FVector& WorldLocation) const override;
	virtual FAeroEnvironmentalDelta GetEnvironmentalDeltaAt(const FVector& WorldLocation) const override;

protected:
	virtual void ApplyDisasterState() override;
};
