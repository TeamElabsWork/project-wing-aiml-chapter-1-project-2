// Copyright AeroSentinel. All Rights Reserved.

#include "Sensors/AeroRecordWriter.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Async/Async.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Math/RandomStream.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Core/AeroGeoSubsystem.h"
#include "Core/AeroLog.h"
#include "Core/AeroSimClockSubsystem.h"
#include "Core/AeroSimSettings.h"
#include "Core/AeroTypes.h"
#include "Drone/AeroBatteryComponent.h"
#include "Drone/AeroDronePawn.h"
#include "Disaster/AeroDisasterSubsystem.h"
#include "Sensors/AeroSensorComponent.h"
#include "World/AeroZoneVolume.h"

namespace
{
	/** Strips the "EnumType::" prefix UEnum::GetValueAsString adds. */
	FString ShortEnumName(const FString& FullName)
	{
		int32 SeparatorIndex = INDEX_NONE;
		if (FullName.FindLastChar(TEXT(':'), SeparatorIndex))
		{
			return FullName.Mid(SeparatorIndex + 1);
		}
		return FullName;
	}
}

namespace AeroRecord
{
	bool ResolveRegion(const UWorld* World, const FVector& WorldLocation, FString& OutRegionId, FString& OutRegionName)
	{
		OutRegionId = TEXT("None");
		OutRegionName = TEXT("Outside");

		if (!World)
		{
			return false;
		}

		// Priority decides overlaps, so a floodplain record reads as the floodplain rather than the valley.
		const AAeroZoneVolume* BestZone = nullptr;
		for (TActorIterator<AAeroZoneVolume> It(const_cast<UWorld*>(World)); It; ++It)
		{
			AAeroZoneVolume* Zone = *It;
			if (!Zone || !Zone->ContainsLocation(WorldLocation))
			{
				continue;
			}

			if (!BestZone || Zone->Priority > BestZone->Priority)
			{
				BestZone = Zone;
			}
		}

		if (!BestZone)
		{
			return false;
		}

		OutRegionId = ShortEnumName(UEnum::GetValueAsString(BestZone->ZoneId));
		OutRegionName = BestZone->ZoneDisplayName.IsEmpty() ? OutRegionId : BestZone->ZoneDisplayName;
		return true;
	}

	FString BuildSensorDirectory(const UAeroSensorComponent* Sensor)
	{
		const UAeroSimSettings* Settings = GetDefault<UAeroSimSettings>();
		const FString SensorName = Sensor ? Sensor->SensorName : TEXT("Sensor");
		const FString DroneId = Settings ? Settings->DroneId : TEXT("DRN-001");
		const FString Root = Settings ? Settings->GetAbsoluteDatasetRoot() : FPaths::ProjectDir();

		return FPaths::Combine(Root, SensorName, DroneId);
	}

	FString BuildBaseFileName(const UAeroSensorComponent* Sensor)
	{
		if (!Sensor)
		{
			return TEXT("sensor_unknown");
		}

		FString TimestampToken;
		if (const UWorld* World = Sensor->GetWorld())
		{
			if (const UAeroSimClockSubsystem* Clock = World->GetSubsystem<UAeroSimClockSubsystem>())
			{
				// ISO-8601 carries ':' characters, which Windows rejects in a filename.
				const FDateTime Utc = Clock->GetSimulationUtc();
				TimestampToken = FString::Printf(TEXT("%04d%02d%02dT%02d%02d%02d"),
					Utc.GetYear(), Utc.GetMonth(), Utc.GetDay(),
					Utc.GetHour(), Utc.GetMinute(), Utc.GetSecond());
			}
		}

		return FString::Printf(TEXT("%s_%s_%06d"), *Sensor->SensorName.ToLower(), *TimestampToken, Sensor->SampleCount);
	}

	TSharedRef<FJsonObject> BuildBaseRecord(const UAeroSensorComponent* Sensor, double SimulationSeconds)
	{
		const UWorld* World = Sensor ? Sensor->GetWorld() : nullptr;
		const UAeroSimSettings* Settings = GetDefault<UAeroSimSettings>();
		const AAeroDronePawn* Drone = Sensor ? Cast<AAeroDronePawn>(Sensor->GetOwner()) : nullptr;

		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("schema"), TEXT("aero.sensor_record/1"));
		Root->SetStringField(TEXT("drone_id"), Settings ? Settings->DroneId : TEXT("DRN-001"));
		Root->SetStringField(TEXT("sensor"), Sensor ? Sensor->SensorName : TEXT("Unknown"));
		Root->SetNumberField(TEXT("sample_index"), Sensor ? Sensor->SampleCount : -1);
		Root->SetNumberField(TEXT("sim_seconds"), SimulationSeconds);

