// Fill out your copyright notice in the Description page of Project Settings.


// CombatDecisionEngine.cpp
// Implementation of the rule-based combat decision system

#include "CombatDecisionEngine.h"

FActionCommand UCombatDecisionEngine::DecideNextMove(const FContextVector& Context)
{
    // Validate DataTable is assigned
    if (!MoveDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("DecisionEngine: MoveDataTable is null!"));
        return FActionCommand();  // Return empty command
    }

    FName SelectedMove = NAME_None;

    // RULE 0 : CHECK IF IT'S FORST MOVE
    if (Context.LastMoveExecuted==NAME_None)
    {
        SelectedMove = Context.CurrentInput.IsNone() ? FName("LightAttack") : Context.CurrentInput;
       // UE_LOG(LogTemp, Log, TEXT("🎬 First Move: %s"), *SelectedMove.ToString());
        
    }else
    {
    // =====================================
    // RULE 1: PUNISH STUNNED ENEMY
    // =====================================
    // If enemy is stunned, use heavy attack for maximum damage
    
    if (Context.EnemyStateTags.HasTag(FGameplayTag::RequestGameplayTag(FName("State.Stunned"))))
    {
        
        SelectedMove = FName("HeavyAttack");
       // UE_LOG(LogTemp, Log, TEXT("Decision: Punish Stunned -> HeavyAttack"));
    }
    
    // =====================================
    // RULE 2: GAP CLOSER
    // =====================================
    // If enemy is far away (>300 units), use dash to close distance
    else if (Context.DistanceToEnemy > 300.f)
    {
        
        SelectedMove = FName("Dash");
    //    UE_LOG(LogTemp, Log, TEXT("Decision: Gap Closer -> Dash (distance: %.1f)"), Context.DistanceToEnemy);
    }
    
    // =====================================
    // RULE 3: COMBO CHAINING
    // =====================================
    // If we just completed a move, check for follow-ups
    // RULE 3: COMBO CHAINING (with directional follow-ups)
    else if (!Context.LastMoveExecuted.IsNone())
    {
        FMoveData* LastMove = MoveDataTable->FindRow<FMoveData>(Context.LastMoveExecuted, TEXT(""));
    
        if (LastMove)
        {
            // ✅ Check if follow-up exists for current enemy direction
            // ✅ Check if follow-up exists for current enemy direction
            if (LastMove->FollowUpMoves.Contains(Context.EnemyDirection))
            {
                SelectedMove = LastMove->FollowUpMoves[Context.EnemyDirection];
          //      UE_LOG(LogTemp, Log, TEXT("🔗 Combo: %s → %s (Dir: %d)"),
         //              *Context.LastMoveExecuted.ToString(),
         //              *SelectedMove.ToString(),
        //               (int32)Context.EnemyDirection);
            }

            // Fallback: Try neutral combo if directional doesn't exist
            else if (LastMove->FollowUpMoves.Contains(EInputDirection::EID_NEUTRAL))
            {
                SelectedMove = LastMove->FollowUpMoves[EInputDirection::EID_NEUTRAL];
        //        UE_LOG(LogTemp, Log, TEXT("🔗 Combo (fallback): %s → %s"), 
        //            *Context.LastMoveExecuted.ToString(), 
        //            *SelectedMove.ToString());
            }
        }
    }

    // =====================================
    // RULE 4: DEFAULT FALLBACK
    // =====================================
    // If no other rule triggered, use light attack
    if (SelectedMove.IsNone())
    {
        SelectedMove = FName("LightAttack");
  //      UE_LOG(LogTemp, Log, TEXT("Decision: Default -> LightAttack"));
    }
}
    // =====================================
    // QUERY DATATABLE AND BUILD COMMAND
    // =====================================
    // Look up the selected move in the DataTable
    FMoveData* MoveData = MoveDataTable->FindRow<FMoveData>(SelectedMove, TEXT(""));
    
    if (!MoveData)
    {
   //     UE_LOG(LogTemp, Error, TEXT("Decision: Move '%s' not found in DataTable!"), 
  //             *SelectedMove.ToString());
        return FActionCommand();  // Return empty command on error
    }

    // Build and return the Action Command
    return BuildActionCommand(SelectedMove, MoveData);
}

FActionCommand UCombatDecisionEngine::BuildActionCommand(FName MoveIdentifier, const FMoveData* MoveData)
{
    // Construct Action Command from DataTable row
    FActionCommand Command;
    Command.MoveIdentifier = MoveIdentifier;
    Command.AnimationToPlay = MoveData->AnimationToPlay;
    Command.DamageToApply = MoveData->Damage;
    Command.StunDurationToInflict = MoveData->HitStunDuration;
    Command.MovementToApply = FVector::ZeroVector;  // TODO: Implement dash movement
    
    return Command;
}



