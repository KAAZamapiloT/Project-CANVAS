#include "PaintingResolverLLM.h"
#include "AssetIndexer.h"
#include "LocationQueryEngine.h"
#include "SceneStateTracker.h"
#include "Kismet/GameplayStatics.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "GenAIUtils.h"
#include"API_KEY.h"
void UPaintingResolverLLM::RequestPaintingPlan(FString UserPrompt, UWorld* World, UAssetIndexer* Indexer)
{
    if (!World || !Indexer) return;

    // 1. Get Spatial Context (The Canvas)
    USceneStateTracker* Tracker = UGameplayStatics::GetGameInstance(World)->GetSubsystem<USceneStateTracker>();
    FString BoundsContext = "Default 5000x5000"; 
    
    if (Tracker && Tracker->LocationEngine)
    {
        // Reuse your existing context builder or pull bounds directly
        FBox Bounds = Tracker->LocationEngine->GetPlayableAreaBounds();
        BoundsContext = FString::Printf(TEXT("Canvas Size: X[%.0f to %.0f], Y[%.0f to %.0f]"), 
            Bounds.Min.X, Bounds.Max.X, Bounds.Min.Y, Bounds.Max.Y);
    }

    // 2. Build the "Director" Prompt
    FString Payload = CreateDirectorPayload(UserPrompt, Indexer, BoundsContext);

    // 3. Send to Groq (Llama 3.3)
    FString APIKey = API_KEY::GetKey(); // REPLACE WITH YOUR KEY or use API_KEY::GroqKey()
    
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL("https://api.groq.com/openai/v1/chat/completions");
    Request->SetVerb("POST");
    Request->SetHeader("Content-Type", "application/json");
    Request->SetHeader("Authorization", FString::Printf(TEXT("Bearer %s"), *APIKey));

    Request->SetContentAsString(Payload);
    Request->OnProcessRequestComplete().BindUObject(this, &UPaintingResolverLLM::OnPlanReceived);
    Request->ProcessRequest();
}

FString UPaintingResolverLLM::CreateDirectorPayload(FString UserPrompt, UAssetIndexer* Indexer, FString BoundsContext)
{
    // Fetch broad categories to give the LLM a palette (without listing 500 files)
    // You might want to implement GetSemanticCategories() in AssetIndexer
    FString SemanticSummary = "Available Themes: [SciFi, Nature, Industrial, Horror, Medieval]"; 

    return FString::Printf(TEXT(
        "{"
        "  \"model\": \"llama-3.3-70b-versatile\","
        "  \"messages\": ["
        "    {\"role\": \"system\", \"content\": \""
        "You are a Level Director. Do NOT output coordinates. Output LAYOUT COMMANDS.\\n"
        "Your goal is to paint the canvas using procedural tools.\\n\\n"
        
        "--- THE TOOLKIT ---\\n"
        "1. FILL_PERIMETER: Creates walls/fences along the map edges.\\n"
        "   - Settings: 'Height' (1-3), 'Spacing' (300-600)\\n"
        "2. SCATTER_CLUSTER: Randomly places debris, nature, or clutter.\\n"
        "   - Settings: 'Count' (5-50), 'Radius' (200-1000), 'MinClearance' (50-200)\\n"
        "3. GRID_FILL: Orderly rows of objects (chairs, pillars, containers).\\n"
        "   - Settings: 'Rows' (2-5), 'Cols' (2-5), 'Spacing' (100-500)\\n"
        "4. RING: Creates a perfect circle (rituals, meetings).\\n"
        "   - Settings: 'Count' (3-12), 'Radius' (200-800)\\n\\n"
        
        "--- TARGET ZONES ---\\n"
        "CENTER, CORNER_LEFT, CORNER_RIGHT, BACKGROUND, FOREGROUND, PLAYER_NEAR\\n\\n"
        
        "--- OUTPUT SCHEMA (JSON) ---\\n"
        "{\\n"
        "  \\\"LayoutCommands\\\": [\\n"
        "    {\\n"
        "      \\\"Tool\\\": \\\"SCATTER_CLUSTER\\\",\\n"
        "      \\\"Archetype\\\": \\\"Prop\\\",\\n"
        "      \\\"TargetZone\\\": \\\"CENTER\\\",\\n"
        "      \\\"Style\\\": { \\\"MeshKeyword\\\": \\\"Rock\\\", \\\"MaterialKeyword\\\": \\\"Mossy_Stone\\\" },\\n"
        "      \\\"Settings\\\": { \\\"Count\\\": 10, \\\"Radius\\\": 500 }\\n"
        "    }\\n"
        "  ]\\n"
        "}\"},"
        
        "    {\"role\": \"user\", \"content\": \"Context: %s\\nRequest: \\\"%s\\\"\"}"
        "  ],"
        "  \"temperature\": 0.3,"
        "  \"response_format\": {\"type\": \"json_object\"}"
        "}"
    ), 
    *BoundsContext, 
    *UserPrompt.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT(" ")));
}

void UPaintingResolverLLM::OnPlanReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid()) 
    {
        OnPaintingPlanReady.Broadcast("{}", "Failed");
        return;
    }

    // 1. Standard OpenAI/Groq Parsing
    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
    
    FString ContentStr;
    if (FJsonSerializer::Deserialize(Reader, JsonObj))
    {
        const TArray<TSharedPtr<FJsonValue>>* Choices;
        if (JsonObj->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num() > 0)
        {
            ContentStr = (*Choices)[0]->AsObject()->GetObjectField(TEXT("message"))->GetStringField(TEXT("content"));
        }
    }
    // PaintingResolverLLM.cpp - OnPlanReceived
    FString CleanString = UGenAIUtils::CleanLLMResponse(ContentStr);

    // ✅ FIX: Change "PaintingCommands" to "LayoutCommands"
    if (CleanString.Contains("LayoutCommands")) 
    {
        OnPaintingPlanReady.Broadcast(CleanString, "Success");
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("PaintingResolver: Invalid JSON received: %s"), *CleanString);
        OnPaintingPlanReady.Broadcast("{}", "Invalid Format");
    }
}

