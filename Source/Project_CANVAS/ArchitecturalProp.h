#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArchitecturalProp.generated.h"

UCLASS()
class PROJECT_CANVAS_API AArchitecturalProp : public AActor
{
	GENERATED_BODY()
    
public:	
	AArchitecturalProp();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenAI")
	UStaticMeshComponent* MeshComponent;

	// The semantic "Soul" of the prop (e.g., "Wall", "Pillar")
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GenAI")
	FString SemanticTag;
};