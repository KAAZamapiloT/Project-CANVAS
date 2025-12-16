#include "BTTask_MoveToPlayer.h"
#include "EnemyCharacter.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

UBTTask_MoveToPlayer::UBTTask_MoveToPlayer()
{
    NodeName = "Move To Player (2D)";
    bCreateNodeInstance = true;
    bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_MoveToPlayer::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIController = Cast<AAIController>(OwnerComp.GetAIOwner());
    if (!AIController)
        return EBTNodeResult::Failed;

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AIController->GetPawn());
    if (!Enemy)
        return EBTNodeResult::Failed;

    ACharacter* Player = Cast<ACharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
    if (!Player)
        return EBTNodeResult::Failed;

    // ✅ Set movement speed
    if (UCharacterMovementComponent* MovementComp = Enemy->GetCharacterMovement())
    {
        MovementComp->MaxWalkSpeed = MoveSpeed;
    }

 //   UE_LOG(LogTemp, Log, TEXT("🚶 Enemy moving to player (2D mode)"));
    
    return EBTNodeResult::InProgress;
}

void UBTTask_MoveToPlayer::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

    AAIController* AIController = Cast<AAIController>(OwnerComp.GetAIOwner());
    if (!AIController)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AIController->GetPawn());
    ACharacter* Player = Cast<ACharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
    
    if (!Enemy || !Player)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    // ✅ SIDESCROLLER: Get X-axis positions only
    FVector EnemyLoc = Enemy->GetActorLocation();
    FVector PlayerLoc = Player->GetActorLocation();
    
    float DistanceX = FMath::Abs(PlayerLoc.X - EnemyLoc.X);
    float DirectionX = FMath::Sign(PlayerLoc.X - EnemyLoc.X);
    float HeightDiff = PlayerLoc.Z - EnemyLoc.Z; // Positive = Player above enemy
    // ✅ Check if close enough (in range)
    if (DistanceX <= AcceptanceRadius)
    {
        // Stop moving
        if (UCharacterMovementComponent* MovementComp = Enemy->GetCharacterMovement())
        {
            Enemy->AddMovementInput(FVector::ZeroVector);
        }
        
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        return;
    }

    // ✅ SIDESCROLLER: Move along X-axis only
    FVector MoveDirection = FVector(DirectionX, 0.0f, 0.0f);
    Enemy->AddMovementInput(MoveDirection, 1.0f);

    if (Enemy && Player)
    {
        // Optional: Increase facing speed while chasing
        Enemy->OrientToTarget(Player->GetActorLocation(), 20.0f, DeltaSeconds); 
    }

    // ✅ CONDITIONAL JUMP: Only if player is ABOVE enemy
    if (HeightDiff > 100.0f) // Player is higher than enemy by 100 units
    {
        UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement();
        if (Movement)
        {
            if (Movement->IsMovingOnGround())
            {
                // Ground jump
                Enemy->Jump();
               // UE_LOG(LogTemp, Warning, TEXT("🦘 [BT] Enemy ground jump (Player %.1f units above)"), HeightDiff);
            }
            else if (Movement->IsFalling())
            {
                // ✅ DOUBLE JUMP: Only if player is STILL above
                if (Enemy->JumpCurrentCount < Enemy->JumpMaxCount)
                {
                    Enemy->Jump();
                    //UE_LOG(LogTemp, Warning, TEXT("🌟 [BT] Enemy double jump (Player %.1f units above)"), HeightDiff);
                }
            }
        }
    }
    // ✅ NO JUMP: Player is at same height or below
    else
    {
        // Just walk - no jumping needed
      //  UE_LOG(LogTemp, Verbose, TEXT("🚶 [BT] Walking (Player at same level or below, HeightDiff=%.1f)"), HeightDiff);
    }
}

void UBTTask_MoveToPlayer::FacePlayer(AEnemyCharacter* Enemy, ACharacter* Player)
{
    if (!Enemy || !Player)
        return;

    float EnemyX = Enemy->GetActorLocation().X;
    float PlayerX = Player->GetActorLocation().X;
    
    // ✅ Determine facing direction
    bool bShouldFaceRight = (PlayerX > EnemyX);

    // ✅ Get current rotation
    FRotator CurrentRotation = Enemy->GetActorRotation();
    
    // ✅ Flip sprite based on direction
    if (bShouldFaceRight && CurrentRotation.Yaw != 0.0f)
    {
        Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f)); // Face right
    }
    else if (!bShouldFaceRight && CurrentRotation.Yaw != 180.0f)
    {
        Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f)); // Face left
    }
}
