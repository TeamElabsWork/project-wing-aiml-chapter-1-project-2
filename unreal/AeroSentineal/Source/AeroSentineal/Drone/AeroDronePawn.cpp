// Copyright AeroSentinel. All Rights Reserved.

#include "Drone/AeroDronePawn.h"

#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Core/AeroLog.h"
#include "Drone/AeroBatteryComponent.h"
#include "Sensors/AeroRgbCameraComponent.h"
#include "Sensors/AeroThermalCameraComponent.h"
#include "Sensors/AeroWeatherSensorComponent.h"
#include "Sensors/AeroGasSensorComponent.h"
#include "Sensors/AeroWaterLevelSensorComponent.h"
#include "Sensors/AeroRainfallSensorComponent.h"
#include "Sensors/AeroSoilMoistureSensorComponent.h"
#include "Sensors/AeroTelemetryComponent.h"
#include "Components/SceneComponent.h"
#include "Sensors/AeroRecordWriter.h"
#include "Core/AeroSimClockSubsystem.h"
#include "Disaster/AeroDisasterSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "CollisionQueryParams.h"
#include "UObject/SoftObjectPath.h"

AAeroDronePawn::AAeroDronePawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetGenerateOverlapEvents(false);

	// The Sketchfab airframe imported in Phase 2. Pivot is at the base, span 85 cm.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DroneMeshFinder(
		TEXT("/Game/AeroSentinel/World/Imported/SM_AeroDrone.SM_AeroDrone"));
	if (DroneMeshFinder.Succeeded())
	{
		Body->SetStaticMesh(DroneMeshFinder.Object);
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Body);
	CameraBoom->TargetArmLength = ThirdPersonArmLength;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bUsePawnControlRotation = false;

	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(CameraBoom);
	ViewCamera->bUsePawnControlRotation = false;

	ApplyCameraMount();

	Battery = CreateDefaultSubobject<UAeroBatteryComponent>(TEXT("Battery"));

	// Sensors ride their own gimbal mount, not the camera boom: pulling the viewing camera back must
	// not drag the sensor backwards with it.
	SensorMount = CreateDefaultSubobject<USceneComponent>(TEXT("SensorMount"));
	SensorMount->SetupAttachment(Body);

	RgbCamera = CreateDefaultSubobject<UAeroRgbCameraComponent>(TEXT("RgbCamera"));
	RgbCamera->SetupAttachment(SensorMount);

	ThermalCamera = CreateDefaultSubobject<UAeroThermalCameraComponent>(TEXT("ThermalCamera"));
	ThermalCamera->SetupAttachment(SensorMount);

	WeatherSensor = CreateDefaultSubobject<UAeroWeatherSensorComponent>(TEXT("WeatherSensor"));
	WeatherSensor->SetupAttachment(Body);

	GasSensor = CreateDefaultSubobject<UAeroGasSensorComponent>(TEXT("GasSensor"));
	GasSensor->SetupAttachment(Body);

	WaterLevelSensor = CreateDefaultSubobject<UAeroWaterLevelSensorComponent>(TEXT("WaterLevelSensor"));
	WaterLevelSensor->SetupAttachment(Body);

	RainfallSensor = CreateDefaultSubobject<UAeroRainfallSensorComponent>(TEXT("RainfallSensor"));
	RainfallSensor->SetupAttachment(Body);

	SoilMoistureSensor = CreateDefaultSubobject<UAeroSoilMoistureSensorComponent>(TEXT("SoilMoistureSensor"));
	SoilMoistureSensor->SetupAttachment(Body);

	Telemetry = CreateDefaultSubobject<UAeroTelemetryComponent>(TEXT("Telemetry"));
	Telemetry->SetupAttachment(Body);

	AutoPossessAI = EAutoPossessAI::Disabled;

	// Resolved lazily in ShowHUD(): the widget blueprint derives from UAeroHUDWidget, so it cannot
	// exist at the moment this module is compiled.
	HudWidgetClass = TSoftClassPtr<UUserWidget>(
		FSoftObjectPath(TEXT("/Game/AeroSentinel/UI/WBP_AeroHUD.WBP_AeroHUD_C")));
}

