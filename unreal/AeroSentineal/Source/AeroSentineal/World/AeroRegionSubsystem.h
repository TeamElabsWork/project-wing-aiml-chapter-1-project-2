// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/AeroTypes.h"
#include "AeroRegionSubsystem.generated.h"

class AAeroZoneVolume;

/**
 * Answers "which region is this world location in?" for the whole simulation.
 *
 * The zone set is small (five volumes), so the actor list is walked on each query rather than
 * cached: there is no invalidation bug to have, and it returns the same answer in the editor as
 * during play, which is what makes zone placement verifiable without pressing Play.
 */
UCLASS()
class AEROSENTINEAL_API UAeroRegionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin UWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem

	/** Highest-priority zone containing the point. Returns false when the point is outside every zone. */
	UFUNCTION(BlueprintPure, Category = "AeroSentinel|Region")
	bool TryGetZoneAtLocation(const FVector& WorldLocation, EAeroZoneId& OutZone) const;

	/** Convenience wrapper returning EAeroZoneId::None when outside every zone. */
	UFUNCTION(BlueprintPure, Category = "AeroSentinel|Region")
	EAeroZoneId GetZoneAtLocation(const FVector& WorldLocation) const;

	/** The volume actor backing a zone, or null when the level has no such zone. */
	UFUNCTION(BlueprintPure, Category = "AeroSentinel|Region")
	AAeroZoneVolume* GetZoneVolume(EAeroZoneId ZoneId) const;

	UFUNCTION(BlueprintPure, Category = "AeroSentinel|Region")
	FBox GetZoneBounds(EAeroZoneId ZoneId) const;

	UFUNCTION(BlueprintPure, Category = "AeroSentinel|Region")
	FVector GetZoneCenter(EAeroZoneId ZoneId) const;

	/** Every zone id that actually has a volume in this world, excluding None. */
	UFUNCTION(BlueprintCallable, Category = "AeroSentinel|Region")
	void GetAllZoneIds(TArray<EAeroZoneId>& OutZones) const;

	UFUNCTION(BlueprintPure, Category = "AeroSentinel|Region")
	int32 GetZoneCount() const;

	/** Enum entry name, e.g. "RiverFloodplain" - the exact string written as region_id. */
	UFUNCTION(BlueprintPure, Category = "AeroSentinel|Region")
	static FString ZoneIdToName(EAeroZoneId ZoneId);

private:
	void CollectZones(TArray<AAeroZoneVolume*>& OutZones) const;
};
