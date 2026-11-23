// Copyright AeroSentinel. All Rights Reserved.

#include "World/AeroPatrolSubsystem.h"

#include "Drone/AeroDronePawn.h"
#include "EngineUtils.h"

bool UAeroPatrolSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::PIE || WorldType == EWorldType::Game
		|| WorldType == EWorldType::Editor || WorldType == EWorldType::EditorPreview;
}

void UAeroPatrolSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!IsValid(Drone))
	{
		for (TActorIterator<AAeroDronePawn> It(&InWorld); It; ++It)
		{
			Drone = *It;
			break;
		}
	}
}

void UAeroPatrolSubsystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	CheckInvestigationCompletion();
	TryStartNextRequest();
}

TStatId UAeroPatrolSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAeroPatrolSubsystem, STATGROUP_Tickables);
}

bool UAeroPatrolSubsystem::SubmitInvestigationRequest(const FAeroAnomalyRequest& Request)
{
	if (!FMath::IsFinite(Request.WorldLocation.X)
		|| !FMath::IsFinite(Request.WorldLocation.Y)
		|| !FMath::IsFinite(Request.WorldLocation.Z))
	{
		return false;
	}

	FAeroAnomalyRequest Normalized = Request;
	if (Normalized.RequestId.IsEmpty())
	{
		Normalized.RequestId = FString::Printf(TEXT("ANOMALY-%06d"), PendingRequests.Num() + 1);
	}
	Normalized.DwellSeconds = FMath::Max(0.0f, Normalized.DwellSeconds);

	PendingRequests.Add(Normalized);
	PendingRequests.Sort([](const FAeroAnomalyRequest& A, const FAeroAnomalyRequest& B)
	{
		return A.Priority > B.Priority;
	});
	return true;
}

void UAeroPatrolSubsystem::ClearInvestigationRequests()
{
	PendingRequests.Reset();
}

void UAeroPatrolSubsystem::TryStartNextRequest()
{
	if (bInvestigationActive || PendingRequests.Num() == 0 || !IsValid(Drone))
	{
		return;
	}

	const EAeroDroneState State = Drone->GetDroneState();
	if (State != EAeroDroneState::Patrolling && State != EAeroDroneState::Idle)
	{
		return;
	}

	ActiveRequest = PendingRequests[0];
	PendingRequests.RemoveAt(0);
	bInvestigationActive = true;
	LastRequestId = ActiveRequest.RequestId;
	Drone->InvestigateAt(ActiveRequest.WorldLocation, ActiveRequest.DwellSeconds);
	OnInvestigationStarted.Broadcast(ActiveRequest);
}

void UAeroPatrolSubsystem::CheckInvestigationCompletion()
{
	if (!bInvestigationActive || !IsValid(Drone))
	{
		return;
	}

	const EAeroDroneState State = Drone->GetDroneState();
	if (State == EAeroDroneState::Patrolling || State == EAeroDroneState::Returning
		|| State == EAeroDroneState::Landing || State == EAeroDroneState::Charging
		|| State == EAeroDroneState::Docked)
	{
		bInvestigationActive = false;
		OnInvestigationCompleted.Broadcast(ActiveRequest);
		ActiveRequest = FAeroAnomalyRequest();
	}
}
