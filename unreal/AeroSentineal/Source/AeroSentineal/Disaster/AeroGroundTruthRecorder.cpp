// Copyright AeroSentinel. All Rights Reserved.

#include "Disaster/AeroGroundTruthRecorder.h"

#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Core/AeroLog.h"
#include "Core/AeroSimClockSubsystem.h"
#include "Core/AeroSimSettings.h"
#include "Core/AeroTypes.h"
#include "Disaster/AeroDisasterSubsystem.h"

AAeroGroundTruthRecorder::AAeroGroundTruthRecorder()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
}

void AAeroGroundTruthRecorder::BeginPlay()
{
	Super::BeginPlay();

	const UAeroSimSettings* Settings = GetDefault<UAeroSimSettings>();
	const FString Root = Settings ? Settings->GetAbsoluteDatasetRoot() : FPaths::ProjectDir();
	const FString DroneId = Settings ? Settings->DroneId : TEXT("DRN-001");

	TimelinePath = FPaths::Combine(Root, TEXT("GroundTruth"), DroneId, TEXT("disaster_timeline.jsonl"));

	// A new run owns a new timeline. Appending to a previous run's file would mix two scenarios' labels
	// into one training set with no way to separate them afterwards.
	IFileManager::Get().Delete(*TimelinePath, false, true, true);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(TimelinePath), true);

	if (UWorld* World = GetWorld())
	{
		if (UAeroSimClockSubsystem* Clock = World->GetSubsystem<UAeroSimClockSubsystem>())
		{
			Clock->OnTimeAdvanced.AddDynamic(this, &AAeroGroundTruthRecorder::HandleSimTimeAdvanced);
		}
		else
		{
			UE_LOG(LogAeroSentinel, Warning,
				TEXT("Ground truth recorder %s found no simulation clock; the timeline will stay empty"), *GetName());
		}
	}

	UE_LOG(LogAeroSentinel, Log, TEXT("Ground truth recorder writing to %s"), *TimelinePath);

	// Emit the initial state straight away so the file never starts with a missing first sample.
	WriteTimelineSample();
}

void AAeroGroundTruthRecorder::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UAeroSimClockSubsystem* Clock = World->GetSubsystem<UAeroSimClockSubsystem>())
		{
			Clock->OnTimeAdvanced.RemoveDynamic(this, &AAeroGroundTruthRecorder::HandleSimTimeAdvanced);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AAeroGroundTruthRecorder::HandleSimTimeAdvanced(double SimulationSeconds, float DeltaSimSeconds)
{
	PendingSimSeconds += DeltaSimSeconds;

	// Unrolled so a 60x clock still emits one sample per interval instead of one per rendered frame.
	while (PendingSimSeconds >= FMath::Max(SampleIntervalSimSeconds, KINDA_SMALL_NUMBER))
	{
		PendingSimSeconds -= FMath::Max(SampleIntervalSimSeconds, KINDA_SMALL_NUMBER);
		WriteTimelineSample();
	}
}

void AAeroGroundTruthRecorder::WriteTimelineSample()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const UAeroDisasterSubsystem* Disasters = World->GetSubsystem<UAeroDisasterSubsystem>();
	if (!Disasters)
	{
		return;
	}

	TArray<FAeroDisasterInstance> Instances;
	Disasters->GetInstances(Instances);

	if (Instances.Num() == 0)
	{
		// No scenario running: nothing to label, and an empty block would just pad the file.
		return;
	}

	const UAeroSimClockSubsystem* Clock = World->GetSubsystem<UAeroSimClockSubsystem>();
	const double Now = Clock ? Clock->GetSimulationSeconds() : 0.0;
	const FString UtcIso = Clock ? Clock->GetSimulationUtcIso8601() : FString();

	FString Block;

	for (const FAeroDisasterInstance& Instance : Instances)
	{
		TSharedRef<FJsonObject> Line = MakeShared<FJsonObject>();
		Line->SetStringField(TEXT("schema"), TEXT("aero.disaster_timeline/1"));
		Line->SetNumberField(TEXT("sim_seconds"), Now);
		Line->SetStringField(TEXT("utc_iso"), UtcIso);
		// The seed lets a run that contains several episodes be split back into them: instance ids are
		// unique per spawn, but the seed is what says which scenario an instance belonged to.
		Line->SetNumberField(TEXT("scenario_seed"), Disasters->GetScenarioSeed());
		Line->SetStringField(TEXT("instance_id"), Instance.InstanceId);
		Line->SetStringField(TEXT("disaster_type"), AeroDisasterNames::TypeToName(Instance.DisasterType));
		Line->SetStringField(TEXT("disaster_phase"), AeroDisasterNames::PhaseToName(Instance.Phase));
		Line->SetNumberField(TEXT("severity"), Instance.Severity);
		Line->SetNumberField(TEXT("affected_radius_cm"), Instance.AffectedRadiusCm);
		Line->SetNumberField(TEXT("water_depth_cm"), Instance.WaterDepthCm);
		Line->SetNumberField(TEXT("fire_intensity"), Instance.FireIntensity);
		Line->SetNumberField(TEXT("elapsed_sim_seconds"), Instance.ElapsedSimSeconds);

		TSharedRef<FJsonObject> OriginObject = MakeShared<FJsonObject>();
		OriginObject->SetNumberField(TEXT("x"), Instance.Origin.X);
		OriginObject->SetNumberField(TEXT("y"), Instance.Origin.Y);
		OriginObject->SetNumberField(TEXT("z"), Instance.Origin.Z);
		Line->SetObjectField(TEXT("origin_cm"), OriginObject);

		FString Serialized;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Serialized);
		FJsonSerializer::Serialize(Line, Writer);
		Writer->Close();

		Block += Serialized;
		Block += LINE_TERMINATOR;
	}

	if (Block.IsEmpty())
	{
		return;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(TimelinePath), true);

	if (!FFileHelper::SaveStringToFile(Block, *TimelinePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), EFileWrite::FILEWRITE_Append))
	{
		UE_LOG(LogAeroSentinel, Error, TEXT("Ground truth recorder failed to append to %s"), *TimelinePath);
		return;
	}

	WrittenLineCount += Instances.Num();
}
