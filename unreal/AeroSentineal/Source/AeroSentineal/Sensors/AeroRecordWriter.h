// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class UAeroSensorComponent;

/**
 * Shared record construction for every virtual sensor.
 *
 * A frame and a scalar reading taken at the same simulation instant must share an identical header,
 * otherwise the dataset cannot be joined without string parsing. Everything here is that header; a
 * sensor only contributes its own measurement block on top.
 *
 * This is the bootstrap path: Phase 7's data subsystem subscribes to UAeroSensorComponent::OnSampleTaken
 * and takes the write over, at which point these helpers become its building blocks rather than the
 * sensors' own.
 */
namespace AeroRecord
{
	/**
	 * Identity, timing, pose and region for one sample.
	 *
	 * Reads the world for the region lookup and the owning airframe for vehicle context, which is the
	 * one place a sensor touches systems above it. Kept in this single file so the coupling has exactly
	 * one home.
	 */
	AEROSENTINEAL_API TSharedRef<FJsonObject> BuildBaseRecord(const UAeroSensorComponent* Sensor, double SimulationSeconds);

	/**
	 * Region containing a world point: region_id and region_name of the highest-priority containing zone.
	 * Returns false when the point is outside every zone, in which case id is "None".
	 */
	AEROSENTINEAL_API bool ResolveRegion(const UWorld* World, const FVector& WorldLocation, FString& OutRegionId, FString& OutRegionName);

	/** <dataset root>/<SensorName>/<DroneId>/ — where this sensor's records belong. */
	AEROSENTINEAL_API FString BuildSensorDirectory(const UAeroSensorComponent* Sensor);

	/** <sensorlower>_<YYYYMMDDTHHMMSS>_<index 6 digits>, the base name a record and its frame share. */
	AEROSENTINEAL_API FString BuildBaseFileName(const UAeroSensorComponent* Sensor);

	/** Pretty-prints and saves Directory/<BaseFileName>.json. Returns the path, or empty on failure. */
	AEROSENTINEAL_API FString SaveRecord(const TSharedRef<FJsonObject>& Record, const FString& Directory, const FString& BaseFileName);

	/** Serializes on the game thread and performs directory/file I/O on the thread pool. */
	AEROSENTINEAL_API void SaveRecordAsync(const TSharedRef<FJsonObject>& Record, const FString& Directory, const FString& BaseFileName);

	/**
	 * Deterministic unit-variance gaussian for a sensor's noise model.
	 *
	 * Seeded from the sample index, the simulation instant and a per-channel salt, so replaying a run
	 * reproduces every noisy reading exactly - which is what makes a measured/ground-truth pair stable
	 * enough to train against. ChannelSalt keeps two channels of one sample independent.
	 */
	AEROSENTINEAL_API float GaussianSample(int32 SampleIndex, double SimulationSeconds, int32 ChannelSalt);

	/** Deterministic uniform in [0,1) for decisions such as dropout, seeded the same way. */
	AEROSENTINEAL_API float DeterministicFraction(int32 SampleIndex, double SimulationSeconds, int32 ChannelSalt);
}
