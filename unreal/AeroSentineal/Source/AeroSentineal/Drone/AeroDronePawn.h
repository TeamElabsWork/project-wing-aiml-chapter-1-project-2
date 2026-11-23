// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "UObject/SoftObjectPtr.h"
#include "Core/AeroTypes.h"
#include "AeroDronePawn.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class USpringArmComponent;
class UCameraComponent;
class UAeroBatteryComponent;
class UAeroRgbCameraComponent;
class UAeroThermalCameraComponent;
class UAeroWeatherSensorComponent;
class UAeroGasSensorComponent;
class UAeroWaterLevelSensorComponent;
class UAeroRainfallSensorComponent;
class UAeroSoilMoistureSensorComponent;
class UAeroTelemetryComponent;
class UUserWidget;

/**
 * The simulated airframe.
 *
 * Movement is controlled-kinematic: the pawn integrates its own position each tick and never runs
 * through the physics solver. That keeps a mission reproducible frame-for-frame, which is the whole
 * point of a dataset generator, and it leaves IAeroFlightControllerAdapter free to drive the same
 * pawn from PX4 or ROS 2 later without touching this class.
 *
 * The mission finite state machine is the one EAeroDroneState describes:
 *
 *   Docked -> TakingOff -> Ascending -> Patrolling <-> Investigating
 *                                            \            /
 *                                             Returning -> Landing -> Charging -> Docked
 *
 * Altitudes are relative to HomeLocation, which AAeroDockingStation sets to the pad position, so a
 * station can be dropped anywhere in the valley without retuning the flight profile.
 */
UCLASS(Blueprintable)
class AEROSENTINEAL_API AAeroDronePawn : public APawn
{
	GENERATED_BODY()

public:
	AAeroDronePawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ---------------------------------------------------------------- mission API

	/** Start the mission from the dock: spin-up, then climb to CruiseAltitude. */
	UFUNCTION(BlueprintCallable, Category = "Aero|Drone")
	void BeginMission();

	/** Break off whatever the drone is doing and fly home, then land and charge. */
	UFUNCTION(BlueprintCallable, Category = "Aero|Drone")
	void RequestReturn();

	/** Fly to a world location, hold there for DwellSeconds, then resume patrol. */
	UFUNCTION(BlueprintCallable, Category = "Aero|Drone")
	void InvestigateAt(const FVector& WorldLocation, float DwellSeconds = 6.0f);

	/** Replace the patrol loop. Empty list makes the drone hold station after take-off. */
	UFUNCTION(BlueprintCallable, Category = "Aero|Drone")
	void SetPatrolPoints(const TArray<FVector>& InPoints);

	/** Anchor for the altitude profile. The docking station calls this on spawn. */
	UFUNCTION(BlueprintCallable, Category = "Aero|Drone")
	void SetHomeLocation(const FVector& InHomeLocation);

	UFUNCTION(BlueprintPure, Category = "Aero|Drone")
	EAeroDroneState GetDroneState() const { return DroneState; }

	UFUNCTION(BlueprintPure, Category = "Aero|Drone")
	FVector GetHomeLocation() const { return HomeLocation; }

	// ---------------------------------------------------------------- tuning