void AAeroDronePawn::BeginPlay()
{
	Super::BeginPlay();

	ApplyCameraMount();

	ShowHUD();

	if (Battery)
	{
		Battery->OnReserveReached.AddDynamic(this, &AAeroDronePawn::HandleReserveReached);
	}

	if (HomeLocation.IsNearlyZero())
	{
		HomeLocation = GetActorLocation();
	}

	if (PatrolPoints.Num() == 0)
	{
		BuildDefaultPatrolLoop();
	}

	SetDroneState(EAeroDroneState::Docked);

	UE_LOG(LogAeroSentinel, Log, TEXT("Drone %s docked at home %s, %d patrol points, cruise altitude %.0f cm"),
		*GetName(), *HomeLocation.ToCompactString(), PatrolPoints.Num(), CruiseAltitude);
}

void AAeroDronePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	StateElapsed += DeltaSeconds;

	const FVector PreviousLocation = GetActorLocation();

	// External control suspends the mission FSM entirely: the policy owns the airframe, and no
	// autonomous state may fight it for the position.
	if (bExternalControlActive)
	{
		const FRotator CurrentRotation = GetActorRotation();
		FVector NewLocation = PreviousLocation + CurrentRotation.RotateVector(CommandedBodyVelocity) * DeltaSeconds;

		// Envelope clamp so a random policy cannot fly the airframe off the map.
		NewLocation.X = FMath::Clamp(NewLocation.X, HomeLocation.X - MaxHorizontalRange, HomeLocation.X + MaxHorizontalRange);
		NewLocation.Y = FMath::Clamp(NewLocation.Y, HomeLocation.Y - MaxHorizontalRange, HomeLocation.Y + MaxHorizontalRange);
		NewLocation.Z = FMath::Clamp(NewLocation.Z, HomeLocation.Z + MinAltitudeAboveHome, HomeLocation.Z + MaxAltitudeAboveHome);
		SetActorLocation(NewLocation, false);

		SetActorRotation(FRotator(CurrentRotation.Pitch,
			CurrentRotation.Yaw + CommandedYawRateDegPerSec * DeltaSeconds,
			CurrentRotation.Roll));

		// Report what actually happened, not what was commanded: a clamped axis is not moving.
		CurrentVelocity = (GetActorLocation() - PreviousLocation) / FMath::Max(DeltaSeconds, KINDA_SMALL_NUMBER);
		return;
	}

	switch (DroneState)
	{
	case EAeroDroneState::Docked:
		// On the pad with a full pack: start the next cycle without being asked, so an
		// unattended session keeps producing patrols.
		if (bAutoRelaunchOnFullCharge && (!Battery || Battery->IsFull()) && StateElapsed >= DockedDwellSeconds)
		{
			SetDroneState(EAeroDroneState::TakingOff);
		}
		break;

	case EAeroDroneState::TakingOff:
		if (StateElapsed >= SpinUpSeconds)
		{
			SetDroneState(EAeroDroneState::Ascending);
		}
		break;

	case EAeroDroneState::Ascending:
	{
		const FVector Target(HomeLocation.X, HomeLocation.Y, HomeLocation.Z + CruiseAltitude);
		MoveTowards(Target, VerticalSpeed, DeltaSeconds);

		if (FMath::Abs(GetActorLocation().Z - Target.Z) <= ArrivalRadius)
		{
			SetDroneState(PatrolPoints.Num() > 0 ? EAeroDroneState::Patrolling : EAeroDroneState::Idle);
		}
		break;
	}

	case EAeroDroneState::Patrolling:
	{
		if (PatrolPoints.Num() == 0)
		{
			SetDroneState(EAeroDroneState::Idle);
			break;
		}

		const FVector Target = PatrolPoints[PatrolIndex];
		MoveTowards(Target, CruiseSpeed, DeltaSeconds);

		if (FVector::Dist(GetActorLocation(), Target) <= ArrivalRadius)
		{
			PatrolIndex = (PatrolIndex + 1) % PatrolPoints.Num();
		}
		break;
	}

	case EAeroDroneState::Investigating:
	{
		MoveTowards(InvestigateLocation, CruiseSpeed, DeltaSeconds);

		if (FVector::Dist(GetActorLocation(), InvestigateLocation) <= ArrivalRadius)
		{
			DwellRemaining -= DeltaSeconds;
			if (DwellRemaining <= 0.0f)
			{
				SetDroneState(PatrolPoints.Num() > 0 ? EAeroDroneState::Patrolling : EAeroDroneState::Idle);
			}
		}
		break;
	}

	case EAeroDroneState::Returning:
	{
		const FVector Target(HomeLocation.X, HomeLocation.Y, HomeLocation.Z + CruiseAltitude);
		MoveTowards(Target, CruiseSpeed, DeltaSeconds);

		if (FVector::Dist2D(GetActorLocation(), HomeLocation) <= ArrivalRadius)
		{
			SetDroneState(EAeroDroneState::Landing);
		}
		break;
	}

	case EAeroDroneState::Landing:
		MoveTowards(HomeLocation, VerticalSpeed, DeltaSeconds);
		if (FVector::Dist(GetActorLocation(), HomeLocation) <= ArrivalRadius * 0.2f)
		{
			SetActorLocation(HomeLocation);
			SetDroneState(EAeroDroneState::Charging);
		}
		break;

	case EAeroDroneState::Charging:
		// Docked needs both the minimum settle time and a full pack.
		if (StateElapsed >= ChargeSeconds && (!Battery || Battery->IsFull()))
		{
			SetDroneState(EAeroDroneState::Docked);
		}
		break;

	case EAeroDroneState::Idle:
	default:
		break;
	}

	CurrentVelocity = (GetActorLocation() - PreviousLocation) / FMath::Max(DeltaSeconds, KINDA_SMALL_NUMBER);
}

