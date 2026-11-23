// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GeographicCoordinates.h"
#include "Core/AeroTypes.h"
#include "AeroGeoSubsystem.generated.h"

class AGeoReferencingSystem;

/**
 * The single gateway between Unreal world space and WGS84.
 *
 * Backed by the GeoReferencing plugin (already enabled in this project), deliberately NOT by a
 * hand-rolled projection: the engine's EPSG handling stays authoritative, and every dataset row
 * gets its lat/lon from exactly one code path.
 *
 * The Unreal origin corresponds to the WGS84 point configured on the level's
 * AGeoReferencingSystem actor and mirrored in UAeroSimSettings.
 */
UCLASS()
class AEROSENTINEAL_API UAeroGeoSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin UWorldSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem

	/** True once a GeoReferencingSystem actor has been found in the world. */
	UFUNCTION(BlueprintPure, Category = "AeroSentinel|Geo")
	bool IsGeoReferenceReady() const;

	/** Unreal world location -> WGS84. Returns false when the world has no GeoReferencingSystem. */
	UFUNCTION(BlueprintPure, Category = "AeroSentinel|Geo")
	bool EngineToGeographic(const FVector& EngineLocation, FGeographicCoordinates& OutGeo) const;

	/** WGS84 -> Unreal world location. Returns false when the world has no GeoReferencingSystem. */
	UFUNCTION(BlueprintPure, Category = "AeroSentinel|Geo")
	bool GeographicToEngine(const FGeographicCoordinates& Geo, FVector& OutEngineLocation) const;

	/** Both frames in one call, which is the form every data record wants. */
	UFUNCTION(BlueprintPure, Category = "AeroSentinel|Geo")
	bool GetGeoPose(const FVector& EngineLocation, const FRotator& EngineRotation, FAeroGeoPose& OutPose) const;

	/** Human-readable "lat, lon, alt" used by the HUD, logs and filename metadata. */
	UFUNCTION(BlueprintPure, Category = "AeroSentinel|Geo")
	FString FormatGeoCoordinate(const FGeographicCoordinates& Geo) const;

private:
	/** Lazily resolves and caches the world's GeoReferencingSystem actor. */
	AGeoReferencingSystem* ResolveGeoSystem() const;

	mutable TWeakObjectPtr<AGeoReferencingSystem> CachedGeoSystem;
};
