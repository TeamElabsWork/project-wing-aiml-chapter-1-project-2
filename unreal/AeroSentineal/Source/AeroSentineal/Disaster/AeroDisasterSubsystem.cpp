// Copyright AeroSentinel. All Rights Reserved.

#include "Disaster/AeroDisasterSubsystem.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RandomStream.h"
#include "Core/AeroLog.h"
#include "Core/AeroSimClockSubsystem.h"
#include "Drone/AeroDockingStation.h"
#include "Drone/AeroDronePawn.h"
#include "Disaster/AeroDisasterActor.h"
#include "Disaster/AeroFireActor.h"
#include "Disaster/AeroFloodActor.h"
#include "Disaster/AeroGroundTruthRecorder.h"
#include "Disaster/AeroLandslideActor.h"
#include "Disaster/AeroStructureActor.h"
#include "World/AeroRegionSubsystem.h"
#include "World/AeroZoneVolume.h"

namespace
{
	/** Hazard families in the order the seeded scenario places them, with the zone each belongs to. */
	struct FDisasterPlacement
	{
		EAeroDisasterType Type;
		EAeroZoneId Zone;
	};

	const FDisasterPlacement GScenarioLayout[] =
	{
		{ EAeroDisasterType::Flood,            EAeroZoneId::RiverFloodplain },
		{ EAeroDisasterType::ForestFire,       EAeroZoneId::Forest },
		{ EAeroDisasterType::Landslide,        EAeroZoneId::Hilly },
		{ EAeroDisasterType::StructuralDamage, EAeroZoneId::Village }
	};
}

bool UAeroDisasterSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UAeroDisasterSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (bAutoStartScenario)
	{
		StartScenario(DefaultScenarioSeed);
	}
}

void UAeroDisasterSubsystem::Deinitialize()
{
	Instances.Reset();
	SpawnedInstances.Reset();
	SpawnedRecorder = nullptr;
	bScenarioRunning = false;

	Super::Deinitialize();
}

void UAeroDisasterSubsystem::RegisterInstance(AAeroDisasterActor* Instance)
{
	if (!IsValid(Instance) || Instances.Contains(Instance))
	{
		return;
	}

	Instances.Add(Instance);
}

void UAeroDisasterSubsystem::UnregisterInstance(AAeroDisasterActor* Instance)
{
	Instances.RemoveAll([Instance](const TObjectPtr<AAeroDisasterActor>& Entry)
	{
		return Entry == nullptr || Entry == Instance;
	});

	SpawnedInstances.RemoveAll([Instance](const TObjectPtr<AAeroDisasterActor>& Entry)
	{
		return Entry == nullptr || Entry == Instance;
	});
}

int32 UAeroDisasterSubsystem::GetInstanceCount() const
{
	int32 Count = 0;
	for (const TObjectPtr<AAeroDisasterActor>& Entry : Instances)
	{
		if (IsValid(Entry))
		{
			++Count;
		}
	}
	return Count;
}

void UAeroDisasterSubsystem::GetInstances(TArray<FAeroDisasterInstance>& OutInstances) const
{
	OutInstances.Reset();

	for (const TObjectPtr<AAeroDisasterActor>& Entry : Instances)
	{
		if (const AAeroDisasterActor* Instance = Entry.Get())
		{
			OutInstances.Add(Instance->MakeInstanceSnapshot());
		}
	}
}

bool UAeroDisasterSubsystem::GetGroundTruthAtLocation(const FVector& WorldLocation, FAeroGroundTruth& OutGroundTruth) const
{
	OutGroundTruth = FAeroGroundTruth();

	for (const TObjectPtr<AAeroDisasterActor>& Entry : Instances)
	{
		const AAeroDisasterActor* Instance = Entry.Get();
		if (!Instance)
		{
			continue;
		}

		const FAeroGroundTruth Candidate = Instance->GetGroundTruthAtLocation(WorldLocation);
		if (Candidate.DisasterType == EAeroDisasterType::None)
		{
			continue;
		}

		if (OutGroundTruth.DisasterType == EAeroDisasterType::None || Candidate.Severity > OutGroundTruth.Severity)
		{
			OutGroundTruth = Candidate;
		}
	}

	return OutGroundTruth.DisasterType != EAeroDisasterType::None;
}

