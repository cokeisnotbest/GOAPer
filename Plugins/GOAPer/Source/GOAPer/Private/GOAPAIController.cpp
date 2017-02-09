// Fill out your copyright notice in the Description page of Project Settings.

#include "GOAPer.h"
#include "GOAPAIController.h"
#include "EngineUtils.h"
#include "Engine.h"
#include "IdleState.h"
#include "MoveToState.h"
#include "DoActionState.h"
#include "GOAPPlanner.h"


bool AGOAPAIController::LoadGOAPDefaults()
{
	CurrentGoal = DefaultGoal;

	for (TSubclassOf<UGOAPAction> elem : AvailableActions)
	{
		GOAPActions.Add(NewObject<UGOAPAction>(this, elem));
	}

	GOAPState = GOAPState + StartingState;

	return true;
}

void AGOAPAIController::BeginPlay()
{
	Super::BeginPlay();

	Planner = NewObject<UGOAPPlanner>(this);

	LoadGOAPDefaults();

	SetNewState(MakeShareable(new IdleState()));
}

void AGOAPAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Run the FSM
	TSharedPtr<GOAPFSMState> _possibleNewState = state->Tick(*this, DeltaTime);
	if (_possibleNewState.IsValid())
	{
		SetNewState(_possibleNewState);
	}
}

void AGOAPAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);
	
	SetNewState(MakeShareable(new IdleState()));
}

void AGOAPAIController::SetNewState(TSharedPtr<GOAPFSMState> newState)
{
	state.Reset();
	state = newState;
	state->Enter(*this);
}

void AGOAPAIController::SetDoActionState()
{
	SetNewState(MakeShareable(new DoActionState()));
}

void AGOAPAIController::SetIdleState()
{
	SetNewState(MakeShareable(new IdleState()));
}

void AGOAPAIController::ClearCurrentActionAndPlan()
{
	// clearing the action and queue will cause IdleState to form a new plan
	CurrentAction = nullptr;
	ActionQueue.Empty();
	SetIdleState();
}

void AGOAPAIController::SetGOAPStateSafe(uint8 eState, bool bValue)
{
	GOAPState.SetState(eState, bValue);
}

void AGOAPAIController::SetMoveToStateWithTarget(TWeakObjectPtr<AActor> aTargetActor, float aAcceptanceRadius)
{
	if (!aTargetActor.IsValid())
	{
		return;
	}

	DrawDebugLine(
		GetWorld(),
		GetPawn()->GetActorLocation(),
		aTargetActor->GetActorLocation(),
		FColor(0, 0, 255),
		true, 5.0f, 0,
		12.333
		);
	AActor* target = aTargetActor.Get(true);
	MoveToActor(target, aAcceptanceRadius);
	MoveToTargetActor = aTargetActor;
	SetNewState(MakeShareable(new MoveToState()));
}

void AGOAPAIController::SetMoveToStateWithLocation(FVector aLocation)
{
	SetNewState(MakeShareable(new MoveToState()));
	MoveToLocation(aLocation, -1.0f);
}

bool AGOAPAIController::BuildActionPlanForCurrentGoal()
{
	TArray<TWeakObjectPtr<UGOAPAction>> planActions;

	// TODO: We only support single state goals right now, awaiting planner upgrade
	FGOAPAtom targetState = CurrentGoal;

	// The goal is already satisfied, discard it
	if (isStateSatisfied(targetState))
	{
		return false;
	}

	// Make a plan!
	planActions = Planner->Plan(this, targetState.Key, targetState.Value, &GOAPActions, &GOAPState, *this);

	// If we find one, push it into the Action Queue
	if (planActions.Num() > 0)
	{
		for (int i = 0; i < planActions.Num(); ++i)
		{
			ActionQueue.Enqueue(planActions[i]);
		}
	}
	else
	{
		return false;
	}

	return true;
}


FString AGOAPAIController::GetCurrentFSMStateString()
{
	if (state.IsValid())
	{
		return state->ToString();
	}
	return TEXT("No Valid State");
}

FString AGOAPAIController::GetCurrentActionString()
{
	if (CurrentAction != nullptr)
	{
		return CurrentAction->ActionDescription;
	}
	else
	{
		return TEXT("None");
	}
}



TArray<UGOAPAction*> AGOAPAIController::GetValidActionsForState(const FGOAPState aState)
{
	TArray<UGOAPAction*> result;

	for (auto& action : GOAPActions)
	{
		if (action->PreConditions.IsSatisfiesState(aState))
		{
			result.Push(action);
		}
	}

	return result;
}

bool AGOAPAIController::isStateSatisfied(FGOAPAtom aAtom)
{
	return GOAPState.IsStateSatisfied(aAtom.Key, aAtom.Value);
}

bool AGOAPAIController::isStateSatisfied(const uint8 aState, const bool aValue)
{
	return GOAPState.IsStateSatisfied(aState, aValue);
}