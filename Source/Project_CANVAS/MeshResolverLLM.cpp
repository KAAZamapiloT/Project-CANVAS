// Fill out your copyright notice in the Description page of Project Settings.


#include "MeshResolverLLM.h"

FString UMeshResolverLLM::CreateMeshPayload(FString UserPrompt, FString AvalibleMeshes)
{
	FString MasterPrompt="";


	return MasterPrompt;
}

void UMeshResolverLLM::RequestPlan(FString UserPrompt, UWorld* World, class USceneHistoryManager* HistoryManager)
{
}

void UMeshResolverLLM::OnPlanReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
}
