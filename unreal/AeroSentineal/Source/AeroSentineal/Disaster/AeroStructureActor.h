// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Disaster/AeroDisasterActor.h"
#include "AeroStructureActor.generated.h"

class UStaticMeshComponent;

/**
 * Structural damage to one building, stepping through EAeroDamageState.
 *
 * Unlike the three natural hazards this one is a graded classification target: the building is always
 * present (an intact village building is not a hazard), and severity walks it from Normal through
 * Minor, Moderate, Severe to Collapse. The state is derived from severity in one place -
 * DamageStateForSeverity - so a record's damage state and the rendered lean can never disagree.
 */
UCLASS(Blueprintable)
class AEROSENTINEAL_API AAeroStructureActor : public AAeroDisasterActor
{
	GENERATED_BODY()

public:
	AAeroStructureActor();

	/** Building extents, cm. The mesh is an engine cube scaled to this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster|Structure")
	FVector StructureExtentCm = FVector(1200.0f, 900.0f, 800.0f);

	/** Lean applied at full severity, degrees of roll. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster|Structure")
	float MaxTiltDegrees = 12.0f;

	/** How far the structure settles into the ground at full severity, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster|Structure")
	float MaxSettlementCm = 180.0f;

	/** Dust raised by the collapse, ug/m3 of PM2.5 at full severity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster|Structure")
	float PeakPm25DeltaUgPerM3 = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Disaster|Structure")
	float PeakVisibilityLossFraction = 0.25f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Disaster|Structure")
	TObjectPtr<UStaticMeshComponent> StructureMesh;

	/** Live damage classification, derived from the life-cycle severity. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster|Structure")
	EAeroDamageState DamageState = EAeroDamageState::Normal;

	/** The only place the severity thresholds live. */
	UFUNCTION(BlueprintPure, Category = "Aero|Disaster|Structure")
	static EAeroDamageState DamageStateForSeverity(float InSeverityFraction);

	virtual FAeroEnvironmentalDelta GetEnvironmentalDeltaAt(const FVector& WorldLocation) const override;

protected:
	virtual void ApplyDisasterState() override;
};