		FString UtcIso;
		if (World)
		{
			if (const UAeroSimClockSubsystem* Clock = World->GetSubsystem<UAeroSimClockSubsystem>())
			{
				UtcIso = Clock->GetSimulationUtcIso8601();
			}
		}
		Root->SetStringField(TEXT("utc_iso"), UtcIso);

		// Pose in both frames from the one code path that owns the WGS84 contract.
		const FVector Location = Sensor ? Sensor->GetComponentLocation() : FVector::ZeroVector;
		const FRotator Rotation = Sensor ? Sensor->GetComponentRotation() : FRotator::ZeroRotator;

		FAeroGeoPose Pose;
		if (World)
		{
			if (const UAeroGeoSubsystem* Geo = World->GetSubsystem<UAeroGeoSubsystem>())
			{
				Geo->GetGeoPose(Location, Rotation, Pose);
			}
		}

		TSharedRef<FJsonObject> LocationObject = MakeShared<FJsonObject>();
		LocationObject->SetNumberField(TEXT("x"), Pose.EngineLocation.X);
		LocationObject->SetNumberField(TEXT("y"), Pose.EngineLocation.Y);
		LocationObject->SetNumberField(TEXT("z"), Pose.EngineLocation.Z);

		TSharedRef<FJsonObject> RotationObject = MakeShared<FJsonObject>();
		RotationObject->SetNumberField(TEXT("pitch"), Pose.EngineRotation.Pitch);
		RotationObject->SetNumberField(TEXT("yaw"), Pose.EngineRotation.Yaw);
		RotationObject->SetNumberField(TEXT("roll"), Pose.EngineRotation.Roll);

		TSharedRef<FJsonObject> Wgs84 = MakeShared<FJsonObject>();
		Wgs84->SetNumberField(TEXT("latitude"), Pose.Latitude);
		Wgs84->SetNumberField(TEXT("longitude"), Pose.Longitude);
		Wgs84->SetNumberField(TEXT("altitude_m"), Pose.AltitudeMeters);

		TSharedRef<FJsonObject> PoseObject = MakeShared<FJsonObject>();
		PoseObject->SetObjectField(TEXT("engine_location_cm"), LocationObject);
		PoseObject->SetObjectField(TEXT("engine_rotation_deg"), RotationObject);
		PoseObject->SetObjectField(TEXT("wgs84"), Wgs84);
		Root->SetObjectField(TEXT("pose"), PoseObject);

		// Region attribution: every record is locatable by named area, not just by coordinates.
		FString RegionId;
		FString RegionName;
		const bool bInZone = ResolveRegion(World, Location, RegionId, RegionName);
		TSharedRef<FJsonObject> Region = MakeShared<FJsonObject>();
		Region->SetStringField(TEXT("region_id"), RegionId);
		Region->SetStringField(TEXT("region_name"), RegionName);
		Region->SetBoolField(TEXT("inside_zone_volume"), bInZone);
		Root->SetObjectField(TEXT("region"), Region);

		// Vehicle context, present only for sensors mounted on the airframe.
		if (Drone)
		{
			TSharedRef<FJsonObject> Vehicle = MakeShared<FJsonObject>();
			Vehicle->SetStringField(TEXT("state"), ShortEnumName(UEnum::GetValueAsString(Drone->GetDroneState())));

			if (const UAeroBatteryComponent* Battery = Drone->GetBattery())
			{
				Vehicle->SetNumberField(TEXT("battery_fraction"), Battery->GetStateOfChargeFraction());
				Vehicle->SetNumberField(TEXT("battery_wh"), Battery->RemainingWh);
			}

			Root->SetObjectField(TEXT("vehicle"), Vehicle);
		}

		// Ground truth comes from the live disaster instances, so a record can only ever carry truth a
		// running scenario actually generated. Outside every footprint that truth is None / Idle, and
		// saying so is the correct label rather than a placeholder.
		TSharedRef<FJsonObject> GroundTruth = MakeShared<FJsonObject>();

		FAeroGroundTruth DisasterTruth;
		bool bHazardFound = false;

