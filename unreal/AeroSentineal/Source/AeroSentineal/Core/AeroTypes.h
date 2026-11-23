// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AeroTypes.generated.h"

/**
 * Shared vocabulary for the whole simulation. Every later module (drone FSM, disaster engine,
 * sensors, patrol, data export) speaks these types, which is what keeps the modules decoupled:
 * no module needs to include another module's headers just to name a state or a disaster.
 */

/** Operating state of a drone airframe. Drives the mission finite state machine (Phase 3). */
UENUM(BlueprintType)
enum class EAeroDroneState : uint8
{
	Docked			UMETA(DisplayName = "Docked"),
	TakingOff		UMETA(DisplayName = "Taking Off"),
	Ascending		UMETA(DisplayName = "Ascending"),
	Patrolling		UMETA(DisplayName = "Patrolling"),
	Investigating	UMETA(DisplayName = "Investigating"),
	Returning		UMETA(DisplayName = "Returning"),
	Landing			UMETA(DisplayName = "Landing"),
	Charging		UMETA(DisplayName = "Charging"),
	Idle			UMETA(DisplayName = "Idle")
};

/** Disaster families supported by the disaster scenario engine (Phase 4). */
UENUM(BlueprintType)
enum class EAeroDisasterType : uint8
{
	None				UMETA(DisplayName = "None"),
	Flood				UMETA(DisplayName = "Flood"),
	ForestFire			UMETA(DisplayName = "Forest Fire"),
	Landslide			UMETA(DisplayName = "Landslide"),
	StructuralDamage	UMETA(DisplayName = "Structural Damage")
};

/** Life-cycle phase of a single disaster instance. Progression is driven by the sim clock. */
UENUM(BlueprintType)
enum class EAeroDisasterPhase : uint8
{
	Idle			UMETA(DisplayName = "Idle"),
	Triggered		UMETA(DisplayName = "Triggered"),
	Developing		UMETA(DisplayName = "Developing"),
	Peak			UMETA(DisplayName = "Peak"),
	Receding		UMETA(DisplayName = "Receding"),
	Terminated		UMETA(DisplayName = "Terminated")
};

/** Geographical zones of the simulation region. Stored as region_id in every data record. */
UENUM(BlueprintType)
enum class EAeroZoneId : uint8
{
	None				UMETA(DisplayName = "None"),
	Forest				UMETA(DisplayName = "Zone A - Forest"),
	RiverFloodplain		UMETA(DisplayName = "Zone B - River and Floodplain"),
	Village				UMETA(DisplayName = "Zone C - Village"),
	Hilly				UMETA(DisplayName = "Zone D - Hilly Region"),
	Industrial			UMETA(DisplayName = "Zone E - Industrial Region")
};

/** Structural damage progression, recorded verbatim as ML ground truth. */
UENUM(BlueprintType)
enum class EAeroDamageState : uint8
{
	Normal			UMETA(DisplayName = "State 0 - Normal"),
	Minor			UMETA(DisplayName = "State 1 - Minor Damage"),
	Moderate		UMETA(DisplayName = "State 2 - Moderate Damage"),
	Severe			UMETA(DisplayName = "State 3 - Severe Damage"),
	Collapse		UMETA(DisplayName = "State 4 - Collapse")
};

/** Weather presets the environment director blends between. */
UENUM(BlueprintType)
enum class EAeroWeatherPreset : uint8
{
	Clear			UMETA(DisplayName = "Clear"),
	PartlyCloudy	UMETA(DisplayName = "Partly Cloudy"),
	Overcast		UMETA(DisplayName = "Overcast"),
	Rain			UMETA(DisplayName = "Rain"),
	Storm			UMETA(DisplayName = "Storm"),
	Fog				UMETA(DisplayName = "Fog")
};

/**
 * Physical description of the atmosphere at one instant, with no reference to lights or actors.
 * The weather director blends between presets and exposes this; Phase 5 sensors read it as their
 * ground truth. Keeping it plain data is what stops the sensor suite depending on the world module.
 */
USTRUCT(BlueprintType)
struct FAeroWeatherState
{
	GENERATED_BODY()

