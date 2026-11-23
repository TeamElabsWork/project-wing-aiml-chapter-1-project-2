// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/AeroTypes.h"
#include "Blueprint/UserWidget.h"
#include "AeroHUDWidget.generated.h"

class AAeroDronePawn;
class AAeroWeatherDirector;
class UAeroDisasterSubsystem;
class UAeroSimClockSubsystem;
class UTextBlock;
class UProgressBar;
class USlider;
class UButton;
class UWidget;

/**
 * Developer HUD for the simulation: live airframe telemetry, the atmospheric channels, the current
 * hazard, and the weather control surface.
 *
 * Every readout is driven natively from C++ rather than through Blueprint property bindings. The
 * widget tree therefore only supplies names and styling, and the numbers on screen cannot drift away
 * from the systems that own them.
 *
 * The values shown are the same ground truth the dataset records carry - the weather director for the
 * atmosphere, the disaster subsystem for hazard state - so a reading on the HUD and the row written to
 * disk at the same simulation instant agree by construction.
 *
 * Weather control is deliberately two-mode: choosing a preset hands the atmosphere back to the
 * director's blend table, while the per-parameter sliders engage the director's manual override. Both
 * paths mutate the single FAeroWeatherState that the sensors read, so an ML training run can vary the
 * environment from the HUD and have that variation appear in the observation and in the records.
 */
UCLASS()
class AEROSENTINEAL_API UAeroHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	// ---------------------------------------------------------------- layout contract
	//
	// BindWidgetOptional throughout: a restyle that drops a label should degrade that one readout
	// rather than fail the widget's compile, and the C++ side already null-checks every member.

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Panel_HUD;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Panel_Weather;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Mode;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_State;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> Bar_Battery;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Battery;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Altitude;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Speed;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Heading;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Position;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Range;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Episode;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_WeatherPreset;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Temp;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Wind;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Precip;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Visibility;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Cloud;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Co2;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Pm25;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Hazard;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_HazardDetail;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> Bar_Severity;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_SimTime;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Hint;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Clear;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_PartlyCloudy;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Overcast;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Rain;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Storm;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Fog;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Manual;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USlider> Sld_Wind;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USlider> Sld_Temperature;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USlider> Sld_Precipitation;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USlider> Sld_Visibility;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USlider> Sld_CloudCover;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USlider> Sld_Fov;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_WindVal;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_TemperatureVal;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_PrecipitationVal;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_VisibilityVal;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_CloudCoverVal;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_FovVal;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Camera;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_ResetEpisode;

	// ---------------------------------------------------------------- button handlers

	UFUNCTION() void HandlePresetClear();
	UFUNCTION() void HandlePresetPartlyCloudy();
	UFUNCTION() void HandlePresetOvercast();
	UFUNCTION() void HandlePresetRain();
	UFUNCTION() void HandlePresetStorm();
	UFUNCTION() void HandlePresetFog();
	UFUNCTION() void HandleManualClicked();
	UFUNCTION() void HandleCameraClicked();
	UFUNCTION() void HandleResetEpisodeClicked();

	// ---------------------------------------------------------------- slider handlers

	UFUNCTION() void HandleWindChanged(float Value);
	UFUNCTION() void HandleTemperatureChanged(float Value);
	UFUNCTION() void HandlePrecipitationChanged(float Value);
	UFUNCTION() void HandleVisibilityChanged(float Value);
	UFUNCTION() void HandleCloudCoverChanged(float Value);
	UFUNCTION() void HandleFovChanged(float Value);

private:
	/** Re-resolve the systems this HUD reads. Cheap, and tolerant of a travelling possession. */
	void ResolveReferences();

	/** Pull fresh numbers into the readout labels. */
	void RefreshReadouts();

	/** Push live weather state into the sliders and their value labels. */
	void RefreshControls();

	void ApplyWeatherPreset(EAeroWeatherPreset Preset);

	/** Cycles the HUD's own key bindings through the player controller without needing input assets. */
	void PollHotkeys();

	/**
	 * True while H is holding both panels down. H hides and restores the telemetry and weather panels as
	 * one unit, so the direction has to be remembered here: deriving it back from Panel_HUD's own
	 * visibility restored only that panel and left the weather surface collapsed.
	 */
	bool bHudPanelsHidden = false;

	/**
	 * True while RefreshControls pushes live weather state into the sliders. USlider::SetValue broadcasts
	 * OnValueChanged, so without this guard the HUD's own sync is indistinguishable from a user drag: it
	 * engaged manual weather and froze the preset blend the button had just started.
	 */
	bool bSyncingControlWidgets = false;

	TWeakObjectPtr<AAeroDronePawn> CachedDrone;
	TWeakObjectPtr<AAeroWeatherDirector> CachedWeather;
	TWeakObjectPtr<UAeroDisasterSubsystem> CachedDisasters;
	TWeakObjectPtr<UAeroSimClockSubsystem> CachedClock;
};
