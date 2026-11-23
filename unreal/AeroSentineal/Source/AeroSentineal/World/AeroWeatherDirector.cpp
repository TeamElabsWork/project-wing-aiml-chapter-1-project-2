// Copyright AeroSentinel. All Rights Reserved.

#include "World/AeroWeatherDirector.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Core/AeroLog.h"
#include "Core/AeroSimClockSubsystem.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace
{
	FString WeatherPresetToString(EAeroWeatherPreset Preset)
	{
		if (const UEnum* Enum = StaticEnum<EAeroWeatherPreset>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Preset));
		}

		return FString::FromInt(static_cast<int32>(Preset));
	}
}

AAeroWeatherDirector::AAeroWeatherDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// The director only holds atmosphere actor references, but it still needs a root
	// component so that level instances have a transform and can be moved in the editor.
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	if (SceneRoot)
	{
		SceneRoot->SetMobility(EComponentMobility::Movable);
		SetRootComponent(SceneRoot);
	}

	BuildDefaultPresets();
	CurrentState = BlendToState = BlendFromState = FAeroWeatherState();
	if (FindPreset(InitialPreset, CurrentState))
	{
		BlendFromState = BlendToState = CurrentState;
		CurrentPreset = TargetPreset = InitialPreset;
	}
}

void AAeroWeatherDirector::BuildDefaultPresets()
{
	if (Presets.Num() > 0)
	{
		return;
	}

	auto Add = [this](EAeroWeatherPreset Preset, float Sun, float Sky, float FogDensity, float Falloff,
		FLinearColor FogColor, float Cloud, float Precip, float Wind, float Temp, float Visibility)
	{
		FAeroWeatherPresetEntry Entry;
		Entry.Preset = Preset;
		Entry.State.SunIntensity = Sun;
		Entry.State.SkyLightIntensity = Sky;
		Entry.State.FogDensity = FogDensity;
		Entry.State.FogHeightFalloff = Falloff;
		Entry.State.FogColor = FogColor;
		Entry.State.CloudCoverage = Cloud;
		Entry.State.PrecipitationMmPerHour = Precip;
		Entry.State.WindSpeedMps = Wind;
		Entry.State.AirTemperatureC = Temp;
		Entry.State.VisibilityMeters = Visibility;
		Presets.Add(Entry);
	};

	Add(EAeroWeatherPreset::Clear,          10.0f, 1.00f, 0.008f, 0.25f, FLinearColor(0.35f, 0.42f, 0.50f), 0.05f,  0.0f,  2.0f, 22.0f, 25000.0f);
	Add(EAeroWeatherPreset::PartlyCloudy,    7.0f, 1.00f, 0.010f, 0.25f, FLinearColor(0.40f, 0.46f, 0.53f), 0.35f,  0.0f,  4.0f, 20.0f, 22000.0f);
	Add(EAeroWeatherPreset::Overcast,        2.5f, 0.90f, 0.015f, 0.22f, FLinearColor(0.46f, 0.50f, 0.55f), 0.80f,  0.0f,  6.0f, 16.0f, 14000.0f);
	Add(EAeroWeatherPreset::Rain,            1.2f, 0.70f, 0.030f, 0.20f, FLinearColor(0.42f, 0.46f, 0.50f), 0.95f,  6.0f,  9.0f, 13.0f,  6000.0f);
	Add(EAeroWeatherPreset::Storm,           0.5f, 0.55f, 0.050f, 0.18f, FLinearColor(0.34f, 0.37f, 0.41f), 1.00f, 25.0f, 20.0f, 11.0f,  2200.0f);
	Add(EAeroWeatherPreset::Fog,             3.0f, 0.85f, 0.090f, 0.40f, FLinearColor(0.62f, 0.65f, 0.68f), 0.50f,  0.0f,  1.0f,  9.0f,   300.0f);
}

void AAeroWeatherDirector::BeginPlay()
{
	Super::BeginPlay();

	BindAtmosphereActors();
	SetWeatherPreset(InitialPreset, 0.0f);
	LastCycleSeconds = 0.0;
}