	/** Directional light intensity for the atmospheric sun. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	float SunIntensity = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	float SkyLightIntensity = 1.0f;

	/** Exponential height fog density. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	float FogDensity = 0.008f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	float FogHeightFalloff = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	FLinearColor FogColor = FLinearColor(0.35f, 0.42f, 0.50f, 1.0f);

	/** 0 = clear sky, 1 = fully overcast. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	float CloudCoverage = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	float PrecipitationMmPerHour = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	float WindSpeedMps = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	float AirTemperatureC = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	float VisibilityMeters = 25000.0f;
};

/**
 * A pose expressed in both frames at once. Produced by UAeroGeoSubsystem::GetGeoPose so a
 * record never has to be converted twice, and every dataset row carries lat/lon/alt plus the
 * engine-space location that produced it.
 */
USTRUCT(BlueprintType)
struct FAeroGeoPose
{
	GENERATED_BODY()

	/** WGS84 latitude in degrees. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Geo")
	double Latitude = 0.0;

	/** WGS84 longitude in degrees. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Geo")
	double Longitude = 0.0;

	/** Altitude above the WGS84 ellipsoid, in metres. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Geo")
	double AltitudeMeters = 0.0;

	/** Same pose in Unreal world space (centimetres). */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Geo")
	FVector EngineLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Aero|Geo")
	FRotator EngineRotation = FRotator::ZeroRotator;
};

/**
 * One reinforcement-learning observation of the airframe.
 *
 * Locations are carried both in world space and relative to HomeLocation, because a policy's input
 * scaling must not change when a docking station is moved. The numeric form a training loop actually
 * consumes is UAeroDronePawn::GetObservationVector(); this struct is the readable equivalent.
 */
USTRUCT(BlueprintType)
struct FAeroRLObservation
{
	GENERATED_BODY()

	/** World location, cm. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|RL")
	FVector Location = FVector::ZeroVector;

	/** Location relative to HomeLocation, cm. This is the frame a policy should see. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|RL")
	FVector LocationRelativeToHome = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Aero|RL")
	FRotator Rotation = FRotator::ZeroRotator;

	/** World-space velocity, cm/s. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|RL")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Aero|RL")
	float AltitudeAboveHomeCm = 0.0f;

	/** 0..1 state of charge. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|RL")
	float BatteryFraction = 0.0f;

	/** Distance to the waypoint the mission is currently flying to, cm. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|RL")
	float DistanceToWaypointCm = 0.0f;

	/** True when an external agent owns the airframe. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|RL")
	bool bExternalControl = false;

	/** Named region containing the airframe, from the zone volumes. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|RL")
	FString RegionId;

	UPROPERTY(BlueprintReadOnly, Category = "Aero|RL")
	FString DroneState;

	/** Nearest active hazard of any family. None when the scenario has no live instance. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|RL")
	EAeroDisasterType NearestHazardType = EAeroDisasterType::None;

	/** Distance to that instance's origin, cm. 1e9 when nothing is active. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|RL")
	float NearestHazardDistanceCm = 1.0e9f;

	/** Severity of the hazard whose footprint contains the airframe, 0 when in clean air. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|RL")
	float HazardSeverityAtLocation = 0.0f;

	/** Air temperature a sensor would read because of nearby hazards, Celsius added to ambient. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|RL")
	float LocalTemperatureDeltaC = 0.0f;

	/** PM2.5 a sensor would read because of nearby hazards, ug/m3 added to the ambient background. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|RL")
	float LocalPm25DeltaUgPerM3 = 0.0f;
};

/**
 * Ground truth for one disaster instance, as it stands at one instant.
 *
 * This is the label side of the dataset: what was actually happening, never what a sensor guessed.
 * It is produced by the live instance (AAeroDisasterActor) through the disaster subsystem, so a record
 * can only ever carry truth that a running simulation actually generated.
 */
USTRUCT(BlueprintType)
struct FAeroGroundTruth
{
	GENERATED_BODY()