void UAeroDisasterSubsystem::GetEnvironmentalDeltaAt(const FVector& WorldLocation, FAeroEnvironmentalDelta& OutDelta) const
{
	OutDelta = FAeroEnvironmentalDelta();

	for (const TObjectPtr<AAeroDisasterActor>& Entry : Instances)
	{
		const AAeroDisasterActor* Instance = Entry.Get();
		if (!Instance)
		{
			continue;
		}

		const FAeroEnvironmentalDelta Contribution = Instance->GetEnvironmentalDeltaAt(WorldLocation);
		OutDelta.TemperatureDeltaC += Contribution.TemperatureDeltaC;
		OutDelta.Co2DeltaPpm += Contribution.Co2DeltaPpm;
		OutDelta.Pm25DeltaUgPerM3 += Contribution.Pm25DeltaUgPerM3;
		OutDelta.VisibilityLossFraction += Contribution.VisibilityLossFraction;
	}

	// Several fires cannot make the world darker than opaque.
	OutDelta.VisibilityLossFraction = FMath::Clamp(OutDelta.VisibilityLossFraction, 0.0f, 0.98f);
}

bool UAeroDisasterSubsystem::FindNearestInstance(const FVector& WorldLocation, FAeroDisasterInstance& OutInstance, float& OutDistanceCm) const
{
	OutInstance = FAeroDisasterInstance();
	OutDistanceCm = 1.0e9f;

	bool bFound = false;

	for (const TObjectPtr<AAeroDisasterActor>& Entry : Instances)
	{
		const AAeroDisasterActor* Instance = Entry.Get();
		if (!Instance)
		{
			continue;
		}

		if (Instance->Phase == EAeroDisasterPhase::Idle || Instance->Phase == EAeroDisasterPhase::Terminated)
		{
			continue;
		}

		const float DistanceCm = FVector::Dist2D(WorldLocation, Instance->GetFootprintCentre());
		if (DistanceCm < OutDistanceCm)
		{
			OutDistanceCm = DistanceCm;
			OutInstance = Instance->MakeInstanceSnapshot();
			bFound = true;
		}
	}

	return bFound;
}

void UAeroDisasterSubsystem::StartScenario(int32 Seed)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (bScenarioRunning)
	{
		DestroyScenarioInstances(false);
	}

	const UAeroSimClockSubsystem* Clock = World->GetSubsystem<UAeroSimClockSubsystem>();
	ScenarioStartSimSeconds = Clock ? Clock->GetSimulationSeconds() : 0.0;
	ScenarioSeed = Seed;
	bScenarioRunning = true;

	FRandomStream Stream(Seed);

	// Anchor on the docking station when the level has one, so the hazards sit in the same frame of
	// reference as the patrol loop rather than at an arbitrary world coordinate.
	FVector Anchor = FVector::ZeroVector;
	const AAeroDockingStation* Station = Cast<AAeroDockingStation>(
		UGameplayStatics::GetActorOfClass(World, AAeroDockingStation::StaticClass()));
	if (Station)
	{
		Anchor = Station->GetActorLocation();
	}

	int32 SpawnedCount = 0;

	for (const FDisasterPlacement& Placement : GScenarioLayout)
	{
		const FVector SpawnOrigin = ResolveSpawnOrigin(Placement.Zone, Stream, Anchor);

		// Stagger the triggers so the drone never faces all four at once: an episode has to contain a
		// decision about where to spend its battery, not a single four-way emergency.
		const float TriggerDelay = 15.0f + Stream.FRandRange(0.0f, 150.0f);

		if (SpawnOne(Placement.Type, SpawnOrigin, TriggerDelay, Stream))
		{
			++SpawnedCount;
		}
	}

	if (bSpawnGroundTruthRecorder && !SpawnedRecorder)
	{
		FActorSpawnParameters RecorderParams;
		RecorderParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		SpawnedRecorder = World->SpawnActor<AAeroGroundTruthRecorder>(
			AAeroGroundTruthRecorder::StaticClass(), FTransform::Identity, RecorderParams);
	}

	UE_LOG(LogAeroSentinel, Log,
		TEXT("Disaster scenario started: seed %d, %d instance(s) spawned, start sim %.1f s"),
		Seed, SpawnedCount, ScenarioStartSimSeconds);
}

void UAeroDisasterSubsystem::StopScenario()
{
	DestroyScenarioInstances(true);
}