void AAeroDronePawn::BeginMission()
{
	if (DroneState == EAeroDroneState::Docked || DroneState == EAeroDroneState::Charging)
	{
		SetDroneState(EAeroDroneState::TakingOff);
	}
}

void AAeroDronePawn::RequestReturn()
{
	switch (DroneState)
	{
	case EAeroDroneState::Patrolling:
	case EAeroDroneState::Investigating:
	case EAeroDroneState::Idle:
	case EAeroDroneState::Ascending:
		SetDroneState(EAeroDroneState::Returning);
		break;
	default:
		break;
	}
}

void AAeroDronePawn::InvestigateAt(const FVector& WorldLocation, float DwellSeconds)
{
	if (DroneState == EAeroDroneState::Docked || DroneState == EAeroDroneState::Charging)
	{
		UE_LOG(LogAeroSentinel, Warning, TEXT("Drone %s cannot investigate while %s"),
			*GetName(), *UEnum::GetValueAsString(DroneState));
		return;
	}

	InvestigateLocation = WorldLocation;
	DwellRemaining = FMath::Max(0.0f, DwellSeconds);
	SetDroneState(EAeroDroneState::Investigating);
}

void AAeroDronePawn::SetPatrolPoints(const TArray<FVector>& InPoints)
{
	PatrolPoints = InPoints;
	PatrolIndex = 0;
}

void AAeroDronePawn::SetHomeLocation(const FVector& InHomeLocation)
{
	HomeLocation = InHomeLocation;

	if (PatrolPoints.Num() == 0)
	{
		BuildDefaultPatrolLoop();
	}
}

void AAeroDronePawn::HandleReserveReached()
{
	switch (DroneState)
	{
	case EAeroDroneState::Patrolling:
	case EAeroDroneState::Investigating:
	case EAeroDroneState::Idle:
	case EAeroDroneState::Ascending:
		UE_LOG(LogAeroSentinel, Log, TEXT("Drone %s breaking off: battery reserve reached"), *GetName());
		SetDroneState(EAeroDroneState::Returning);
		break;

	default:
		break;
	}
}

