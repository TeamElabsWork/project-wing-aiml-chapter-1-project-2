// Copyright AeroSentinel. All Rights Reserved.

#include "UI/AeroHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Core/AeroLog.h"
#include "Core/AeroSimClockSubsystem.h"
#include "Core/AeroTypes.h"
#include "Disaster/AeroDisasterSubsystem.h"
#include "Drone/AeroBatteryComponent.h"
#include "Drone/AeroDronePawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Sensors/AeroGasSensorComponent.h"
#include "World/AeroWeatherDirector.h"

namespace
{
	/** Reflected display name for a drone state, so the HUD label cannot disagree with the enum. */
	FString StateName(EAeroDroneState State)
	{
		if (const UEnum* Enum = StaticEnum<EAeroDroneState>())
		{
			return Enum->GetDisplayNameTextByValue(static_cast<int64>(State)).ToString();
		}

		return FString::FromInt(static_cast<int32>(State));
	}

	FString PresetName(EAeroWeatherPreset Preset)
	{
		if (const UEnum* Enum = StaticEnum<EAeroWeatherPreset>())
		{
			return Enum->GetDisplayNameTextByValue(static_cast<int64>(Preset)).ToString();
		}

		return FString::FromInt(static_cast<int32>(Preset));
	}

	FString PhaseName(EAeroDisasterPhase Phase)
	{
		if (const UEnum* Enum = StaticEnum<EAeroDisasterPhase>())
		{
			return Enum->GetDisplayNameTextByValue(static_cast<int64>(Phase)).ToString();
		}

		return FString::FromInt(static_cast<int32>(Phase));
	}

	FText Num(float Value, int32 Decimals, const FString& Suffix)
	{
		return FText::FromString(FString::Printf(TEXT("%.*f%s"), Decimals, Value, *Suffix));
	}

	FText Label(const FString& Value)
	{
		return FText::FromString(Value);
	}

	/**
	 * A slider the user is currently touching owns its own value; pushing state back into it would
	 * fight the drag. Everything else follows the atmosphere.
	 */
	void SyncSlider(USlider* Slider, float Value)
	{
		if (Slider == nullptr)
		{
			return;
		}

		if (Slider->HasMouseCapture() || Slider->IsHovered())
		{
			return;
		}

		if (!FMath::IsNearlyEqual(Slider->GetValue(), Value, 0.01f))
		{
			Slider->SetValue(Value);
		}
	}
}

void UAeroHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ResolveReferences();

	// Slider ranges are set here rather than trusted from the tree: a slider whose range does not match
	// the physical quantity would silently clamp a command the user meant.
	if (Sld_Wind)
	{
		Sld_Wind->SetMinValue(0.0f);
		Sld_Wind->SetMaxValue(25.0f);
	}
	if (Sld_Temperature)
	{
		Sld_Temperature->SetMinValue(-20.0f);
		Sld_Temperature->SetMaxValue(45.0f);
	}
	if (Sld_Precipitation)
	{
		Sld_Precipitation->SetMinValue(0.0f);
		Sld_Precipitation->SetMaxValue(40.0f);
	}
	if (Sld_Visibility)
	{
		Sld_Visibility->SetMinValue(100.0f);
		Sld_Visibility->SetMaxValue(25000.0f);
	}
	if (Sld_CloudCover)
	{
		Sld_CloudCover->SetMinValue(0.0f);
		Sld_CloudCover->SetMaxValue(1.0f);
	}
	if (Sld_Fov)
	{
		Sld_Fov->SetMinValue(40.0f);
		Sld_Fov->SetMaxValue(130.0f);
	}

	if (Btn_Clear)        { Btn_Clear->OnClicked.AddDynamic(this, &UAeroHUDWidget::HandlePresetClear); }
	if (Btn_PartlyCloudy) { Btn_PartlyCloudy->OnClicked.AddDynamic(this, &UAeroHUDWidget::HandlePresetPartlyCloudy); }
	if (Btn_Overcast)     { Btn_Overcast->OnClicked.AddDynamic(this, &UAeroHUDWidget::HandlePresetOvercast); }
	if (Btn_Rain)         { Btn_Rain->OnClicked.AddDynamic(this, &UAeroHUDWidget::HandlePresetRain); }
	if (Btn_Storm)        { Btn_Storm->OnClicked.AddDynamic(this, &UAeroHUDWidget::HandlePresetStorm); }
	if (Btn_Fog)          { Btn_Fog->OnClicked.AddDynamic(this, &UAeroHUDWidget::HandlePresetFog); }
	if (Btn_Manual)       { Btn_Manual->OnClicked.AddDynamic(this, &UAeroHUDWidget::HandleManualClicked); }
	if (Btn_Camera)       { Btn_Camera->OnClicked.AddDynamic(this, &UAeroHUDWidget::HandleCameraClicked); }
	if (Btn_ResetEpisode) { Btn_ResetEpisode->OnClicked.AddDynamic(this, &UAeroHUDWidget::HandleResetEpisodeClicked); }

	if (Sld_Wind)          { Sld_Wind->OnValueChanged.AddDynamic(this, &UAeroHUDWidget::HandleWindChanged); }
	if (Sld_Temperature)   { Sld_Temperature->OnValueChanged.AddDynamic(this, &UAeroHUDWidget::HandleTemperatureChanged); }
	if (Sld_Precipitation) { Sld_Precipitation->OnValueChanged.AddDynamic(this, &UAeroHUDWidget::HandlePrecipitationChanged); }
	if (Sld_Visibility)    { Sld_Visibility->OnValueChanged.AddDynamic(this, &UAeroHUDWidget::HandleVisibilityChanged); }
	if (Sld_CloudCover)    { Sld_CloudCover->OnValueChanged.AddDynamic(this, &UAeroHUDWidget::HandleCloudCoverChanged); }
	if (Sld_Fov)           { Sld_Fov->OnValueChanged.AddDynamic(this, &UAeroHUDWidget::HandleFovChanged); }

	// The airframe flies itself, so nothing else needs the pointer. Showing it lets the weather
	// surface and the sliders be operated while the simulation keeps running.
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->bShowMouseCursor = true;

		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}

	RefreshControls();
	RefreshReadouts();
}

