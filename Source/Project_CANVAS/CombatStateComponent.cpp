// CombatStateComponent.cpp
// Implementation of context vector builder

#include "CombatStateComponent.h"
#include "GameFramework/Character.h"
#include "CombatAnimationComponent.h"
#include "HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h" 
UCombatStateComponent::UCombatStateComponent()
{
    // Enable tick to update cooldowns
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f;  // Check cooldowns every 100ms
}

void UCombatStateComponent::BeginPlay()
{
    Super::BeginPlay();

    // Cache owner character reference
    OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (!OwnerCharacter)
    {
        UE_LOG(LogTemp, Error, TEXT("CombatStateComponent: Owner is not a Character!"));
        return;
    }

    // Cache CombatAnimationComponent reference
    OwnerCombatAnimComp = OwnerCharacter->FindComponentByClass<UCombatAnimationComponent>();
    if (!OwnerCombatAnimComp)
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatStateComponent: Owner has no CombatAnimationComponent"));
    }

    // Auto-find enemy if not set manually
    if (!EnemyCharacter)
    {
        TArray<AActor*> FoundEnemies;
        UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Enemy"), FoundEnemies);
        if (FoundEnemies.Num() > 0)
        {
            EnemyCharacter = Cast<ACharacter>(FoundEnemies[0]);
            UE_LOG(LogTemp, Log, TEXT("CombatStateComponent: Auto-found enemy: %s"), *EnemyCharacter->GetName());
        }
    }
}

void UCombatStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update cooldowns - remove expired entries
    float CurrentTime = GetWorld()->GetTimeSeconds();
    TArray<FName> ExpiredCooldowns;

    for (auto& Pair : ActiveCooldowns)
    {
        if (CurrentTime >= Pair.Value)
        {
            ExpiredCooldowns.Add(Pair.Key);
        }
    }

    for (FName& Expired : ExpiredCooldowns)
    {
        ActiveCooldowns.Remove(Expired);
        UE_LOG(LogTemp, Log, TEXT("Cooldown expired: %s"), *Expired.ToString());
    }
}

// =====================================
// MAIN CONTEXT BUILDER
// =====================================

FContextVector UCombatStateComponent::BuildContext(FName CurrentInput)
{
    FContextVector Context;

    // ===== CURRENT INPUT =====
    Context.CurrentInput = CurrentInput;

    // ===== LAST MOVE EXECUTED =====
    if (OwnerCombatAnimComp)
    {
        Context.LastMoveExecuted = OwnerCombatAnimComp->CurrentMoveIdentifier;
    }

    // ===== DISTANCE TO ENEMY =====
    Context.DistanceToEnemy = GetDistanceToEnemy();

    // ===== PLAYER STATE TAGS =====
    Context.PlayerStateTags = GetCharacterTags(OwnerCharacter);

    // ===== ENEMY STATE TAGS =====
    if (EnemyCharacter)
    {
        Context.EnemyStateTags = GetCharacterTags(EnemyCharacter);
    }

    // ===== ACTIVE COOLDOWNS =====
    ActiveCooldowns.GetKeys(Context.ActiveCooldowns);

    Context.EnemyDirection=CalculateEnemyDirection();    // Log assembled context for debugging
    UE_LOG(LogTemp, Log, TEXT("Context built: Input=%s, LastMove=%s, Distance=%.1f, Cooldowns=%d"),
           *CurrentInput.ToString(),
           *Context.LastMoveExecuted.ToString(),
           Context.DistanceToEnemy,
           Context.ActiveCooldowns.Num());

    return Context;
}

// =====================================
// ENEMY MANAGEMENT
// =====================================

void UCombatStateComponent::SetEnemy(ACharacter* Enemy)
{
    EnemyCharacter = Enemy;
    UE_LOG(LogTemp, Log, TEXT("CombatStateComponent: Enemy set to %s"), 
           Enemy ? *Enemy->GetName() : TEXT("nullptr"));
}

// =====================================
// COOLDOWN MANAGEMENT
// =====================================

void UCombatStateComponent::StartCooldown(FName MoveName, float Duration)
{
    float ExpirationTime = GetWorld()->GetTimeSeconds() + Duration;
    ActiveCooldowns.Add(MoveName, ExpirationTime);
    
    UE_LOG(LogTemp, Log, TEXT("Cooldown started: %s for %.1f seconds"), 
           *MoveName.ToString(), Duration);
}

bool UCombatStateComponent::IsOnCooldown(FName MoveName) const
{
    if (!ActiveCooldowns.Contains(MoveName))
    {
        return false;
    }

    float CurrentTime = GetWorld()->GetTimeSeconds();
    float ExpirationTime = ActiveCooldowns[MoveName];
    
    return CurrentTime < ExpirationTime;
}

EInputDirection UCombatStateComponent::CalculateEnemyDirection()
{
   
    EInputDirection Dir=EInputDirection::EID_NEUTRAL;
    ACharacter* PlayerCharacter = Cast<ACharacter>(GetOwner());
    if (!PlayerCharacter)
    {
        return EInputDirection::EID_NEUTRAL;
    }
    FVector PlayerLocation=OwnerCharacter->GetActorLocation();
    FVector EnemyLocation=EnemyCharacter->GetActorLocation();
    
    // Vertical distance
    float VerticalDelta = EnemyLocation.Z - PlayerLocation.Z;
    
    // Thresholds
    const float VerticalThreshold = 50.0f;  // Tune this value
    
    if (VerticalDelta > VerticalThreshold)
    {
        return EInputDirection::EID_UP;  // Enemy above
    }
    else if (VerticalDelta < -VerticalThreshold)
    {
        return EInputDirection::EID_DOWN;  // Enemy below
    }
    
    // Add horizontal left/right logic if needed for 2.5D
    
    if (PlayerLocation.X<EnemyLocation.X)
    {
        return EInputDirection::EID_RIGHT;
    }else if (PlayerLocation.X>EnemyLocation.X){
        return EInputDirection::EID_LEFT;
    }
    return Dir;
}

// =====================================
// HELPER FUNCTIONS
// =====================================

float UCombatStateComponent::GetDistanceToEnemy() const
{
    if (!OwnerCharacter || !EnemyCharacter)
    {
        return 0.f;
    }

    return FVector::Dist(OwnerCharacter->GetActorLocation(), EnemyCharacter->GetActorLocation());
}

FGameplayTagContainer UCombatStateComponent::GetCharacterTags(ACharacter* Character) const
{
    FGameplayTagContainer Tags;

    if (!Character)
    {
        return Tags;
    }

    // Query HealthComponent for state tags (stunned, blocking, etc.)
    if (UHealthComponent* HealthComp = Character->FindComponentByClass<UHealthComponent>())
    {
        // TODO: Implement tag system in HealthComponent
        // For now, check health state
        if (HealthComp->StunDuration > 0.f)
        {
            Tags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Stunned")));
        }
    }

    // Check if character is in air (jumping)
    if (Character->GetCharacterMovement() && Character->GetCharacterMovement()->IsFalling())
    {
        Tags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Airborne")));
    }

    return Tags;
}