void AAeroDronePawn::ApplyCameraMount()
{
	if (CameraBoom)
	{
		// The chase camera rides higher than the gimbal mount. The vertical offset is applied to the
		// boom alone: CameraOffset is shared with SensorMount, and raising the sensor would move what
		// the airframe measures from.
		const FVector BoomOffset = CameraOffset
			+ FVector(0.0f, 0.0f, bThirdPersonView ? ThirdPersonVerticalOffset : 0.0f);

		CameraBoom->SetRelativeLocation(BoomOffset);
		CameraBoom->TargetArmLength = bThirdPersonView ? ThirdPersonArmLength : 0.0f;
	}

	if (ViewCamera)
	{
		// Third person is a viewing choice and gets its own framing. The gimbal view stays aligned with
		// the sensors, so the preview keeps framing exactly what is being captured.
		ViewCamera->SetRelativeRotation(bThirdPersonView
			? FRotator(ThirdPersonPitch, 0.0f, 0.0f) : CameraRotation);
		ViewCamera->SetFieldOfView(bThirdPersonView ? ThirdPersonFov : GimbalFov);
	}

	// Sensors sit on their own gimbal mount, so the boom's arm length is purely a viewing choice and
	// never moves what the airframe is measuring from.
	if (SensorMount)
	{
		SensorMount->SetRelativeLocation(CameraOffset);
		SensorMount->SetRelativeRotation(CameraRotation);
	}
}

void AAeroDronePawn::SetThirdPerson(bool bEnabled)
{
	bThirdPersonView = bEnabled;
	ApplyCameraMount();
}

float AAeroDronePawn::GetViewFieldOfView() const
{
	return bThirdPersonView ? ThirdPersonFov : GimbalFov;
}

void AAeroDronePawn::SetViewFieldOfView(float NewFov)
{
	// Clamped to a range that still renders: below ~20 the world reads as a telescope, above ~130 the
	// projection distortion makes the horizon unusable for judging where the airframe is.
	const float Clamped = FMath::Clamp(NewFov, 20.0f, 130.0f);

	if (bThirdPersonView)
	{
		ThirdPersonFov = Clamped;
	}
	else
	{
		GimbalFov = Clamped;
	}

	ApplyCameraMount();
}

void AAeroDronePawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// A pawn can be spawned before a local player exists, so the HUD is attempted here as well as in
	// BeginPlay. ShowHUD() is idempotent, so whichever runs second is a no-op.
	ShowHUD();
}

void AAeroDronePawn::ShowHUD()
{
	if (IsValid(HudWidget))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (PC == nullptr)
	{
		return;
	}

	// Only the airframe the player is actually flying owns the HUD. A level can contain more than one
	// drone (a docking station deploys its own), and without this check every one of them added a full
	// HUD to the same viewport, stacking two copies of every readout on top of each other.
	if (PC->GetPawn() != this)
	{
		return;
	}

	UClass* WidgetClass = HudWidgetClass.LoadSynchronous();
	if (WidgetClass == nullptr)
	{
		UE_LOG(LogAeroSentinel, Warning,
			TEXT("HUD class %s did not load; flying without a HUD."), *HudWidgetClass.ToString());
		return;
	}

	HudWidget = CreateWidget<UUserWidget>(PC, WidgetClass);
	if (HudWidget == nullptr)
	{
		UE_LOG(LogAeroSentinel, Warning, TEXT("HUD widget %s could not be created."),
			*WidgetClass->GetName());
		return;
	}

	HudWidget->AddToViewport();
	UE_LOG(LogAeroSentinel, Log, TEXT("HUD %s added to viewport."), *WidgetClass->GetName());
}

void AAeroDronePawn::SetExternalControlActive(bool bActive)
{
	if (bExternalControlActive == bActive)
	{
		return;
	}

	bExternalControlActive = bActive;
	CommandedBodyVelocity = FVector::ZeroVector;
	CommandedYawRateDegPerSec = 0.0f;
	CurrentVelocity = FVector::ZeroVector;
	StateElapsed = 0.0f;

	UE_LOG(LogAeroSentinel, Log, TEXT("Drone %s external control %s at %s"),
		*GetName(), bActive ? TEXT("engaged") : TEXT("released"), *GetActorLocation().ToCompactString());
}

