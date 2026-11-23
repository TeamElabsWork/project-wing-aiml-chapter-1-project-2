// Copyright AeroSentinel. All Rights Reserved.

#include "Drone/AeroBatteryComponent.h"

#include "Core/AeroLog.h"
#include "Drone/AeroDronePawn.h"

UAeroBatteryComponent::UAeroBatteryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UAeroBatteryComponent::BeginPlay()
{
	Super::BeginPlay();

	// A run always starts with a full pack on the pad.
	RemainingWh = CapacityWh;
	bReserveNotified = false;
	bFullNotified = true;

	UE_LOG(LogAeroSentinel, Log, TEXT("Battery %s: %.1f Wh, return reserve at %.0f%%"),
		*GetName(), CapacityWh, ReturnReserveFraction * 100.0f);
}

void UAeroBatteryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AAeroDronePawn* Drone = Cast<AAeroDronePawn>(GetOwner());
	const EAeroDroneState State = Drone ? Drone->GetDroneState() : EAeroDroneState::Docked;

	const bool bOnPad = (State == EAeroDroneState::Charging || State == EAeroDroneState::Docked);

	if (bOnPad)
	{
		CurrentDrawWatts = -ChargeWatts;
		RemainingWh = FMath::Min(CapacityWh, RemainingWh + ChargeWatts * DeltaTime * TimeScaleMultiplier / 3600.0f);
	}
	else
	{
		switch (State)
		{
		case EAeroDroneState::TakingOff:
		case EAeroDroneState::Ascending:
		case EAeroDroneState::Landing:
			CurrentDrawWatts = ClimbDrainWatts;
			break;

		case EAeroDroneState::Patrolling:
		case EAeroDroneState::Investigating:
		case EAeroDroneState::Returning:
			CurrentDrawWatts = CruiseDrainWatts;
			break;

		case EAeroDroneState::Idle:
			CurrentDrawWatts = IdleDrainWatts;
			break;

		default:
			CurrentDrawWatts = 0.0f;
			break;
		}

		RemainingWh = FMath::Max(0.0f, RemainingWh - CurrentDrawWatts * DeltaTime * TimeScaleMultiplier / 3600.0f);
	}

	const float Fraction = GetStateOfChargeFraction();

	if (!bReserveNotified && Fraction <= ReturnReserveFraction)
	{
		bReserveNotified = true;
		bFullNotified = false;
		UE_LOG(LogAeroSentinel, Log, TEXT("Battery %s reached return reserve (%.1f%% of %.1f Wh)"),
			*GetName(), Fraction * 100.0f, CapacityWh);
		OnReserveReached.Broadcast();
	}

	if (!bFullNotified && Fraction >= 0.995f)
	{
		bFullNotified = true;
		bReserveNotified = false;
		UE_LOG(LogAeroSentinel, Log, TEXT("Battery %s fully charged"), *GetName());
		OnFullyCharged.Broadcast();
	}
}

float UAeroBatteryComponent::GetStateOfChargeFraction() const
{
	return CapacityWh > KINDA_SMALL_NUMBER ? FMath::Clamp(RemainingWh / CapacityWh, 0.0f, 1.0f) : 0.0f;
}

bool UAeroBatteryComponent::IsReserveReached() const
{
	return GetStateOfChargeFraction() <= ReturnReserveFraction;
}

bool UAeroBatteryComponent::IsFull() const
{
	return GetStateOfChargeFraction() >= 0.995f;
}

void UAeroBatteryComponent::SetStateOfChargeFraction(float Fraction)
{
	RemainingWh = CapacityWh * FMath::Clamp(Fraction, 0.0f, 1.0f);
	bReserveNotified = IsReserveReached();
	bFullNotified = IsFull();
}