	/** Horizontal cruise speed, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Flight")
	float CruiseSpeed = 1200.0f;

	/** Climb and descent rate, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Flight")
	float VerticalSpeed = 400.0f;

	/** Cruise altitude above HomeLocation, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Flight")
	float CruiseAltitude = 12000.0f;

	/** A waypoint counts as reached inside this radius, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Flight")
	float ArrivalRadius = 300.0f;

	/** Yaw slew rate, degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Flight")
	float TurnRate = 120.0f;

	/** Sweep radius used to avoid blocking world geometry during controlled flight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Flight|Safety")
	float CollisionAvoidanceRadius = 45.0f;

	/** Upward sidestep used when the direct flight segment is blocked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Flight|Safety")
	float CollisionAvoidanceLift = 150.0f;

	/** Rotor spin-up delay before the climb starts, seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Flight")
	float SpinUpSeconds = 2.0f;

	/** Minimum time settled on the pad before Docked, seconds. The charge itself is battery-driven. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Flight")
	float ChargeSeconds = 10.0f;

	/** Relaunch once the pack is full again, so an unattended session keeps cycling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Mission")
	bool bAutoRelaunchOnFullCharge = true;

	/** Dwell in Docked before the automatic relaunch, seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Mission")
	float DockedDwellSeconds = 5.0f;

	/** Radius of the generated patrol square used when no patrol points are supplied, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Patrol")
	float PatrolRadius = 30000.0f;

	/**
	 * Gimbal mount relative to the airframe root, cm. Sits under the nose: the rotor disc reaches
	 * roughly 43 cm out and 28 cm up, so a mount inside that envelope puts the airframe itself in
	 * every captured frame. Kept below the disc and forward of the body centre.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Camera")
	FVector CameraOffset = FVector(26.0f, 0.0f, 1.0f);

	/** Gimbal pitch, degrees. Negative looks down at the survey angle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Camera")
	FRotator CameraRotation = FRotator(-15.0f, 0.0f, 0.0f);

	/**
	 * Viewing distance in third person, cm.
	 *
	 * Set for chase framing: the airframe sits low in the frame with the valley visible above it,
	 * which needs the camera much closer than a survey orbit would.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Camera")
	float ThirdPersonArmLength = 120.0f;

	/** Horizontal field of view of the chase camera, degrees. Wide so the landscape reads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Camera")
	float ThirdPersonFov = 90.0f;

	/**
	 * Chase camera down-pitch, degrees. Negative looks down.
	 *
	 * Kept separate from CameraRotation, which is the gimbal angle: tilting the chase view must not
	 * tilt what the sensors are pointed at.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Camera")
	float ThirdPersonPitch = -6.0f;

	/** Adds to CameraOffset for the chase camera only, cm. SensorMount deliberately does not move. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Camera")
	float ThirdPersonVerticalOffset = 32.0f;

	/** Gimbal-view field of view, degrees. Matches the sensor so the preview frames the capture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Camera")
	float GimbalFov = 60.0f;

	/** Third person by default so the airframe is visible on launch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Camera")
	bool bThirdPersonView = true;

	/** Switch the viewing camera between third person and the gimbal view. Sensors are unaffected. */
	UFUNCTION(BlueprintCallable, Category = "Aero|Camera")
	void SetThirdPerson(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Aero|Camera")
	bool IsThirdPerson() const { return bThirdPersonView; }

	/** Field of view of whichever camera is currently the viewing camera, degrees. */
	UFUNCTION(BlueprintPure, Category = "Aero|Camera")
	float GetViewFieldOfView() const;

	/** Sets the field of view of whichever camera is currently the viewing camera, degrees. */
	UFUNCTION(BlueprintCallable, Category = "Aero|Camera")
	void SetViewFieldOfView(float NewFov);

	// ---------------------------------------------------------------- HUD

	/**
	 * HUD widget class, loaded on begin play. Held as a soft reference because the widget blueprint is
	 * authored on top of UAeroHUDWidget and therefore cannot exist while this module is compiling.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|UI")
	TSoftClassPtr<UUserWidget> HudWidgetClass;

	/** Creates the HUD once, when a widget class is set and a local player controller exists. */
	UFUNCTION(BlueprintCallable, Category = "Aero|UI")
	void ShowHUD();

	UFUNCTION(BlueprintPure, Category = "Aero|UI")
	UUserWidget* GetHudWidget() const { return HudWidget; }

	virtual void PossessedBy(AController* NewController) override;

	// ---------------------------------------------------------------- state

	UPROPERTY(BlueprintReadOnly, Category = "Aero|Drone")
	TArray<FVector> PatrolPoints;

	UPROPERTY(BlueprintReadOnly, Category = "Aero|Drone")
	EAeroDroneState DroneState = EAeroDroneState::Docked;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Drone")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Drone")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Drone")
	TObjectPtr<UCameraComponent> ViewCamera;