void AAeroDronePawn::ApplyFlightCommand(const FVector& BodyVelocity, float YawRateDegPerSec)
{
	// A policy that starts commanding without an explicit handover is taken to mean it wants control.
	if (!bExternalControlActive)
	{
		SetExternalControlActive(true);
	}

	CommandedBodyVelocity = BodyVelocity.GetClampedToMaxSize(FMath::Max(MaxCommandedSpeed, 0.0f));
	CommandedYawRateDegPerSec = FMath::Clamp(YawRateDegPerSec, -TurnRate, TurnRate);
}

void AAeroDronePawn::ClearFlightCommand()
{
	CommandedBodyVelocity = FVector::ZeroVector;
	CommandedYawRateDegPerSec = 0.0f;
}

FVector AAeroDronePawn::GetActiveWaypoint() const
{
	if (bExternalControlActive)
	{
		return GetActorLocation();
	}

	switch (DroneState)
	{
	case EAeroDroneState::Patrolling:
		return PatrolPoints.IsValidIndex(PatrolIndex) ? PatrolPoints[PatrolIndex] : GetActorLocation();

	case EAeroDroneState::Investigating:
		return InvestigateLocation;

	case EAeroDroneState::Ascending:
	case EAeroDroneState::Returning:
		return FVector(HomeLocation.X, HomeLocation.Y, HomeLocation.Z + CruiseAltitude);

	case EAeroDroneState::Landing:
		return HomeLocation;

	default:
		return GetActorLocation();
	}
}

FAeroRLObservation AAeroDronePawn::GetObservation() const
{
	FAeroRLObservation Observation;

	const FVector Location = GetActorLocation();
	Observation.Location = Location;
	Observation.LocationRelativeToHome = Location - HomeLocation;
	Observation.Rotation = GetActorRotation();
	Observation.Velocity = CurrentVelocity;
	Observation.AltitudeAboveHomeCm = Location.Z - HomeLocation.Z;
	Observation.BatteryFraction = Battery ? Battery->GetStateOfChargeFraction() : 0.0f;
	Observation.DistanceToWaypointCm = FVector::Dist(Location, GetActiveWaypoint());
	Observation.bExternalControl = bExternalControlActive;
	Observation.DroneState = UEnum::GetValueAsString(DroneState);

	FString RegionName;
	AeroRecord::ResolveRegion(GetWorld(), Location, Observation.RegionId, RegionName);

	// Hazard context. This is deliberately measurement-side, not truth-side: the policy is given what a
	// sensor could read, so learning to find a hazard is a real inference task and not a lookup of the
	// ground-truth label the dataset is scored against.
	if (const UWorld* World = GetWorld())
	{
		if (const UAeroDisasterSubsystem* Disasters = World->GetSubsystem<UAeroDisasterSubsystem>())
		{
			FAeroDisasterInstance NearestInstance;
			float NearestDistanceCm = 0.0f;
			if (Disasters->FindNearestInstance(Location, NearestInstance, NearestDistanceCm))
			{
				Observation.NearestHazardType = NearestInstance.DisasterType;
				Observation.NearestHazardDistanceCm = NearestDistanceCm;
			}

			FAeroGroundTruth Truth;
			if (Disasters->GetGroundTruthAtLocation(Location, Truth))
			{
				Observation.HazardSeverityAtLocation = Truth.Severity;
			}

			FAeroEnvironmentalDelta Delta;
			Disasters->GetEnvironmentalDeltaAt(Location, Delta);
			Observation.LocalTemperatureDeltaC = Delta.TemperatureDeltaC;
			Observation.LocalPm25DeltaUgPerM3 = Delta.Pm25DeltaUgPerM3;
		}
	}

	return Observation;
}