	/** None when the observation point is outside every active instance. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	EAeroDisasterType DisasterType = EAeroDisasterType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	EAeroDisasterPhase DisasterPhase = EAeroDisasterPhase::Idle;

	/** Unique id of the instance that produced this truth; empty when there is none. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	FString InstanceId;

	/** Life-cycle severity of the instance, 0..1. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	float Severity = 0.0f;

	/** Distance from the observation point to the instance origin, cm. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	float DistanceToOriginCm = 0.0f;

	/** Footprint radius of the instance at this instant, cm. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	float AffectedRadiusCm = 0.0f;

	/** Standing water at the observation point, cm. Zero for every non-flood instance. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	float WaterDepthCm = 0.0f;

	/** Radiant load from a fire at the observation point, 0..1. Zero for every non-fire instance. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	float FireIntensity = 0.0f;
};

/**
 * A live disaster instance, flattened for queries and for the ground-truth timeline.
 * Mirrors the state AAeroDisasterActor is driving, so a consumer never has to touch the actor.
 */
USTRUCT(BlueprintType)
struct FAeroDisasterInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	FString InstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	EAeroDisasterType DisasterType = EAeroDisasterType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	EAeroDisasterPhase Phase = EAeroDisasterPhase::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	FVector Origin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	float Severity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	float AffectedRadiusCm = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	float WaterDepthCm = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	float FireIntensity = 0.0f;

	/** Simulation seconds since the instance triggered; negative before it triggers. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	double ElapsedSimSeconds = 0.0;
};

/**
 * Extra atmosphere a hazard puts at one world point, over and above the weather director's ambient.
 *
 * Deliberately additive and unit-tagged, because several instances superpose: a fire burning beside a
 * river both warms and cools the same parcel of air. Sensors add this onto the ambient state rather
 * than reading a hazard actor directly, which is what keeps the sensor suite unaware of the disaster
 * module's class hierarchy.
 */
USTRUCT(BlueprintType)
struct FAeroEnvironmentalDelta
{
	GENERATED_BODY()

	/** Celsius added to ambient air temperature. Negative for evaporative cooling. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	float TemperatureDeltaC = 0.0f;

	/** ppm of CO2 added to the ambient background. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	float Co2DeltaPpm = 0.0f;

	/** ug/m3 of PM2.5 added to the ambient background. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	float Pm25DeltaUgPerM3 = 0.0f;

	/** Fraction of the ambient visibility removed, 0..1. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|Disaster")
	float VisibilityLossFraction = 0.0f;
};

/**
 * An external anomaly request. The patrol layer owns the request lifecycle; the detector that
 * produced it can be an external ML process, a replay file, or a Blueprint test trigger.
 */
USTRUCT(BlueprintType)
struct FAeroAnomalyRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Anomaly")
	FString RequestId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Anomaly")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Anomaly")
	float DwellSeconds = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Anomaly")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Anomaly")
	FString Source = TEXT("external");
};

/**
 * Enum spelling for dataset records: the bare entry name, never "EAeroDisasterType::Flood".
 * Kept here so the record writer and the disaster layer cannot drift apart on the label strings.
 */
namespace AeroDisasterNames
{
	inline FString TypeToName(EAeroDisasterType Type)
	{
		// Spelled out rather than reflected: a dataset label is part of the schema, so it must not change
		// just because an enum entry is reordered.
		switch (Type)
		{
		case EAeroDisasterType::Flood:
			return TEXT("Flood");
		case EAeroDisasterType::ForestFire:
			return TEXT("ForestFire");
		case EAeroDisasterType::Landslide:
			return TEXT("Landslide");
		case EAeroDisasterType::StructuralDamage:
			return TEXT("StructuralDamage");
		case EAeroDisasterType::None:
		default:
			return TEXT("None");
		}
	}

	inline FString PhaseToName(EAeroDisasterPhase Phase)
	{
		switch (Phase)
		{
		case EAeroDisasterPhase::Triggered:
			return TEXT("Triggered");
		case EAeroDisasterPhase::Developing:
			return TEXT("Developing");
		case EAeroDisasterPhase::Peak:
			return TEXT("Peak");
		case EAeroDisasterPhase::Receding:
			return TEXT("Receding");
		case EAeroDisasterPhase::Terminated:
			return TEXT("Terminated");
		case EAeroDisasterPhase::Idle:
		default:
			return TEXT("Idle");
		}
	}
}
