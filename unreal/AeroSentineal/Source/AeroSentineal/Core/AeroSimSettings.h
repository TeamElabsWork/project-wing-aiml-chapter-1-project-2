// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AeroSimSettings.generated.h"

/**
 * Project-wide configuration for the AeroSentinel simulation.
 *
 * Found in the editor under Project Settings -> Game -> AeroSentinel Simulation, and persisted
 * to Config/DefaultGame.ini. This is the single source of truth shared by the Unreal side and
 * the external B1/B2 Python modules (which read the same values from Simulation/Config).
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "AeroSentinel Simulation"))
class AEROSENTINEAL_API UAeroSimSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAeroSimSettings();

	// ---------------------------------------------------------------- Data

	/** Dataset root, relative to the project directory. Receives every exported record. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Data")
	FString DatasetRootDirectory = TEXT("Simulation/Datasets");

	/** Identifier stamped into every record produced by this simulation instance. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Data")
	FString DroneId = TEXT("DRN-001");

	// ---------------------------------------------------------------- Time

	/** Simulation seconds advanced per real second at start-up (e.g. 60 = one minute per second). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Time", meta = (ClampMin = "0.01", ClampMax = "3600.0"))
	float DefaultTimeMultiplier = 1.0f;

	/** UTC instant the simulation clock starts from. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Time", meta = (ClampMin = "1970", ClampMax = "2200"))
	int32 StartYear = 2026;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Time", meta = (ClampMin = "1", ClampMax = "12"))
	int32 StartMonth = 6;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Time", meta = (ClampMin = "1", ClampMax = "31"))
	int32 StartDay = 1;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Time", meta = (ClampMin = "0", ClampMax = "23"))
	int32 StartHourUtc = 6;

	// ---------------------------------------------------------------- Geo

	/** WGS84 latitude of the Unreal world origin. Must match the level's GeoReferencingSystem actor. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Geo")
	double OriginLatitude = 0.0;

	/** WGS84 longitude of the Unreal world origin. Must match the level's GeoReferencingSystem actor. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Geo")
	double OriginLongitude = 0.0;

	/** Altitude (metres above ellipsoid) of the Unreal world origin. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Geo")
	double OriginAltitude = 0.0;

	// ---------------------------------------------------------------- Helpers

	/** Absolute on-disk dataset root: project directory + DatasetRootDirectory. */
	UFUNCTION(BlueprintPure, Category = "AeroSentinel|Data")
	FString GetAbsoluteDatasetRoot() const;

	/** UTC instant the simulation clock starts from, built from the Start* fields. */
	UFUNCTION(BlueprintPure, Category = "AeroSentinel|Time")
	FDateTime GetStartUtc() const;
};