TArray<float> AAeroDronePawn::GetObservationVector() const
{
	const FAeroRLObservation Observation = GetObservation();

	TArray<float> Vector;
	Vector.Reserve(14);

	Vector.Add(Observation.LocationRelativeToHome.X);
	Vector.Add(Observation.LocationRelativeToHome.Y);
	Vector.Add(Observation.LocationRelativeToHome.Z);
	Vector.Add(Observation.Velocity.X);
	Vector.Add(Observation.Velocity.Y);
	Vector.Add(Observation.Velocity.Z);
	Vector.Add(Observation.Rotation.Yaw);
	Vector.Add(Observation.AltitudeAboveHomeCm);
	Vector.Add(Observation.BatteryFraction);
	Vector.Add(Observation.DistanceToWaypointCm);
	Vector.Add(Observation.NearestHazardDistanceCm);
	Vector.Add(Observation.HazardSeverityAtLocation);
	Vector.Add(Observation.LocalTemperatureDeltaC);
	Vector.Add(Observation.LocalPm25DeltaUgPerM3);

	return Vector;
}

void AAeroDronePawn::ResetEpisode()
{
	bExternalControlActive = false;
	CommandedBodyVelocity = FVector::ZeroVector;
	CommandedYawRateDegPerSec = 0.0f;
	CurrentVelocity = FVector::ZeroVector;

	SetActorLocation(HomeLocation, false);
	SetActorRotation(FRotator::ZeroRotator);

	PatrolIndex = 0;
	DwellRemaining = 0.0f;
	StateElapsed = 0.0f;
	DroneState = EAeroDroneState::Docked;

	if (Battery)
	{
		Battery->SetStateOfChargeFraction(1.0f);
	}

	if (PatrolPoints.Num() == 0)
	{
		BuildDefaultPatrolLoop();
	}

	// Reward baselines belong to the episode that just ended. Carrying them across would book the
	// teleport home as a step the policy took.
	LastRewardDistanceCm = 0.0f;
	LastRewardBatteryWh = 0.0f;
	bRewardDistanceValid = false;
	bRewardBatteryValid = false;
	LastStepReward = 0.0f;

	UWorld* World = GetWorld();

	if (const UAeroSimClockSubsystem* Clock = World ? World->GetSubsystem<UAeroSimClockSubsystem>() : nullptr)
	{
		EpisodeStartSimSeconds = Clock->GetSimulationSeconds();
	}

	// A fresh scenario per episode stops a policy memorising one fixed hazard layout. The seed chain is
	// deterministic, so a run started from the same seed replays the same sequence of episodes.
	if (bResetScenarioOnEpisodeReset && World)
	{
		if (UAeroDisasterSubsystem* Disasters = World->GetSubsystem<UAeroDisasterSubsystem>())
		{
			Disasters->ResetScenario(Disasters->GetScenarioSeed() + 1);
		}
	}

	UE_LOG(LogAeroSentinel, Log, TEXT("Episode reset: %s docked at %s, episode clock starts at %.1f s"),
		*GetName(), *HomeLocation.ToCompactString(), EpisodeStartSimSeconds);
}

float AAeroDronePawn::ComputeStepReward()
{
	float Reward = 0.0f;

	const float CurrentDistanceCm = FVector::Dist(GetActorLocation(), GetActiveWaypoint());

	// Positive when the airframe closed on its waypoint during the step that just ended, negative when
	// it flew away. Nothing on the first call: there is no previous position to compare against.
	if (bRewardDistanceValid)
	{
		Reward += (LastRewardDistanceCm - CurrentDistanceCm) / 100.0f * RewardPerMeterClosed;
	}

	LastRewardDistanceCm = CurrentDistanceCm;
	bRewardDistanceValid = true;

	if (Battery)
	{
		const float RemainingWh = Battery->RemainingWh;

		// Only energy actually spent is charged for, so a recharge at the dock is not punished.
		if (bRewardBatteryValid)
		{
			Reward -= FMath::Max(0.0f, LastRewardBatteryWh - RemainingWh) * PenaltyPerWh;
		}

		LastRewardBatteryWh = RemainingWh;
		bRewardBatteryValid = true;
	}

	if (CurrentDistanceCm <= EpisodeGoalRadiusCm)
	{
		Reward += RewardGoalReached;
	}

	LastStepReward = Reward;
	return Reward;
}

