// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/AeroTypes.h"
#include "AeroWeatherDirector.generated.h"

class ADirectionalLight;
class ASkyLight;
class AExponentialHeightFog;

/** One named weather preset and the atmosphere it produces. */
USTRUCT(BlueprintType)
struct FAeroWeatherPresetEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	EAeroWeatherPreset Preset = EAeroWeatherPreset::Clear;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	FAeroWeatherState State;
};

/**
 * Drives the level's atmosphere from a small set of editable weather presets and blends between
 * them over sim time, so weather is a gradual environmental process rather than a hard switch.
 *
 * The director owns no data of its own beyond the current state: disasters (Phase 4) call
 * SetWeatherPreset, and sensors (Phase 5) read GetWeatherState as ground truth. That keeps weather
 * between the two without either depending on the other.
 */
UCLASS(Blueprintable)
class AEROSENTINEAL_API AAeroWeatherDirector : public AActor
{
	GENERATED_BODY()

public:
	AAeroWeatherDirector();

	/** Editable preset table. Defaults are built in the constructor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	TArray<FAeroWeatherPresetEntry> Presets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	EAeroWeatherPreset InitialPreset = EAeroWeatherPreset::Clear;

	/** Blend length used when a caller does not specify one. 0 snaps immediately. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	float DefaultBlendSeconds = 20.0f;

	/** Cycle through the presets on a timer. Useful for unattended demos. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	bool bAutoCycle = false;

	/** Sim seconds between automatic preset changes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	float AutoCycleIntervalSeconds = 1800.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	TObjectPtr<ADirectionalLight> SunLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	TObjectPtr<ASkyLight> SkyLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aero|Weather")
	TObjectPtr<AExponentialHeightFog> HeightFog;

	/** Blend to a preset over BlendSeconds (negative = DefaultBlendSeconds). */
	UFUNCTION(BlueprintCallable, Category = "Aero|Weather")
	void SetWeatherPreset(EAeroWeatherPreset NewPreset, float BlendSeconds = -1.0f);

	/**
	 * Hand the atmosphere to the individual setters below instead of to a preset.
	 *
	 * Entering manual mode freezes the blend where it currently stands, so switching over mid-storm
	 * keeps the sky already on screen instead of snapping to the last target preset. Choosing a
	 * preset releases manual mode again.
	 */
	UFUNCTION(BlueprintCallable, Category = "Aero|Weather")
	void SetManualWeatherActive(bool bActive);

	UFUNCTION(BlueprintPure, Category = "Aero|Weather")
	bool IsManualWeatherActive() const { return bManualWeatherActive; }

	// Individual parameter control. Each writes through to the bound lights and fog immediately.

	UFUNCTION(BlueprintCallable, Category = "Aero|Weather")
	void SetWindSpeedMps(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "Aero|Weather")
	void SetAirTemperatureC(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "Aero|Weather")
	void SetPrecipitationMmPerHour(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "Aero|Weather")
	void SetVisibilityMeters(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "Aero|Weather")
	void SetCloudCoverage(float NewValue);

	/** Push CurrentState onto the bound lights and fog. Called automatically while blending. */
	UFUNCTION(BlueprintCallable, Category = "Aero|Weather")
	void ApplyCurrentState();

	/** Find the level's sun, sky light and height fog. Called on BeginPlay; callable again if they change. */
	UFUNCTION(BlueprintCallable, Category = "Aero|Weather")
	void BindAtmosphereActors();

	UFUNCTION(BlueprintPure, Category = "Aero|Weather")
	FAeroWeatherState GetWeatherState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "Aero|Weather")
	EAeroWeatherPreset GetCurrentPreset() const { return CurrentPreset; }

	UFUNCTION(BlueprintPure, Category = "Aero|Weather")
	EAeroWeatherPreset GetTargetPreset() const { return TargetPreset; }

	/** True while a preset blend is in progress. */
	UFUNCTION(BlueprintPure, Category = "Aero|Weather")
	bool IsBlending() const { return BlendDuration > KINDA_SMALL_NUMBER && BlendElapsed < BlendDuration; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	/** Bind the atmosphere actors if needed, then push the state out with the blend frozen. */
	void ApplyManualChange();

	/** True while the individual setters own the atmosphere and preset blending is suspended. */
	bool bManualWeatherActive = false;

	void BuildDefaultPresets();
	bool FindPreset(EAeroWeatherPreset Preset, FAeroWeatherState& OutState) const;
	static FAeroWeatherState BlendStates(const FAeroWeatherState& A, const FAeroWeatherState& B, float Alpha);
	void AdvanceAutoCycle();

	FAeroWeatherState CurrentState;
	FAeroWeatherState BlendFromState;
	FAeroWeatherState BlendToState;

	EAeroWeatherPreset CurrentPreset = EAeroWeatherPreset::Clear;
	EAeroWeatherPreset TargetPreset = EAeroWeatherPreset::Clear;

	float BlendElapsed = 0.0f;
	float BlendDuration = 0.0f;
	double LastCycleSeconds = 0.0;
};