		if (World)
		{
			if (const UAeroDisasterSubsystem* Disasters = World->GetSubsystem<UAeroDisasterSubsystem>())
			{
				bHazardFound = Disasters->GetGroundTruthAtLocation(Location, DisasterTruth);
			}
		}

		if (bHazardFound)
		{
			GroundTruth->SetStringField(TEXT("disaster_type"), AeroDisasterNames::TypeToName(DisasterTruth.DisasterType));
			GroundTruth->SetStringField(TEXT("disaster_phase"), AeroDisasterNames::PhaseToName(DisasterTruth.DisasterPhase));
			GroundTruth->SetStringField(TEXT("instance_id"), DisasterTruth.InstanceId);
			GroundTruth->SetNumberField(TEXT("severity"), DisasterTruth.Severity);
			GroundTruth->SetNumberField(TEXT("affected_radius_cm"), DisasterTruth.AffectedRadiusCm);
			GroundTruth->SetNumberField(TEXT("distance_to_origin_cm"), DisasterTruth.DistanceToOriginCm);
			GroundTruth->SetNumberField(TEXT("water_depth_cm"), DisasterTruth.WaterDepthCm);
			GroundTruth->SetNumberField(TEXT("fire_intensity"), DisasterTruth.FireIntensity);
		}
		else
		{
			GroundTruth->SetStringField(TEXT("disaster_type"), TEXT("None"));
			GroundTruth->SetStringField(TEXT("disaster_phase"), TEXT("Idle"));
		}

		Root->SetObjectField(TEXT("ground_truth"), GroundTruth);

		return Root;
	}

	FString SaveRecord(const TSharedRef<FJsonObject>& Record, const FString& Directory, const FString& BaseFileName)
	{
		FString Serialized;
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Serialized);
		FJsonSerializer::Serialize(Record, Writer);
		Writer->Close();

		IFileManager::Get().MakeDirectory(*Directory, true);

		const FString RecordPath = FPaths::Combine(Directory, BaseFileName + TEXT(".json"));
		if (!FFileHelper::SaveStringToFile(Serialized, *RecordPath))
		{
			UE_LOG(LogAeroSentinel, Error, TEXT("Failed to write sensor record %s"), *RecordPath);
			return FString();
		}

		return RecordPath;
	}

	void SaveRecordAsync(const TSharedRef<FJsonObject>& Record, const FString& Directory, const FString& BaseFileName)
	{
		FString Serialized;
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Serialized);
		FJsonSerializer::Serialize(Record, Writer);
		Writer->Close();

		Async(EAsyncExecution::ThreadPool, [Serialized = MoveTemp(Serialized), Directory, BaseFileName]()
		{
			IFileManager::Get().MakeDirectory(*Directory, true);
			const FString RecordPath = FPaths::Combine(Directory, BaseFileName + TEXT(".json"));
			if (!FFileHelper::SaveStringToFile(Serialized, *RecordPath))
			{
				UE_LOG(LogAeroSentinel, Error, TEXT("Failed to write sensor record %s"), *RecordPath);
			}
		});
	}

	float GaussianSample(int32 SampleIndex, double SimulationSeconds, int32 ChannelSalt)
	{
		const int32 TimeKey = (int32)FMath::RoundToInt(SimulationSeconds * 100.0);
		int32 Seed = HashCombine(GetTypeHash(SampleIndex), GetTypeHash(TimeKey));
		Seed = HashCombine(Seed, GetTypeHash(ChannelSalt));

		// Box-Muller: two uniforms in, one standard normal out.
		FRandomStream Stream(Seed);
		const float Uniform1 = FMath::Max(Stream.GetFraction(), 1.0e-6f);
		const float Uniform2 = Stream.GetFraction();
		return FMath::Sqrt(-2.0f * FMath::Loge(Uniform1)) * FMath::Cos(2.0f * PI * Uniform2);
	}

	float DeterministicFraction(int32 SampleIndex, double SimulationSeconds, int32 ChannelSalt)
	{
		const int32 TimeKey = (int32)FMath::RoundToInt(SimulationSeconds * 100.0);
		int32 Seed = HashCombine(GetTypeHash(SampleIndex), GetTypeHash(TimeKey));
		Seed = HashCombine(Seed, GetTypeHash(ChannelSalt));

		FRandomStream Stream(Seed);
		return Stream.GetFraction();
	}
}
