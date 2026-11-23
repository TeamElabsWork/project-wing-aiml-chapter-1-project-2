// Copyright AeroSentinel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/AeroTypes.h"
#include "AeroPatrolSubsystem.generated.h"

class AAeroDronePawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAeroAnomalyRequestSignature, const FAeroAnomalyRequest&, Request);

/**
 * Coordinates patrol and investigation requests without knowing about individual disaster types or
 * external ML implementations.
 */
UCLASS()
class AEROSENTINEAL_API UAeroPatrolSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintCallable, Category = "Aero|Patrol")
	bool SubmitInvestigationRequest(const FAeroAnomalyRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "Aero|Patrol")
	void ClearInvestigationRequests();

	UFUNCTION(BlueprintPure, Category = "Aero|Patrol")
	int32 GetPendingRequestCount() const { return PendingRequests.Num(); }

	UFUNCTION(BlueprintPure, Category = "Aero|Patrol")
	bool HasActiveInvestigation() const { return bInvestigationActive; }

	UFUNCTION(BlueprintPure, Category = "Aero|Patrol")
	FAeroAnomalyRequest GetActiveRequest() const { return ActiveRequest; }

	UPROPERTY(BlueprintAssignable, Category = "Aero|Patrol")
	FAeroAnomalyRequestSignature OnInvestigationStarted;

	UPROPERTY(BlueprintAssignable, Category = "Aero|Patrol")
	FAeroAnomalyRequestSignature OnInvestigationCompleted;

	/** Optional level override; otherwise the first AAeroDronePawn is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aero|Patrol")
	TObjectPtr<AAeroDronePawn> Drone;

protected:
	void TryStartNextRequest();
	void CheckInvestigationCompletion();

	UPROPERTY()
	TArray<FAeroAnomalyRequest> PendingRequests;

	UPROPERTY()
	FAeroAnomalyRequest ActiveRequest;

	bool bInvestigationActive = false;
	FString LastRequestId;
};