void UAeroDisasterSubsystem::ResetScenario(int32 Seed)
{
	DestroyScenarioInstances(false);
	StartScenario(Seed);
}

void UAeroDisasterSubsystem::DestroyScenarioInstances(bool bDestroyRecorder)
{
	// Only the spawned set is destroyed: a hazard a designer placed in the level belongs to the level.
	TArray<TObjectPtr<AAeroDisasterActor>> ToDestroy = SpawnedInstances;
	SpawnedInstances.Reset();

	for (const TObjectPtr<AAeroDisasterActor>& Instance : ToDestroy)
	{
		if (IsValid(Instance))
		{
			Instance->Destroy();
		}
	}

	if (bDestroyRecorder && IsValid(SpawnedRecorder))
	{
		SpawnedRecorder->Destroy();
		SpawnedRecorder = nullptr;
	}

	Instances.RemoveAll([](const TObjectPtr<AAeroDisasterActor>& Entry) { return !IsValid(Entry); });

	bScenarioRunning = false;
}

FVector UAeroDisasterSubsystem::ResolveSpawnOrigin(EAeroZoneId PreferredZone, FRandomStream& Stream, const FVector& Anchor) const
{
	const UWorld* World = GetWorld();

	if (World)
	{
		if (const UAeroRegionSubsystem* Regions = World->GetSubsystem<UAeroRegionSubsystem>())
		{
			if (Regions->GetZoneVolume(PreferredZone) != nullptr)
			{
				const FVector Centre = Regions->GetZoneCenter(PreferredZone);
				const FVector Extent = Regions->GetZoneBounds(PreferredZone).GetExtent();

				// Jitter stays inside the zone but never wanders far enough to leave it.
				const float JitterX = FMath::Min(Extent.X * 0.4f, 4000.0f);
				const float JitterY = FMath::Min(Extent.Y * 0.4f, 4000.0f);

				return FVector(
					Centre.X + Stream.FRandRange(-JitterX, JitterX),
					Centre.Y + Stream.FRandRange(-JitterY, JitterY),
					Centre.Z);
			}
		}
	}

	// No zone volume for this family yet (a streamed World Partition cell that has not loaded, or a
	// level without zones). Place it on a ring close to the anchor and say exactly that, rather than
	// pretending the hazard is in a region it is not.
	UE_LOG(LogAeroSentinel, Warning,
		TEXT("No zone volume for %s; placing that disaster on the fallback ring around %s"),
		*UAeroRegionSubsystem::ZoneIdToName(PreferredZone), *Anchor.ToCompactString());

	const float Angle = Stream.FRandRange(0.0f, 2.0f * PI);
	const float Radius = Stream.FRandRange(8000.0f, 22000.0f);

	return FVector(
		Anchor.X + FMath::Cos(Angle) * Radius,
		Anchor.Y + FMath::Sin(Angle) * Radius,
		Anchor.Z);
}

AAeroDisasterActor* UAeroDisasterSubsystem::SpawnOne(EAeroDisasterType Type, const FVector& SpawnOrigin, float TriggerDelay, FRandomStream& Stream)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TSubclassOf<AAeroDisasterActor> Class = nullptr;

	switch (Type)
	{
	case EAeroDisasterType::Flood:
		Class = AAeroFloodActor::StaticClass();
		break;

	case EAeroDisasterType::ForestFire:
		Class = AAeroFireActor::StaticClass();
		break;

	case EAeroDisasterType::Landslide:
		Class = AAeroLandslideActor::StaticClass();
		break;

	case EAeroDisasterType::StructuralDamage:
		Class = AAeroStructureActor::StaticClass();
		break;

	default:
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAeroDisasterActor* Instance = World->SpawnActor<AAeroDisasterActor>(
		Class, FTransform(FRotator::ZeroRotator, SpawnOrigin), Params);

	if (!Instance)
	{
		UE_LOG(LogAeroSentinel, Error, TEXT("Disaster scenario failed to spawn a %s at %s"),
			*AeroDisasterNames::TypeToName(Type), *SpawnOrigin.ToCompactString());
		return nullptr;
	}

	// Applied after spawn: the actor reads these every tick, so BeginPlay has already registered it.
	Instance->TriggerDelaySimSeconds = TriggerDelay;
	Instance->PeakSeverity = Stream.FRandRange(0.65f, 1.0f);

	SpawnedInstances.Add(Instance);

	return Instance;
}
