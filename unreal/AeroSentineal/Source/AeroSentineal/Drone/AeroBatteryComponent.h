// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AeroBatteryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAeroBatteryReserveSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAeroBatteryFullSignature);

/**
 * Energy model for one airframe.
 *
 * The battery is the only thing that decides when a mission must end: it drains while the airframe is
 * airborne and refills while it sits on the pad, and the pawn watches ReturnReserveFraction rather
 * than a hard-coded timer. Values are real watt-hours and watts at the pack, so the numbers in the
 * Telemetry/ dataset records mean something:
 *
 *   99 Wh pack, 190 W in cruise  -> about 31 min of patrol, which is a plausible survey quad
 *   320 W charger on the pad     -> about 18 min to refill
 *
 * TimeScaleMultiplier exists so a full drain-charge cycle can be watched in a test without inventing
 * different energy numbers; leave it at 1.0 for any data-generating run.
 */
UCLASS(ClassGroup = (Aero), meta = (BlueprintSpawnableComponent))
class AEROSENTINEAL_API UAeroBatteryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAeroBatteryComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Pack capacity, watt-hours. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Battery")
	float CapacityWh = 99.0f;

	/** Draw in level cruise and investigation, watts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Battery")
	float CruiseDrainWatts = 190.0f;

	/** Draw while climbing or descending, watts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Battery")
	float ClimbDrainWatts = 260.0f;

	/** Draw while sitting on the pad with systems up, watts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Battery")
	float IdleDrainWatts = 5.0f;

	/** Charger output while docked, watts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Battery")
	float ChargeWatts = 320.0f;

	/** Break off the mission at this state of charge. 0.25 leaves room to fly home and land. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Battery")
	float ReturnReserveFraction = 0.25f;

	/**
	 * Multiplies drain and charge rates. 1.0 is real time and the only value that belongs in a
	 * dataset run; raise it to watch a whole cycle in PIE.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Battery")
	float TimeScaleMultiplier = 1.0f;

	/** Energy left in the pack, watt-hours. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Battery")
	float RemainingWh = 99.0f;

	/** Instantaneous power: positive while draining, negative while charging, watts. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Battery")
	float CurrentDrawWatts = 0.0f;

	UPROPERTY(BlueprintAssignable, Category = "Aero|Battery")
	FAeroBatteryReserveSignature OnReserveReached;

	UPROPERTY(BlueprintAssignable, Category = "Aero|Battery")
	FAeroBatteryFullSignature OnFullyCharged;

	UFUNCTION(BlueprintPure, Category = "Aero|Battery")
	float GetStateOfChargeFraction() const;

	UFUNCTION(BlueprintPure, Category = "Aero|Battery")
	bool IsReserveReached() const;

	UFUNCTION(BlueprintPure, Category = "Aero|Battery")
	bool IsFull() const;

	/** Force a state of charge, 0..1. Used to set up a test scenario. */
	UFUNCTION(BlueprintCallable, Category = "Aero|Battery")
	void SetStateOfChargeFraction(float Fraction);

protected:
	bool bReserveNotified = false;

	bool bFullNotified = false;
};
