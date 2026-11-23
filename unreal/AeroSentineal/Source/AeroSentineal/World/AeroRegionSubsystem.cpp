// Copyright AeroSentinel. All Rights Reserved.

#include "World/AeroRegionSubsystem.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "World/AeroZoneVolume.h"

bool UAeroRegionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE
		|| WorldType == EWorldType::Editor;
}

void UAeroRegionSubsystem::CollectZones(TArray<AAeroZoneVolume*>& OutZones) const
{
	OutZones.Reset();

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<AAeroZoneVolume> It(World); It; ++It)
	{
		AAeroZoneVolume* Zone = *It;
		if (IsValid(Zone) && Zone->ZoneId != EAeroZoneId::None)
		{
			OutZones.Add(Zone);
		}
	}
}

bool UAeroRegionSubsystem::TryGetZoneAtLocation(const FVector& WorldLocation, EAeroZoneId& OutZone) const
{
	TArray<AAeroZoneVolume*> Zones;
	CollectZones(Zones);

	AAeroZoneVolume* Best = nullptr;
	for (AAeroZoneVolume* Zone : Zones)
	{
		if (!Zone->ContainsLocation(WorldLocation))
		{
			continue;
		}

		if (Best == nullptr || Zone->Priority > Best->Priority)
		{
			Best = Zone;
		}
	}

	if (Best == nullptr)
	{
		OutZone = EAeroZoneId::None;
		return false;
	}

	OutZone = Best->ZoneId;
	return true;
}

EAeroZoneId UAeroRegionSubsystem::GetZoneAtLocation(const FVector& WorldLocation) const
{
	EAeroZoneId Found = EAeroZoneId::None;
	TryGetZoneAtLocation(WorldLocation, Found);
	return Found;
}

AAeroZoneVolume* UAeroRegionSubsystem::GetZoneVolume(EAeroZoneId ZoneId) const
{
	if (ZoneId == EAeroZoneId::None)
	{
		return nullptr;
	}

	TArray<AAeroZoneVolume*> Zones;
	CollectZones(Zones);

	for (AAeroZoneVolume* Zone : Zones)
	{
		if (Zone->ZoneId == ZoneId)
		{
			return Zone;
		}
	}

	return nullptr;
}

FBox UAeroRegionSubsystem::GetZoneBounds(EAeroZoneId ZoneId) const
{
	const AAeroZoneVolume* Zone = GetZoneVolume(ZoneId);
	return Zone != nullptr ? Zone->GetZoneBounds() : FBox(ForceInit);
}

FVector UAeroRegionSubsystem::GetZoneCenter(EAeroZoneId ZoneId) const
{
	const FBox Bounds = GetZoneBounds(ZoneId);
	return Bounds.IsValid != 0 ? Bounds.GetCenter() : FVector::ZeroVector;
}

void UAeroRegionSubsystem::GetAllZoneIds(TArray<EAeroZoneId>& OutZones) const
{
	OutZones.Reset();

	TArray<AAeroZoneVolume*> Zones;
	CollectZones(Zones);

	for (const AAeroZoneVolume* Zone : Zones)
	{
		OutZones.AddUnique(Zone->ZoneId);
	}
}

int32 UAeroRegionSubsystem::GetZoneCount() const
{
	TArray<AAeroZoneVolume*> Zones;
	CollectZones(Zones);
	return Zones.Num();
}

FString UAeroRegionSubsystem::ZoneIdToName(EAeroZoneId ZoneId)
{
	const UEnum* Enum = StaticEnum<EAeroZoneId>();
	if (Enum == nullptr)
	{
		return TEXT("None");
	}

	return Enum->GetNameStringByValue(static_cast<int64>(ZoneId));
}
