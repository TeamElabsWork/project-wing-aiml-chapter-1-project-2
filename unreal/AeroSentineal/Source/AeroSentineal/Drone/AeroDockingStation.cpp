// Copyright AeroSentinel. All Rights Reserved.

#include "Drone/AeroDockingStation.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Core/AeroLog.h"
#include "Drone/AeroDronePawn.h"

AAeroDockingStation::AAeroDockingStation()
{
	PrimaryActorTick.bCanEverTick = false;

	Pad = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pad"));
	SetRootComponent(Pad);
	Pad->SetMobility(EComponentMobility::Static);
	Pad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pad->SetGenerateOverlapEvents(false);
	Pad->SetCastShadow(false);

	// Engine content is referenced, never duplicated: a unit cube scaled to PadExtent.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		Pad->SetStaticMesh(CubeFinder.Object);
	}

	ApplyPadScale();
}

void AAeroDockingStation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyPadScale();
}

void AAeroDockingStation::ApplyPadScale()
{
	if (!Pad)
	{
		return;
	}

	// The engine cube is 100 cm on a side, so half-extents map to scale as extent / 50.
	Pad->SetRelativeScale3D(FVector(
		FMath::Max(PadExtent.X, 1.0f) / 50.0f,
		FMath::Max(PadExtent.Y, 1.0f) / 50.0f,
		FMath::Max(PadExtent.Z, 1.0f) / 50.0f));
}

void AAeroDockingStation::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnDroneOnBeginPlay && !DockedDrone)
	{
		AAeroDronePawn* Drone = SpawnDrone();
		if (Drone && bLaunchMissionOnBeginPlay)
		{
			Drone->BeginMission();
		}
	}
}

FTransform AAeroDockingStation::GetDroneSpawnTransform() const
{
	const FVector DeckCentre = GetActorLocation() + FVector(0.0f, 0.0f, PadExtent.Z + 5.0f);
	return FTransform(GetActorRotation(), DeckCentre);
}

AAeroDronePawn* AAeroDockingStation::SpawnDrone()
{
	if (DockedDrone)
	{
		return DockedDrone;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// An airframe already placed in the level IS the deployment: adopt it rather than deploying a
	// second one. Two airframes in one training environment interleave two trajectories into the same
	// dataset and give the player two HUDs fighting for one viewport.
	if (AAeroDronePawn* Existing = Cast<AAeroDronePawn>(
			UGameplayStatics::GetActorOfClass(World, AAeroDronePawn::StaticClass())))
	{
		DockedDrone = Existing;

		if (bPossessSpawnedDrone)
		{
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
			{
				if (PC->GetPawn() != Existing)
				{
					PC->Possess(Existing);
				}
			}
		}

		UE_LOG(LogAeroSentinel, Log,
			TEXT("Docking station %s adopted the airframe already in the level: %s at %s"),
			*GetName(), *Existing->GetName(), *Existing->GetActorLocation().ToCompactString());

		return Existing;
	}

	TSubclassOf<AAeroDronePawn> ClassToSpawn = DroneClass;
	if (!ClassToSpawn)
	{
		ClassToSpawn = AAeroDronePawn::StaticClass();
	}

	const FTransform SpawnTransform = GetDroneSpawnTransform();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;

	AAeroDronePawn* Drone = World->SpawnActor<AAeroDronePawn>(ClassToSpawn, SpawnTransform, Params);
	if (!Drone)
	{
		UE_LOG(LogAeroSentinel, Error, TEXT("Docking station %s failed to spawn a drone"), *GetName());
		return nullptr;
	}

	DockedDrone = Drone;
	Drone->SetHomeLocation(SpawnTransform.GetLocation());

	if (bPossessSpawnedDrone)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (PC->GetPawn() != Drone)
			{
				PC->Possess(Drone);
			}
		}
	}

	UE_LOG(LogAeroSentinel, Log, TEXT("Docking station %s deployed %s at %s"),
		*GetName(), *Drone->GetName(), *SpawnTransform.GetLocation().ToCompactString());

	return Drone;
}