void AAeroWeatherDirector::BindAtmosphereActors()
{
	SunLight = nullptr;
	SkyLight = nullptr;
	HeightFog = nullptr;

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<ADirectionalLight> It(World); It && SunLight == nullptr; ++It)
	{
		SunLight = *It;
	}

	for (TActorIterator<ASkyLight> It(World); It && SkyLight == nullptr; ++It)
	{
		SkyLight = *It;
	}

	for (TActorIterator<AExponentialHeightFog> It(World); It && HeightFog == nullptr; ++It)
	{
		HeightFog = *It;
	}

	UE_LOG(LogAeroSentinel, Log, TEXT("Weather director bound: sun=%s sky=%s fog=%s"),
		SunLight != nullptr ? TEXT("yes") : TEXT("MISSING"),
		SkyLight != nullptr ? TEXT("yes") : TEXT("MISSING"),
		HeightFog != nullptr ? TEXT("yes") : TEXT("MISSING"));
}

bool AAeroWeatherDirector::FindPreset(EAeroWeatherPreset Preset, FAeroWeatherState& OutState) const
{
	for (const FAeroWeatherPresetEntry& Entry : Presets)
	{
		if (Entry.Preset == Preset)
		{
			OutState = Entry.State;
			return true;
		}
	}

	return false;
}

void AAeroWeatherDirector::SetWeatherPreset(EAeroWeatherPreset NewPreset, float BlendSeconds)
{
	// A preset is a whole atmosphere, so choosing one takes the pen back from the manual setters.
	bManualWeatherActive = false;

	FAeroWeatherState Target;
	if (!FindPreset(NewPreset, Target))
	{
		UE_LOG(LogAeroSentinel, Warning, TEXT("Weather preset %s has no entry in the director's table."),
			*WeatherPresetToString(NewPreset));
		return;
	}

	// Atmosphere references are normally resolved in BeginPlay, but the director also has to
	// drive the level from the editor (and from Blueprint/Python before play starts). Bind
	// lazily on first use so the preset never updates state without touching the lights.
	if (SunLight == nullptr || SkyLight == nullptr || HeightFog == nullptr)
	{
		BindAtmosphereActors();
	}

	const float Seconds = BlendSeconds < 0.0f ? DefaultBlendSeconds : BlendSeconds;

	BlendFromState = CurrentState;
	BlendToState = Target;
	TargetPreset = NewPreset;
	BlendElapsed = 0.0f;
	BlendDuration = Seconds;

	if (Seconds <= KINDA_SMALL_NUMBER)
	{
		CurrentState = Target;
		CurrentPreset = NewPreset;
		BlendDuration = 0.0f;
		ApplyCurrentState();
	}

	UE_LOG(LogAeroSentinel, Log, TEXT("Weather -> %s over %.1f s"),
		*WeatherPresetToString(NewPreset), Seconds);
}

FAeroWeatherState AAeroWeatherDirector::BlendStates(const FAeroWeatherState& A, const FAeroWeatherState& B, float Alpha)
{
	FAeroWeatherState Out;
	Out.SunIntensity = FMath::Lerp(A.SunIntensity, B.SunIntensity, Alpha);
	Out.SkyLightIntensity = FMath::Lerp(A.SkyLightIntensity, B.SkyLightIntensity, Alpha);
	Out.FogDensity = FMath::Lerp(A.FogDensity, B.FogDensity, Alpha);
	Out.FogHeightFalloff = FMath::Lerp(A.FogHeightFalloff, B.FogHeightFalloff, Alpha);
	Out.FogColor = FMath::Lerp(A.FogColor, B.FogColor, Alpha);
	Out.CloudCoverage = FMath::Lerp(A.CloudCoverage, B.CloudCoverage, Alpha);
	Out.PrecipitationMmPerHour = FMath::Lerp(A.PrecipitationMmPerHour, B.PrecipitationMmPerHour, Alpha);
	Out.WindSpeedMps = FMath::Lerp(A.WindSpeedMps, B.WindSpeedMps, Alpha);
	Out.AirTemperatureC = FMath::Lerp(A.AirTemperatureC, B.AirTemperatureC, Alpha);
	Out.VisibilityMeters = FMath::Lerp(A.VisibilityMeters, B.VisibilityMeters, Alpha);
	return Out;
}