bool AAeroDronePawn::IsEpisodeTerminated() const
{
	// A flat pack is a forced landing: the episode is over, not merely long.
	if (Battery && Battery->RemainingWh <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	// Below the floor while off the pad is a crash. Sitting exactly on the pad is not.
	return GetActorLocation().Z < HomeLocation.Z + MinAltitudeAboveHome;
}

bool AAeroDronePawn::IsEpisodeTruncated() const
{
	return MaxEpisodeSimSeconds > 0.0f && GetEpisodeSimSeconds() >= MaxEpisodeSimSeconds;
}

float AAeroDronePawn::GetEpisodeSimSeconds() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const UAeroSimClockSubsystem* Clock = World->GetSubsystem<UAeroSimClockSubsystem>())
		{
			return static_cast<float>(Clock->GetSimulationSeconds() - EpisodeStartSimSeconds);
		}
	}

	return 0.0f;
}

void AAeroDronePawn::SetDroneState(EAeroDroneState NewState)
{
	if (DroneState == NewState)
	{
		return;
	}

	const EAeroDroneState Previous = DroneState;
	DroneState = NewState;
	StateElapsed = 0.0f;

	UE_LOG(LogAeroSentinel, Log, TEXT("Drone %s: %s -> %s"),
		*GetName(),
		*UEnum::GetValueAsString(Previous),
		*UEnum::GetValueAsString(NewState));
}

void AAeroDronePawn::MoveTowards(const FVector& Target, float Speed, float DeltaSeconds)
{
	const FVector ToTarget = Target - GetActorLocation();
	const float Distance = ToTarget.Size();

	if (Distance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float Step = FMath::Min(Distance, Speed * DeltaSeconds);
	const FVector Direction = ToTarget / Distance;
	const FVector Start = GetActorLocation();
	const FVector DesiredEnd = Start + Direction * Step;

	bool bBlocked = false;
	if (UWorld* World = GetWorld())
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AeroDroneAvoidance), false, this);
		FHitResult Hit;
		bBlocked = World->SweepSingleByChannel(
			Hit,
			Start,
			DesiredEnd,
			FQuat::Identity,
			ECC_Visibility,
			FCollisionShape::MakeSphere(FMath::Max(1.0f, CollisionAvoidanceRadius)),
			QueryParams);
	}

	if (bBlocked)
	{
		const FVector LiftedEnd = Start + FVector(0.0f, 0.0f, FMath::Min(CollisionAvoidanceLift, Step));
		AddActorWorldOffset(LiftedEnd - Start, false);
	}
	else
	{
		AddActorWorldOffset(DesiredEnd - Start, false);
	}

	// Only steer on the horizontal plane: a pure climb or descent must not spin the airframe.
	if (FVector::DistSquared2D(GetActorLocation(), Target) > 1.0f)
	{
		FaceTowards(Target, DeltaSeconds);
	}
}

void AAeroDronePawn::FaceTowards(const FVector& Target, float DeltaSeconds)
{
	FVector Flat = Target - GetActorLocation();
	Flat.Z = 0.0f;

	if (Flat.SizeSquared() < 1.0f)
	{
		return;
	}

	const float DesiredYaw = Flat.Rotation().Yaw;
	const float NewYaw = FMath::FixedTurn(GetActorRotation().Yaw, DesiredYaw, TurnRate * DeltaSeconds);
	SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
}

void AAeroDronePawn::BuildDefaultPatrolLoop()
{
	PatrolPoints.Reset();

	const float Altitude = HomeLocation.Z + CruiseAltitude;
	PatrolPoints.Add(FVector(HomeLocation.X + PatrolRadius, HomeLocation.Y, Altitude));
	PatrolPoints.Add(FVector(HomeLocation.X + PatrolRadius, HomeLocation.Y + PatrolRadius, Altitude));
	PatrolPoints.Add(FVector(HomeLocation.X, HomeLocation.Y + PatrolRadius, Altitude));
	PatrolPoints.Add(FVector(HomeLocation.X, HomeLocation.Y, Altitude));

	PatrolIndex = 0;
}