	/**
	 * Gimbal point the sensors attach to. Deliberately NOT the camera boom: the boom's arm length is a
	 * viewing preference, and a sensor that slid backwards whenever the camera pulled back would not be
	 * measuring the airframe's position any more.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Drone")
	TObjectPtr<USceneComponent> SensorMount;

	/** Energy model. Drives the return decision and the recharge at the dock. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Drone")
	TObjectPtr<UAeroBatteryComponent> Battery;

	UFUNCTION(BlueprintPure, Category = "Aero|Drone")
	UAeroBatteryComponent* GetBattery() const { return Battery; }

	/** Visible-light sensor. Samples on the simulation clock, not on frame time. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Drone")
	TObjectPtr<UAeroRgbCameraComponent> RgbCamera;

	UFUNCTION(BlueprintPure, Category = "Aero|Drone")
	UAeroRgbCameraComponent* GetRgbCamera() const { return RgbCamera; }

	/** Simulated thermal channel, same cadence as the RGB sensor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Drone")
	TObjectPtr<UAeroThermalCameraComponent> ThermalCamera;

	UFUNCTION(BlueprintPure, Category = "Aero|Drone")
	UAeroThermalCameraComponent* GetThermalCamera() const { return ThermalCamera; }

	/** Atmospheric channel: temperature, wind, precipitation, visibility. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Drone")
	TObjectPtr<UAeroWeatherSensorComponent> WeatherSensor;

	UFUNCTION(BlueprintPure, Category = "Aero|Drone")
	UAeroWeatherSensorComponent* GetWeatherSensor() const { return WeatherSensor; }

	/** Gas channel: CO2 and PM2.5 concentration. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Drone")
	TObjectPtr<UAeroGasSensorComponent> GasSensor;

	UFUNCTION(BlueprintPure, Category = "Aero|Drone")
	UAeroGasSensorComponent* GetGasSensor() const { return GasSensor; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Drone")
	TObjectPtr<UAeroWaterLevelSensorComponent> WaterLevelSensor;

	UFUNCTION(BlueprintPure, Category = "Aero|Drone")
	UAeroWaterLevelSensorComponent* GetWaterLevelSensor() const { return WaterLevelSensor; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Drone")
	TObjectPtr<UAeroRainfallSensorComponent> RainfallSensor;

	UFUNCTION(BlueprintPure, Category = "Aero|Drone")
	UAeroRainfallSensorComponent* GetRainfallSensor() const { return RainfallSensor; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Drone")
	TObjectPtr<UAeroSoilMoistureSensorComponent> SoilMoistureSensor;

	UFUNCTION(BlueprintPure, Category = "Aero|Drone")
	UAeroSoilMoistureSensorComponent* GetSoilMoistureSensor() const { return SoilMoistureSensor; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Drone")
	TObjectPtr<UAeroTelemetryComponent> Telemetry;

	UFUNCTION(BlueprintPure, Category = "Aero|Drone")
	UAeroTelemetryComponent* GetTelemetry() const { return Telemetry; }

	// ---------------------------------------------------------------- reinforcement learning

	/**
	 * Hand the airframe to an external agent (an RL policy, PX4, ROS 2). The mission FSM is suspended
	 * for as long as this is active, so no autonomous state can fight the policy for the position.
	 */
	UFUNCTION(BlueprintCallable, Category = "Aero|RL")
	void SetExternalControlActive(bool bActive);

	UFUNCTION(BlueprintPure, Category = "Aero|RL")
	bool IsExternalControlActive() const { return bExternalControlActive; }

	/**
	 * One action step. BodyVelocity is cm/s in the airframe's own frame (X forward, Y right, Z up) and
	 * is clamped to MaxCommandedSpeed; YawRateDegPerSec is clamped to TurnRate. Kinematic, so the same
	 * action sequence always produces the same trajectory.
	 */
	UFUNCTION(BlueprintCallable, Category = "Aero|RL")
	void ApplyFlightCommand(const FVector& BodyVelocity, float YawRateDegPerSec);

	/** Stop moving without leaving external control. */
	UFUNCTION(BlueprintCallable, Category = "Aero|RL")
	void ClearFlightCommand();

	/** Full observation of the airframe, region included. */
	UFUNCTION(BlueprintPure, Category = "Aero|RL")
	FAeroRLObservation GetObservation() const;

	/**
	 * The same observation as a fixed-layout float vector, which is what a training loop wants:
	 * [0..2] position relative to home (cm), [3..5] velocity (cm/s), [6] yaw (deg),
	 * [7] altitude above home (cm), [8] battery fraction, [9] distance to waypoint (cm),
	 * [10] distance to the nearest live hazard (cm), [11] hazard severity at the airframe,
	 * [12] local temperature delta from hazards (C), [13] local PM2.5 delta from hazards (ug/m3).
	 */
	UFUNCTION(BlueprintPure, Category = "Aero|RL")
	TArray<float> GetObservationVector() const;

	/**
	 * Start a new episode: back on the pad, full pack, mission reset, command cleared, external control
	 * released. Returns the airframe to exactly the state a fresh spawn has, so episode N and N+1 are
	 * comparable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Aero|RL")
	void ResetEpisode();

	/** Waypoint the mission is currently flying to. Invalid when idle or under external control. */
	UFUNCTION(BlueprintPure, Category = "Aero|RL")
	FVector GetActiveWaypoint() const;

	// ---------------------------------------------------------------- reward and episode

