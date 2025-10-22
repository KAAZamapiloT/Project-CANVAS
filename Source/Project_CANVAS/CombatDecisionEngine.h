// Fill out your copyright notice in the Description page of Project Settings.
// CombatDecisionEngine.h
// The "Black Box" Decision Engine from your design document
// Pure function: Context Vector → Action Command

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CombatData.h"
#include "Engine/DataTable.h"
#include "CombatDecisionEngine.generated.h"

/**
 * UCombatDecisionEngine
 * 
 * Core decision-making system for combat.
 * Implements the "Black Box" architecture from the design document:
 * - Takes Context Vector as input (current game state)
 * - Evaluates rule-based logic
 * - Queries DT_CombatMoves DataTable
 * - Returns Action Command for execution
 * 
 * Rules evaluated in order:
 * 1. Punish Stunned - Heavy attack if enemy is stunned
 * 2. Gap Closer - Dash if enemy is far
 * 3. Combo Chain - Follow-up move if chaining from previous move
 * 4. Default Fallback - Light attack if no other rule applies
 * 
 * Usage:
 * - Create one instance per character (or share between player/AI)
 * - Assign MoveDataTable reference in editor
 * - Call DecideNextMove() with assembled Context Vector
 */
UCLASS(Blueprintable)
class PROJECT_CANVAS_API UCombatDecisionEngine : public UObject
{
    GENERATED_BODY()

public:
    /** 
     * DataTable containing all move definitions (FMoveData rows)
     * Must be assigned in editor or character constructor
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat")
    UDataTable* MoveDataTable;

    /**
     * Main decision function - the "Black Box"
     * Evaluates context and returns the optimal Action Command
     * 
     * @param Context - Current game state snapshot
     * @return Action Command ready for execution
     */
    UFUNCTION(BlueprintCallable, Category="Combat")
    FActionCommand DecideNextMove(const FContextVector& Context);

protected:
    /**
     * Build an Action Command from a DataTable row
     * Populates all fields from FMoveData
     * 
     * @param MoveIdentifier - Row name in DataTable
     * @param MoveData - Pointer to row data
     * @return Populated Action Command
     */
    FActionCommand BuildActionCommand(FName MoveIdentifier, const FMoveData* MoveData);
};