void AAeroWeatherDirector::ApplyCurrentState()
{
	if (SunLight != nullptr)
	{
		if (ULightComponent* Light = SunLight->GetLightComponent())
		{
			Light->SetIntensity(CurrentState.SunIntensity);
		}
	}

	if (SkyLight != nullptr)
	{
		if (USkyLightComponent* Sky = SkyLight->GetLightComponent())
		{
			Sky->SetIntensity(CurrentState.SkyLightIntensity);
		}
	}

	if (HeightFog != nullptr)
	{
		if (UExponentialHeightFogComponent* Fog = HeightFog->GetComponent())
		{
			Fog->SetFogDensity(CurrentState.FogDensity);
			Fog->SetFogHeightFalloff(CurrentState.FogHeightFalloff);
			Fog->SetFogInscatteringColor(CurrentState.FogColor);
		}
	}
}

void AAeroWeatherDirector::AdvanceAutoCycle()
{
	if (!bAutoCycle || Presets.Num() < 2)
	{
		return;
	}

	double NowSeconds = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0;
	if (const UAeroSimClockSubsystem* Clock = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UAeroSimClockSubsystem>() : nullptr)
	{
		NowSeconds = Clock->GetSimulationSeconds();
	}

	if (NowSeconds - LastCycleSeconds < AutoCycleIntervalSeconds)
	{
		return;
	}

	LastCycleSeconds = NowSeconds;

	int32 Index = 0;
	for (int32 i = 0; i < Presets.Num(); ++i)
	{
		if (Presets[i].Preset == TargetPreset)
		{
			Index = (i + 1) % Presets.Num();
			break;
		}
	}

	SetWeatherPreset(Presets[Index].Preset, DefaultBlendSeconds);
}

void AAeroWeatherDirector::SetManualWeatherActive(bool bActive)
{
	if (bManualWeatherActive == bActive)
	{
		return;
	}

	bManualWeatherActive = bActive;

	if (bActive)
	{
		// Freeze the blend where it stands. Finishing the in-flight blend towards its target preset
		// would make taking manual control visibly change the sky, which is the opposite of an override.
		BlendFromState = CurrentState;
		BlendToState = CurrentState;
		BlendElapsed = 0.0f;
		BlendDuration = 0.0f;
		TargetPreset = CurrentPreset;
	}

	UE_LOG(LogAeroSentinel, Log, TEXT("Weather manual override %s"),
		bActive ? TEXT("engaged") : TEXT("released"));
}

void AAeroWeatherDirector::ApplyManualChange()
{
	if (SunLight == nullptr || SkyLight == nullptr || HeightFog == nullptr)
	{
		BindAtmosphereActors();
	}

	// Keep both blend endpoints on the live value, so leaving manual mode cannot resume a stale blend
	// from an atmosphere the user has since overwritten.
	BlendFromState = CurrentState;
	BlendToState = CurrentState;
	BlendElapsed = 0.0f;
	BlendDuration = 0.0f;

	ApplyCurrentState();
}

void AAeroWeatherDirector::SetWindSpeedMps(float NewValue)
{
	CurrentState.WindSpeedMps = FMath::Max(0.0f, NewValue);
	ApplyManualChange();
}

void AAeroWeatherDirector::SetAirTemperatureC(float NewValue)
{
	CurrentState.AirTemperatureC = NewValue;
	ApplyManualChange();
}

void AAeroWeatherDirector::SetPrecipitationMmPerHour(float NewValue)
{
	CurrentState.PrecipitationMmPerHour = FMath::Max(0.0f, NewValue);
	ApplyManualChange();
}

void AAeroWeatherDirector::SetVisibilityMeters(float NewValue)
{
	CurrentState.VisibilityMeters = FMath::Max(0.0f, NewValue);
	ApplyManualChange();
}

void AAeroWeatherDirector::SetCloudCoverage(float NewValue)
{
	CurrentState.CloudCoverage = FMath::Clamp(NewValue, 0.0f, 1.0f);
	ApplyManualChange();
}

void AAeroWeatherDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Manual mode owns the atmosphere: there is no blend to advance, and an auto-cycle would fight
	// the sliders for control of the same state.
	if (bManualWeatherActive)
	{
		return;
	}

	if (IsBlending())
	{
		BlendElapsed = FMath::Min(BlendElapsed + DeltaSeconds, BlendDuration);
		CurrentState = BlendStates(BlendFromState, BlendToState, BlendElapsed / BlendDuration);
		ApplyCurrentState();

		if (BlendElapsed >= BlendDuration)
		{
			CurrentState = BlendToState;
			CurrentPreset = TargetPreset;
		}
	}

	AdvanceAutoCycle();
}
