// Copyright AeroSentinel. All Rights Reserved.

#include "Core/AeroGeoSubsystem.h"
#include "Core/AeroLog.h"
#include "GeoReferencingSystem.h"

void UAeroGeoSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Deliberately no actor lookup here: world subsystems initialise BEFORE the level's actors do,
	// so a check at this point would always fail and log a misleading warning. ResolveGeoSystem()
	// is lazy and retries on every call; OnWorldBeginPlay() reports a genuine absence.
}

void UAeroGeoSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (ResolveGeoSystem() == nullptr)
	{
		UE_LOG(LogAeroSentinel, Warning,
			TEXT("No GeoReferencingSystem actor in this world - geographic conversion is unavailable. "
				 "Add one to the level to produce WGS84 dataset records."));
	}
}

bool UAeroGeoSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Editor is included so level-authoring tools (zone volumes, docking pad placement) can preview
	// coordinates without entering PIE.
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE
		|| WorldType == EWorldType::GamePreview
		|| WorldType == EWorldType::Editor;
}

AGeoReferencingSystem* UAeroGeoSubsystem::ResolveGeoSystem() const
{
	if (CachedGeoSystem.IsValid())
	{
		return CachedGeoSystem.Get();
	}

	AGeoReferencingSystem* Resolved = AGeoReferencingSystem::GetGeoReferencingSystem(GetWorld());
	CachedGeoSystem = Resolved;
	return Resolved;
}

bool UAeroGeoSubsystem::IsGeoReferenceReady() const
{
	return ResolveGeoSystem() != nullptr;
}

bool UAeroGeoSubsystem::EngineToGeographic(const FVector& EngineLocation, FGeographicCoordinates& OutGeo) const
{
	AGeoReferencingSystem* GeoSystem = ResolveGeoSystem();
	if (GeoSystem == nullptr)
	{
		OutGeo = FGeographicCoordinates();
		return false;
	}

	GeoSystem->EngineToGeographic(EngineLocation, OutGeo);
	return true;
}

bool UAeroGeoSubsystem::GeographicToEngine(const FGeographicCoordinates& Geo, FVector& OutEngineLocation) const
{
	AGeoReferencingSystem* GeoSystem = ResolveGeoSystem();
	if (GeoSystem == nullptr)
	{
		OutEngineLocation = FVector::ZeroVector;
		return false;
	}

	GeoSystem->GeographicToEngine(Geo, OutEngineLocation);
	return true;
}

bool UAeroGeoSubsystem::GetGeoPose(const FVector& EngineLocation, const FRotator& EngineRotation, FAeroGeoPose& OutPose) const
{
	FGeographicCoordinates Geo;
	if (!EngineToGeographic(EngineLocation, Geo))
	{
		OutPose = FAeroGeoPose();
		return false;
	}

	OutPose.Latitude = Geo.Latitude;
	OutPose.Longitude = Geo.Longitude;
	OutPose.AltitudeMeters = Geo.Altitude;
	OutPose.EngineLocation = EngineLocation;
	OutPose.EngineRotation = EngineRotation;
	return true;
}

FString UAeroGeoSubsystem::FormatGeoCoordinate(const FGeographicCoordinates& Geo) const
{
	return FString::Printf(TEXT("%.7f, %.7f, %.2fm"), Geo.Latitude, Geo.Longitude, Geo.Altitude);
}
