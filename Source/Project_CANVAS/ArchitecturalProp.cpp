#include "ArchitecturalProp.h"

AArchitecturalProp::AArchitecturalProp()
{
	PrimaryActorTick.bCanEverTick = false; // Optimize! Static props don't tick.

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComponent;
    
	// Default settings for architectural props
	MeshComponent->SetMobility(EComponentMobility::Movable); // Needed for runtime changes
	MeshComponent->SetCollisionProfileName(TEXT("BlockAll"));
}