void UAeroHUDWidget::ResolveReferences()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	if (!CachedDrone.IsValid())
	{
		CachedDrone = Cast<AAeroDronePawn>(GetOwningPlayerPawn());
	}

	if (!CachedWeather.IsValid())
	{
		for (TActorIterator<AAeroWeatherDirector> It(World); It; ++It)
		{
			CachedWeather = *It;
			break;
		}
	}

	if (!CachedDisasters.IsValid())
	{
		CachedDisasters = World->GetSubsystem<UAeroDisasterSubsystem>();
	}

	if (!CachedClock.IsValid())
	{
		CachedClock = World->GetSubsystem<UAeroSimClockSubsystem>();
	}
}

void UAeroHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ResolveReferences();
	PollHotkeys();
	RefreshReadouts();
	RefreshControls();
}

void UAeroHUDWidget::PollHotkeys()
{
	APlayerController* PC = GetOwningPlayer();
	if (PC == nullptr)
	{
		return;
	}

	// Polled rather than routed through an InputAction: the HUD is a developer surface and must work
	// in any level without an InputMappingContext asset being present.
	if (PC->WasInputKeyJustPressed(EKeys::H))
	{
		// Both panels move together as one unit, and the direction comes from the remembered state
		// rather than from Panel_HUD's visibility: reading that back after collapsing it made the
		// restore path believe the UI was still hidden, so it put only the telemetry panel back.
		bHudPanelsHidden = !bHudPanelsHidden;

		const ESlateVisibility PanelVisibility = bHudPanelsHidden
			? ESlateVisibility::Collapsed
			: ESlateVisibility::SelfHitTestInvisible;

		if (Panel_HUD)
		{
			Panel_HUD->SetVisibility(PanelVisibility);
		}
		if (Panel_Weather)
		{
			Panel_Weather->SetVisibility(PanelVisibility);
		}
	}

	if (PC->WasInputKeyJustPressed(EKeys::T))
	{
		if (Panel_Weather)
		{
			Panel_Weather->SetVisibility(Panel_Weather->IsVisible()
				? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		}
	}

	if (PC->WasInputKeyJustPressed(EKeys::V))
	{
		if (AAeroDronePawn* Drone = CachedDrone.Get())
		{
			Drone->SetThirdPerson(!Drone->IsThirdPerson());
		}
	}

	if (PC->WasInputKeyJustPressed(EKeys::M))
	{
		HandleManualClicked();
	}

	if (PC->WasInputKeyJustPressed(EKeys::R))
	{
		HandleResetEpisodeClicked();
	}
}

void UAeroHUDWidget::RefreshReadouts()
{
	AAeroDronePawn* Drone = CachedDrone.Get();

	if (Txt_Mode)
	{
		if (Drone == nullptr)
		{
			Txt_Mode->SetText(Label(TEXT("NO AIRFRAME")));
		}
		else
		{
			Txt_Mode->SetText(Label(Drone->IsExternalControlActive() ? TEXT("RL AGENT") : TEXT("AUTONOMOUS")));
		}
	}

	if (Drone != nullptr)
	{
		const FVector Location = Drone->GetActorLocation();
		const FVector Home = Drone->GetHomeLocation();
		const FVector Velocity = Drone->CurrentVelocity;

		if (Txt_State)
		{
			Txt_State->SetText(Label(StateName(Drone->GetDroneState())));
		}

		if (UAeroBatteryComponent* Pack = Drone->GetBattery())
		{
			const float Fraction = Pack->GetStateOfChargeFraction();

			if (Bar_Battery)
			{
				Bar_Battery->SetPercent(FMath::Clamp(Fraction, 0.0f, 1.0f));
			}

			if (Txt_Battery)
			{
				Txt_Battery->SetText(Label(FString::Printf(TEXT("%.0f%%  %.1f / %.0f Wh"),
					Fraction * 100.0f, Pack->RemainingWh, Pack->CapacityWh)));
			}
		}

		if (Txt_Altitude)
		{
			Txt_Altitude->SetText(Num((Location.Z - Home.Z) / 100.0f, 1, TEXT(" m")));
		}

		if (Txt_Speed)
		{
			const float Horizontal = FVector(Velocity.X, Velocity.Y, 0.0f).Size();
			Txt_Speed->SetText(Num(Horizontal / 100.0f, 1, TEXT(" m/s")));
		}

		if (Txt_Heading)
		{
			float Yaw = Drone->GetActorRotation().Yaw;
			if (Yaw < 0.0f)
			{
				Yaw += 360.0f;
			}
			Txt_Heading->SetText(Num(Yaw, 0, TEXT(" deg")));
		}

		if (Txt_Position)
		{
			Txt_Position->SetText(Label(FString::Printf(TEXT("X %.0f  Y %.0f"), Location.X / 100.0f, Location.Y / 100.0f)));
		}

		if (Txt_Range)
		{
			const float RangeToHome = FVector::Dist2D(Location, Home);
			Txt_Range->SetText(Num(RangeToHome / 100.0f, 0, TEXT(" m from home")));
		}

		if (Txt_Episode)
		{
			Txt_Episode->SetText(Label(FString::Printf(TEXT("ep %.0f s   r %.3f"),
				Drone->GetEpisodeSimSeconds(), Drone->GetLastStepReward())));
		}
	}

	// ---------------------------------------------------------------- atmosphere

	AAeroWeatherDirector* Weather = CachedWeather.Get();
	if (Weather != nullptr)
	{
		const FAeroWeatherState State = Weather->GetWeatherState();

		if (Txt_WeatherPreset)
		{
			Txt_WeatherPreset->SetText(Label(Weather->IsManualWeatherActive()
				? TEXT("MANUAL") : PresetName(Weather->GetCurrentPreset()).ToUpper()));
		}

		if (Txt_Temp)
		{
			Txt_Temp->SetText(Num(State.AirTemperatureC, 1, TEXT(" C")));
		}

		if (Txt_Wind)
		{
			Txt_Wind->SetText(Num(State.WindSpeedMps, 1, TEXT(" m/s")));
		}

		if (Txt_Precip)
		{
			Txt_Precip->SetText(Num(State.PrecipitationMmPerHour, 1, TEXT(" mm/h")));
		}

		if (Txt_Visibility)
		{
			Txt_Visibility->SetText(Num(State.VisibilityMeters / 1000.0f, 1, TEXT(" km")));
		}

		if (Txt_Cloud)
		{
			Txt_Cloud->SetText(Num(State.CloudCoverage * 100.0f, 0, TEXT("% cloud")));
		}
	}

	// ---------------------------------------------------------------- gas and hazard

	if (UAeroDisasterSubsystem* Disasters = CachedDisasters.Get())
	{
		const FVector Probe = Drone != nullptr ? Drone->GetActorLocation() : FVector::ZeroVector;

		FAeroEnvironmentalDelta Delta;
		Disasters->GetEnvironmentalDeltaAt(Probe, Delta);

		if (Txt_Co2 || Txt_Pm25)
		{
			float AmbientCo2 = 420.0f;
			float AmbientPm25 = 8.0f;

			if (Drone != nullptr)
			{
				if (const UAeroGasSensorComponent* Gas = Drone->GetGasSensor())
				{
					AmbientCo2 = Gas->AmbientCo2Ppm;
					AmbientPm25 = Gas->AmbientPm25UgPerM3;
				}
			}

			// Same composition the gas record writes: modelled background plus the live hazard
			// contribution at this point, so the HUD and the dataset row agree.
			if (Txt_Co2)
			{
				Txt_Co2->SetText(Num(AmbientCo2 + Delta.Co2DeltaPpm, 0, TEXT(" ppm")));
			}

			if (Txt_Pm25)
			{
				Txt_Pm25->SetText(Num(AmbientPm25 + Delta.Pm25DeltaUgPerM3, 1, TEXT(" ug/m3")));
			}
		}

		FAeroDisasterInstance Nearest;
		float NearestDistanceCm = 1.0e9f;
		const bool bHasHazard = Disasters->FindNearestInstance(Probe, Nearest, NearestDistanceCm);

		FAeroGroundTruth Truth;
		const bool bInside = Disasters->GetGroundTruthAtLocation(Probe, Truth);

		if (Txt_Hazard)
		{
			Txt_Hazard->SetText(Label(bHasHazard
				? AeroDisasterNames::TypeToName(Nearest.DisasterType) : TEXT("NO ACTIVE HAZARD")));
		}

		if (Txt_HazardDetail)
		{
			if (bInside)
			{
				Txt_HazardDetail->SetText(Label(FString::Printf(TEXT("INSIDE  %s  sev %.2f"),
					*PhaseName(Truth.DisasterPhase), Truth.Severity)));
			}
			else if (bHasHazard)
			{
				Txt_HazardDetail->SetText(Label(FString::Printf(TEXT("%s  %.0f m"),
					*PhaseName(Nearest.Phase), NearestDistanceCm / 100.0f)));
			}
			else
			{
				Txt_HazardDetail->SetText(Label(TEXT("scenario idle")));
			}
		}

		if (Bar_Severity)
		{
			Bar_Severity->SetPercent(FMath::Clamp(bInside ? Truth.Severity : 0.0f, 0.0f, 1.0f));
		}
	}

	if (Txt_SimTime)
	{
		if (UAeroSimClockSubsystem* Clock = CachedClock.Get())
		{
			Txt_SimTime->SetText(Label(Clock->GetSimulationUtcIso8601()));
		}
	}
}

void UAeroHUDWidget::RefreshControls()
{
	AAeroWeatherDirector* Weather = CachedWeather.Get();
	if (Weather == nullptr)
	{
		return;
	}

	const FAeroWeatherState State = Weather->GetWeatherState();

	// Mirroring the atmosphere into the controls must not read as user input. USlider::SetValue fires
	// OnValueChanged, so without this the sync below engaged manual weather on the frame after a preset
	// button was pressed and froze the blend at whatever value it had reached.
	bSyncingControlWidgets = true;

	SyncSlider(Sld_Wind, State.WindSpeedMps);
	SyncSlider(Sld_Temperature, State.AirTemperatureC);
	SyncSlider(Sld_Precipitation, State.PrecipitationMmPerHour);
	SyncSlider(Sld_Visibility, State.VisibilityMeters);
	SyncSlider(Sld_CloudCover, State.CloudCoverage);

	if (Txt_WindVal)          { Txt_WindVal->SetText(Num(Sld_Wind ? Sld_Wind->GetValue() : State.WindSpeedMps, 1, TEXT(""))); }
	if (Txt_TemperatureVal)   { Txt_TemperatureVal->SetText(Num(Sld_Temperature ? Sld_Temperature->GetValue() : State.AirTemperatureC, 1, TEXT(""))); }
	if (Txt_PrecipitationVal) { Txt_PrecipitationVal->SetText(Num(Sld_Precipitation ? Sld_Precipitation->GetValue() : State.PrecipitationMmPerHour, 1, TEXT(""))); }
	if (Txt_VisibilityVal)    { Txt_VisibilityVal->SetText(Num(Sld_Visibility ? Sld_Visibility->GetValue() : State.VisibilityMeters, 0, TEXT(""))); }
	if (Txt_CloudCoverVal)    { Txt_CloudCoverVal->SetText(Num((Sld_CloudCover ? Sld_CloudCover->GetValue() : State.CloudCoverage) * 100.0f, 0, TEXT("%"))); }

	if (Sld_Fov)
	{
		if (const AAeroDronePawn* Drone = CachedDrone.Get())
		{
			SyncSlider(Sld_Fov, Drone->GetViewFieldOfView());
		}
	}

	if (Txt_FovVal && Sld_Fov)
	{
		Txt_FovVal->SetText(Num(Sld_Fov->GetValue(), 0, TEXT(" deg")));
	}

	if (Txt_Hint)
	{
		Txt_Hint->SetText(Label(TEXT("H  hide UI      V  camera      T  weather      M  manual      R  reset episode")));
	}

	bSyncingControlWidgets = false;
}

void UAeroHUDWidget::ApplyWeatherPreset(EAeroWeatherPreset Preset)
{
	if (AAeroWeatherDirector* Weather = CachedWeather.Get())
	{
		// A preset is a whole atmosphere, so this also releases the director's manual override.
		Weather->SetWeatherPreset(Preset, 2.0f);
	}
}

void UAeroHUDWidget::HandlePresetClear()        { ApplyWeatherPreset(EAeroWeatherPreset::Clear); }
void UAeroHUDWidget::HandlePresetPartlyCloudy() { ApplyWeatherPreset(EAeroWeatherPreset::PartlyCloudy); }
void UAeroHUDWidget::HandlePresetOvercast()     { ApplyWeatherPreset(EAeroWeatherPreset::Overcast); }
void UAeroHUDWidget::HandlePresetRain()         { ApplyWeatherPreset(EAeroWeatherPreset::Rain); }
void UAeroHUDWidget::HandlePresetStorm()        { ApplyWeatherPreset(EAeroWeatherPreset::Storm); }
void UAeroHUDWidget::HandlePresetFog()          { ApplyWeatherPreset(EAeroWeatherPreset::Fog); }

void UAeroHUDWidget::HandleManualClicked()
{
	AAeroWeatherDirector* Weather = CachedWeather.Get();
	if (Weather == nullptr)
	{
		return;
	}

	Weather->SetManualWeatherActive(!Weather->IsManualWeatherActive());
}

void UAeroHUDWidget::HandleCameraClicked()
{
	if (AAeroDronePawn* Drone = CachedDrone.Get())
	{
		Drone->SetThirdPerson(!Drone->IsThirdPerson());
	}
}

void UAeroHUDWidget::HandleResetEpisodeClicked()
{
	if (AAeroDronePawn* Drone = CachedDrone.Get())
	{
		Drone->ResetEpisode();
	}
}

// Every slider both engages manual weather and writes its parameter through. Toggling first means a
// user who grabs a slider without pressing the manual button still gets the value they asked for.
void UAeroHUDWidget::HandleWindChanged(float Value)
{
	// A slider the HUD itself just wrote is not a user command; see bSyncingControlWidgets.
	if (bSyncingControlWidgets)
	{
		return;
	}

	if (AAeroWeatherDirector* Weather = CachedWeather.Get())
	{
		Weather->SetManualWeatherActive(true);
		Weather->SetWindSpeedMps(Value);
	}
}

void UAeroHUDWidget::HandleTemperatureChanged(float Value)
{
	if (bSyncingControlWidgets)
	{
		return;
	}

	if (AAeroWeatherDirector* Weather = CachedWeather.Get())
	{
		Weather->SetManualWeatherActive(true);
		Weather->SetAirTemperatureC(Value);
	}
}

void UAeroHUDWidget::HandlePrecipitationChanged(float Value)
{
	if (bSyncingControlWidgets)
	{
		return;
	}

	if (AAeroWeatherDirector* Weather = CachedWeather.Get())
	{
		Weather->SetManualWeatherActive(true);
		Weather->SetPrecipitationMmPerHour(Value);
	}
}

void UAeroHUDWidget::HandleVisibilityChanged(float Value)
{
	if (bSyncingControlWidgets)
	{
		return;
	}

	if (AAeroWeatherDirector* Weather = CachedWeather.Get())
	{
		Weather->SetManualWeatherActive(true);
		Weather->SetVisibilityMeters(Value);
	}
}

void UAeroHUDWidget::HandleCloudCoverChanged(float Value)
{
	if (bSyncingControlWidgets)
	{
		return;
	}

	if (AAeroWeatherDirector* Weather = CachedWeather.Get())
	{
		Weather->SetManualWeatherActive(true);
		Weather->SetCloudCoverage(Value);
	}
}

void UAeroHUDWidget::HandleFovChanged(float Value)
{
	if (bSyncingControlWidgets)
	{
		return;
	}

	if (AAeroDronePawn* Drone = CachedDrone.Get())
	{
		Drone->SetViewFieldOfView(Value);
	}
}