	/** Reward for closing one metre on the active waypoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|RL|Reward")
	float RewardPerMeterClosed = 0.01f;

	/** One-off reward for standing inside EpisodeGoalRadiusCm of the waypoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|RL|Reward")
	float RewardGoalReached = 1.0f;

	/** Radius that counts as having reached the waypoint, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|RL|Reward")
	float EpisodeGoalRadiusCm = 500.0f;

	/** Penalty per watt-hour the pack lost during the step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|RL|Reward")
	float PenaltyPerWh = 0.02f;

	/** Episode length ceiling in simulation seconds. Hitting it truncates the episode, it does not fail it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|RL")
	float MaxEpisodeSimSeconds = 900.0f;

	/** Start a new scenario, from the next seed, at every episode boundary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|RL")
	bool bResetScenarioOnEpisodeReset = true;

	/**
	 * Scalar reward for the control step that just ended. Call it exactly once per step: it advances its
	 * own distance and energy baselines, so calling it twice over one step would book the same movement
	 * twice.
	 *
	 * Closing distance on the active waypoint, minus the energy the step cost, plus a bonus for
	 * arriving. There is deliberately no term for proximity to a hazard: shaping a bonus for being near
	 * a fire would teach a policy to fly into one. Detecting hazards is what the observation and the
	 * labelled dataset are for.
	 */
	UFUNCTION(BlueprintCallable, Category = "Aero|RL")
	float ComputeStepReward();

	/** True when the episode ended from inside the environment: exhausted pack, or below the floor. */
	UFUNCTION(BlueprintPure, Category = "Aero|RL")
	bool IsEpisodeTerminated() const;

	/** True when the episode hit MaxEpisodeSimSeconds: a time-limit truncation, not a failure. */
	UFUNCTION(BlueprintPure, Category = "Aero|RL")
	bool IsEpisodeTruncated() const;

	/** Simulation seconds elapsed since the episode began. */
	UFUNCTION(BlueprintPure, Category = "Aero|RL")
	float GetEpisodeSimSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Aero|RL")
	float GetLastStepReward() const { return LastStepReward; }

	/** Last known velocity, cm/s. Zero when stationary. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|RL")
	FVector CurrentVelocity = FVector::ZeroVector;

	/** True while an external agent owns the airframe. */
	UPROPERTY(BlueprintReadOnly, Category = "Aero|RL")
	bool bExternalControlActive = false;

	/** Envelope clamps for external control, so a random policy cannot fly off the map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|RL")
	float MinAltitudeAboveHome = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|RL")
	float MaxAltitudeAboveHome = 40000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|RL")
	float MaxHorizontalRange = 80000.0f;

	/** Commanded speed ceiling, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|RL")
	float MaxCommandedSpeed = 3000.0f;

protected:
	void SetDroneState(EAeroDroneState NewState);

	/** The live HUD instance. Lifetimes with the viewport, so this is only ever read behind IsValid. */
	UPROPERTY()
	TObjectPtr<UUserWidget> HudWidget;

	/** Battery crossed the return reserve: break off and fly home. */
	UFUNCTION()
	void HandleReserveReached();

	/** Push CameraOffset/CameraRotation onto the boom and camera. */
	void ApplyCameraMount();

	/** Step towards Target at Speed, then face the direction of travel. */
	void MoveTowards(const FVector& Target, float Speed, float DeltaSeconds);

	void FaceTowards(const FVector& Target, float DeltaSeconds);

	/** Build a square patrol loop around HomeLocation at PatrolRadius. */
	void BuildDefaultPatrolLoop();

	FVector HomeLocation = FVector::ZeroVector;

	FVector InvestigateLocation = FVector::ZeroVector;

	int32 PatrolIndex = 0;

	float StateElapsed = 0.0f;

	float DwellRemaining = 0.0f;

	/** Commanded body-frame velocity, cm/s. */
	FVector CommandedBodyVelocity = FVector::ZeroVector;

	/** Commanded yaw rate, deg/s. */
	float CommandedYawRateDegPerSec = 0.0f;

	/** Distance to the active waypoint at the previous ComputeStepReward call, cm. */
	float LastRewardDistanceCm = 0.0f;

	/** Pack energy at the previous ComputeStepReward call, Wh. */
	float LastRewardBatteryWh = 0.0f;

	/** True once a baseline exists. Without one the first reward would charge for the whole episode. */
	bool bRewardDistanceValid = false;

	bool bRewardBatteryValid = false;

	float LastStepReward = 0.0f;

	/** Simulation instant the current episode began, seconds. */
	double EpisodeStartSimSeconds = 0.0;
};
