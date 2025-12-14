// Fill out your copyright notice in the Description page of Project Settings.


#include "TextureResolverLLM.h"
//#include "JsonParser.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Kismet/GameplayStatics.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "AssetIndexer.h"
#include "SceneStateTracker.h"
#include"RandomAssetSelector.h"
#include"API_KEY.h"

FString UTextureResolverLLM::CreateMasterPrompt(FString UserPrompt, UAssetIndexer* AssetIndexer)
{
    // [OPTIMIZATION] Use Base Names (Keys) instead of Full Paths
    // The LLM needs "Wood", not "/Game/Textures/T_Wood_BC.uasset"
    TArray<FString> AvailableMaterials = AssetIndexer->GetMaterialBaseNames(); 
    TArray<FString> AvailableTags = AssetIndexer->GetDiscoveredActorTags();
    
    // Join with quotes
    FString MaterialsString = FString::Join(AvailableMaterials, TEXT("\", \""));
    FString TagString = FString::Join(AvailableTags, TEXT("\", \""));

    FString MasterPrompt = FString::Printf(TEXT(
            "ROLE: You are the Lead Material Specialist (Interior Designer) for a game production pipeline.\n"
            "OBJECTIVE: Create precise surface specifications to transform the scene according to the client's vision.\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "CLIENT CREATIVE BRIEF\n"
            "═══════════════════════════════════════════════════════════════\n"
            "\"%s\"\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "SITE SURVEY - TARGETABLE SURFACES\n"
            "═══════════════════════════════════════════════════════════════\n"
            "You may ONLY modify actors tagged with these exact identifiers:\n"
            "[\"%s\"]\n\n"
            
            "IMPORTANT: TagName values must match these EXACTLY (case-sensitive).\n"
            "Each tag represents a group of scene objects with shared properties.\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "WAREHOUSE INVENTORY - APPROVED MATERIALS\n"
            "═══════════════════════════════════════════════════════════════\n"
            "You must select textures exclusively from this verified list:\n"
            "[\"%s\"]\n\n"
            
            "CRITICAL RULES:\n"
            "• Use ONLY base names from this list (e.g., \"Wood\", \"Metal\", \"Concrete\")\n"
            "• Do NOT add file extensions or paths\n"
            "• Do NOT invent material names not listed above\n"
            "• If a perfect match doesn't exist, use the closest thematic alternative\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "PBR TEXTURE SYSTEM\n"
            "═══════════════════════════════════════════════════════════════\n"
            
            "Each material can have multiple texture maps for realism:\n\n"
            
            "REQUIRED:\n"
            "• BaseColorPath: The main color/diffuse texture (ALWAYS provide this)\n\n"
            
            "OPTIONAL (Provide if available in inventory):\n"
            "• NormalPath: Surface detail (bumps, grooves)\n"
            "• RoughnessPath: Surface smoothness (0=mirror, 1=matte)\n"
            "• MetallicPath: Metallic properties (0=non-metal, 1=pure metal)\n"
            "• AOPath: Ambient occlusion (shadows in crevices)\n\n"
            
            "TEXTURE NAMING CONVENTION:\n"
            "If inventory contains \"Wood\", check for:\n"
            "  • Wood (base color) → Use as BaseColorPath\n"
            "  • Wood_N (normal) → Use as NormalPath\n"
            "  • Wood_R (roughness) → Use as RoughnessPath\n"
            "  • Wood_M (metallic) → Use as MetallicPath\n"
            "  • Wood_AO (ambient occlusion) → Use as AOPath\n\n"
            
            "If only base material exists, populate ONLY BaseColorPath.\n"
            "Leave other fields as empty strings (\"\").\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "COLOR TINTING SYSTEM\n"
            "═══════════════════════════════════════════════════════════════\n"
            
            "PropColor: [R, G, B] where each value is an INTEGER from 0-255\n\n"
            
            "PURPOSE: Tint the texture to match theme mood\n"
            "• Neutral/Natural: [255, 255, 255] (white = no tint)\n"
            "• Warm/Fire: [255, 180, 120] (orange tint)\n"
            "• Cool/Ice: [180, 220, 255] (blue tint)\n"
            "• Toxic/Radioactive: [150, 255, 100] (green tint)\n"
            "• Mars/Desert: [255, 150, 100] (red-orange tint)\n\n"
            
            "EXAMPLES:\n"
            "• \"Concrete\" + [255, 100, 100] = Reddish concrete (blood-stained)\n"
            "• \"Metal\" + [100, 255, 200] = Greenish metal (toxic waste)\n"
            "• \"Stone\" + [200, 200, 255] = Bluish stone (frozen/ice cave)\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "THEMATIC MAPPING GUIDELINES\n"
            "═══════════════════════════════════════════════════════════════\n"
            
            "SCI-FI / CYBERPUNK:\n"
            "  Materials: Metal, Plastic, Tech, Carbon\n"
            "  Colors: Cool blues [100-200, 150-255, 200-255], neon accents\n\n"
            
            "NATURE / ORGANIC:\n"
            "  Materials: Wood, Stone, Grass, Dirt, Moss\n"
            "  Colors: Natural earth tones [180-255, 180-220, 150-200]\n\n"
            
            "HORROR / ABANDONED:\n"
            "  Materials: Rust, Concrete, Decay, Rot\n"
            "  Colors: Desaturated [100-180, 100-180, 100-180], blood reds\n\n"
            
            "FANTASY / MAGICAL:\n"
            "  Materials: Crystal, Marble, Gold, Gem\n"
            "  Colors: Vibrant purples, golds, mystical glows\n\n"
            
            "INDUSTRIAL / WAREHOUSE:\n"
            "  Materials: Metal, Concrete, Steel, Iron\n"
            "  Colors: Greys [150-200, 150-200, 150-200], rust browns\n\n"
            
            "DESERT / ARID:\n"
            "  Materials: Sand, Rock, Clay, Sandstone\n"
            "  Colors: Warm yellows/oranges [255, 220, 180]\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "EXECUTION PROTOCOL\n"
            "═══════════════════════════════════════════════════════════════\n"
            
            "STEP 1: ANALYZE CLIENT BRIEF\n"
            "• Identify primary theme keywords (e.g., \"cyberpunk\", \"forest\", \"horror\")\n"
            "• Determine mood (bright/dark, warm/cool, clean/dirty)\n"
            "• Note any specific material requests\n\n"
            
            "STEP 2: MAP TAGS TO MATERIALS\n"
            "• Review available tags from Site Survey\n"
            "• Match each tag to appropriate material from inventory\n"
            "• Prioritize thematic consistency over variety\n\n"
            
            "STEP 3: APPLY COLOR GRADING\n"
            "• Use PropColor to reinforce theme mood\n"
            "• Maintain color harmony across all props\n"
            "• Use tinting to create atmosphere (warm sunset, cool moonlight, etc.)\n\n"
            
            "STEP 4: COMPREHENSIVE COVERAGE\n"
            "• Try to assign materials to ALL relevant tags\n"
            "• If a tag doesn't fit theme, use neutral material (e.g., Concrete)\n"
            "• Aim for 70-100%% tag coverage for immersive transformation\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "JSON OUTPUT SCHEMA\n"
            "═══════════════════════════════════════════════════════════════\n"
            
            "{\n"
            "  \"ThemeName\": \"creative_descriptive_name\",\n"
            "  \"bModifyProps\": true,\n"
            "  \"Props\": [\n"
            "    {\n"
            "      \"TagName\": \"EXACT_TAG_FROM_SITE_SURVEY\",\n"
            "      \"PropColor\": [R, G, B],\n"
            "      \"Texture\": {\n"
            "        \"BaseColorPath\": \"EXACT_NAME_FROM_INVENTORY\",\n"
            "        \"NormalPath\": \"optional_name_or_empty\",\n"
            "        \"RoughnessPath\": \"optional_name_or_empty\",\n"
            "        \"MetallicPath\": \"optional_name_or_empty\",\n"
            "        \"AOPath\": \"optional_name_or_empty\"\n"
            "      },\n"
            "      \"ParticleEffects\": \"\"\n"
            "    }\n"
            "  ]\n"
            "}\n\n"
            
            "FIELD NOTES:\n"
            "• ThemeName: Short descriptor (e.g., \"Cyberpunk_Neon\", \"Forest_Autumn\")\n"
            "• bModifyProps: Always set to true\n"
            "• PropColor: RGB integers 0-255, NOT floats\n"
            "• ParticleEffects: Leave empty (\"\") for now (reserved for future use)\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "EXAMPLE OUTPUT - Cyberpunk Theme\n"
            "═══════════════════════════════════════════════════════════════\n"
            
            "{\n"
            "  \"ThemeName\": \"Cyberpunk_Neon_District\",\n"
            "  \"bModifyProps\": true,\n"
            "  \"Props\": [\n"
            "    {\n"
            "      \"TagName\": \"Floor\",\n"
            "      \"PropColor\": [80, 100, 140],\n"
            "      \"Texture\": {\n"
            "        \"BaseColorPath\": \"Metal\",\n"
            "        \"NormalPath\": \"Metal_N\",\n"
            "        \"RoughnessPath\": \"Metal_R\",\n"
            "        \"MetallicPath\": \"Metal_M\",\n"
            "        \"AOPath\": \"Metal_AO\"\n"
            "      },\n"
            "      \"ParticleEffects\": \"\"\n"
            "    },\n"
            "    {\n"
            "      \"TagName\": \"Wall\",\n"
            "      \"PropColor\": [60, 60, 80],\n"
            "      \"Texture\": {\n"
            "        \"BaseColorPath\": \"Concrete\",\n"
            "        \"NormalPath\": \"Concrete_N\",\n"
            "        \"RoughnessPath\": \"\",\n"
            "        \"MetallicPath\": \"\",\n"
            "        \"AOPath\": \"\"\n"
            "      },\n"
            "      \"ParticleEffects\": \"\"\n"
            "    },\n"
            "    {\n"
            "      \"TagName\": \"Furniture\",\n"
            "      \"PropColor\": [255, 100, 200],\n"
            "      \"Texture\": {\n"
            "        \"BaseColorPath\": \"Plastic\",\n"
            "        \"NormalPath\": \"\",\n"
            "        \"RoughnessPath\": \"\",\n"
            "        \"MetallicPath\": \"\",\n"
            "        \"AOPath\": \"\"\n"
            "      },\n"
            "      \"ParticleEffects\": \"\"\n"
            "    }\n"
            "  ]\n"
            "}\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "VALIDATION CHECKLIST\n"
            "═══════════════════════════════════════════════════════════════\n"
            
            "Before submitting, verify:\n"
            "✓ Every TagName matches Site Survey list exactly\n"
            "✓ Every BaseColorPath matches Warehouse Inventory exactly\n"
            "✓ All PropColor values are integers 0-255\n"
            "✓ Optional texture paths are either valid names or empty strings\n"
            "✓ JSON is valid (proper brackets, commas, quotes)\n"
            "✓ bModifyProps is set to true\n"
            "✓ ParticleEffects is empty string for all entries\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "EDGE CASE HANDLING\n"
            "═══════════════════════════════════════════════════════════════\n"
            
            "• If client brief is vague: Default to neutral theme with [255,255,255] colors\n"
            "• If no perfect material match: Use closest semantic alternative\n"
            "• If inventory is empty: Return minimal schema with empty Props array\n"
            "• If tag list is empty: Return valid schema noting no surfaces available\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "OUTPUT INSTRUCTIONS\n"
            "═══════════════════════════════════════════════════════════════\n"
            
            "1. Return ONLY valid JSON (no markdown, no commentary, no explanations)\n"
            "2. Start with '{' and end with '}'\n"
            "3. Do not include ```json code blocks\n"
            "4. Ensure proper escaping of any special characters\n"
            "5. Double-check all array brackets and commas\n\n"
            
            "GENERATE SURFACE SPECIFICATION PLAN NOW:"
        ),
        *UserPrompt,
        *TagString,
        *MaterialsString
        );

    return MasterPrompt;
}

FString UTextureResolverLLM::CreatePrunedTexturePayload(FString UserPrompt,  FString& PrunedAssets,
	UAssetIndexer* Indexer)
{
	// 1. Parse String back to Array
	TArray<FString> RawAssets;
	PrunedAssets.ParseIntoArray(RawAssets, TEXT(","), true);

	TArray<FString> RelevantMaterials;
	TArray<FString> AllBaseNames = Indexer->GetMaterialBaseNames(); 

	// 2. Filter for Materials
	for (FString& Item : RawAssets)
	{
		Item.TrimStartAndEndInline();
		FString Candidate = FPaths::GetBaseFilename(Item);
        
		if (AllBaseNames.Contains(Candidate))
		{
			RelevantMaterials.Add(Candidate);
		}
	}

	// Fallback if empty
	if (RelevantMaterials.Num() == 0)
	{
		RelevantMaterials = Indexer->GetSmartMaterialList(); 
	}

	FString MaterialString = FString::Join(RelevantMaterials, TEXT("\", \""));
	FString TagString = FString::Join(Indexer->GetDiscoveredActorTags(), TEXT("\", \""));
	FString MasterPrompt = FString::Printf(TEXT(
            "ROLE: You are the Lead Material Specialist (Interior Designer) for a game production pipeline.\n"
            "OBJECTIVE: Create precise surface specifications to transform the scene according to the client's vision.\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "CLIENT CREATIVE BRIEF\n"
            "═══════════════════════════════════════════════════════════════\n"
            "\"%s\"\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "SITE SURVEY - TARGETABLE SURFACES\n"
            "═══════════════════════════════════════════════════════════════\n"
            "You may ONLY modify actors tagged with these exact identifiers:\n"
            "[\"%s\"]\n\n"
            
            "IMPORTANT: TagName values must match these EXACTLY (case-sensitive).\n"
            "Each tag represents a group of scene objects with shared properties.\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "WAREHOUSE INVENTORY - APPROVED MATERIALS\n"
            "═══════════════════════════════════════════════════════════════\n"
            "You must select textures exclusively from this verified list:\n"
            "[\"%s\"]\n\n"
            
            "CRITICAL RULES:\n"
            "• Use ONLY base names from this list (e.g., \"Wood\", \"Metal\", \"Concrete\")\n"
            "• Do NOT add file extensions or paths\n"
            "• Do NOT invent material names not listed above\n"
            "• If a perfect match doesn't exist, use the closest thematic alternative\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "PBR TEXTURE SYSTEM\n"
            "═══════════════════════════════════════════════════════════════\n"
            
            "Each material can have multiple texture maps for realism:\n\n"
            
            "REQUIRED:\n"
            "• BaseColorPath: The main color/diffuse texture (ALWAYS provide this)\n\n"
            
            "OPTIONAL (Provide if available in inventory):\n"
            "• NormalPath: Surface detail (bumps, grooves)\n"
            "• RoughnessPath: Surface smoothness (0=mirror, 1=matte)\n"
            "• MetallicPath: Metallic properties (0=non-metal, 1=pure metal)\n"
            "• AOPath: Ambient occlusion (shadows in crevices)\n\n"
            
            "TEXTURE NAMING CONVENTION:\n"
            "If inventory contains \"Wood\", check for:\n"
            "  • Wood (base color) → Use as BaseColorPath\n"
            "  • Wood_N (normal) → Use as NormalPath\n"
            "  • Wood_R (roughness) → Use as RoughnessPath\n"
            "  • Wood_M (metallic) → Use as MetallicPath\n"
            "  • Wood_AO (ambient occlusion) → Use as AOPath\n\n"
            
            "If only base material exists, populate ONLY BaseColorPath.\n"
            "Leave other fields as empty strings (\"\").\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "COLOR TINTING SYSTEM\n"
            "═══════════════════════════════════════════════════════════════\n"
            
            "PropColor: [R, G, B] where each value is an INTEGER from 0-255\n\n"
            
            "PURPOSE: Tint the texture to match theme mood\n"
            "• Neutral/Natural: [255, 255, 255] (white = no tint)\n"
            "• Warm/Fire: [255, 180, 120] (orange tint)\n"
            "• Cool/Ice: [180, 220, 255] (blue tint)\n"
            "• Toxic/Radioactive: [150, 255, 100] (green tint)\n"
            "• Mars/Desert: [255, 150, 100] (red-orange tint)\n\n"
            
            "EXAMPLES:\n"
            "• \"Concrete\" + [255, 100, 100] = Reddish concrete (blood-stained)\n"
            "• \"Metal\" + [100, 255, 200] = Greenish metal (toxic waste)\n"
            "• \"Stone\" + [200, 200, 255] = Bluish stone (frozen/ice cave)\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "THEMATIC MAPPING GUIDELINES\n"
            "═══════════════════════════════════════════════════════════════\n"
            
            "SCI-FI / CYBERPUNK:\n"
            "  Materials: Metal, Plastic, Tech, Carbon\n"
            "  Colors: Cool blues [100-200, 150-255, 200-255], neon accents\n\n"
            
            "NATURE / ORGANIC:\n"
            "  Materials: Wood, Stone, Grass, Dirt, Moss\n"
            "  Colors: Natural earth tones [180-255, 180-220, 150-200]\n\n"
            
            "HORROR / ABANDONED:\n"
            "  Materials: Rust, Concrete, Decay, Rot\n"
            "  Colors: Desaturated [100-180, 100-180, 100-180], blood reds\n\n"
            
            "FANTASY / MAGICAL:\n"
            "  Materials: Crystal, Marble, Gold, Gem\n"
            "  Colors: Vibrant purples, golds, mystical glows\n\n"
            
            "INDUSTRIAL / WAREHOUSE:\n"
            "  Materials: Metal, Concrete, Steel, Iron\n"
            "  Colors: Greys [150-200, 150-200, 150-200], rust browns\n\n"
            
            "DESERT / ARID:\n"
            "  Materials: Sand, Rock, Clay, Sandstone\n"
            "  Colors: Warm yellows/oranges [255, 220, 180]\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "EXECUTION PROTOCOL\n"
            "═══════════════════════════════════════════════════════════════\n"
            
            "STEP 1: ANALYZE CLIENT BRIEF\n"
            "• Identify primary theme keywords (e.g., \"cyberpunk\", \"forest\", \"horror\")\n"
            "• Determine mood (bright/dark, warm/cool, clean/dirty)\n"
            "• Note any specific material requests\n\n"
            
            "STEP 2: MAP TAGS TO MATERIALS\n"
            "• Review available tags from Site Survey\n"
            "• Match each tag to appropriate material from inventory\n"
            "• Prioritize thematic consistency over variety\n\n"
            
            "STEP 3: APPLY COLOR GRADING\n"
            "• Use PropColor to reinforce theme mood\n"
            "• Maintain color harmony across all props\n"
            "• Use tinting to create atmosphere (warm sunset, cool moonlight, etc.)\n\n"
            
            "STEP 4: COMPREHENSIVE COVERAGE\n"
            "• Try to assign materials to ALL relevant tags\n"
            "• If a tag doesn't fit theme, use neutral material (e.g., Concrete)\n"
            "• Aim for 70-100%% tag coverage for immersive transformation\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "JSON OUTPUT SCHEMA\n"
            "═══════════════════════════════════════════════════════════════\n"
            
            "{\n"
            "  \"ThemeName\": \"creative_descriptive_name\",\n"
            "  \"bModifyProps\": true,\n"
            "  \"Props\": [\n"
            "    {\n"
            "      \"TagName\": \"EXACT_TAG_FROM_SITE_SURVEY\",\n"
            "      \"PropColor\": [R, G, B],\n"
            "      \"Texture\": {\n"
            "        \"BaseColorPath\": \"EXACT_NAME_FROM_INVENTORY\",\n"
            "        \"NormalPath\": \"optional_name_or_empty\",\n"
            "        \"RoughnessPath\": \"optional_name_or_empty\",\n"
            "        \"MetallicPath\": \"optional_name_or_empty\",\n"
            "        \"AOPath\": \"optional_name_or_empty\"\n"
            "      },\n"
            "      \"ParticleEffects\": \"\"\n"
            "    }\n"
            "  ]\n"
            "}\n\n"
            
            "FIELD NOTES:\n"
            "• ThemeName: Short descriptor (e.g., \"Cyberpunk_Neon\", \"Forest_Autumn\")\n"
            "• bModifyProps: Always set to true\n"
            "• PropColor: RGB integers 0-255, NOT floats\n"
            "• ParticleEffects: Leave empty (\"\") for now (reserved for future use)\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "EXAMPLE OUTPUT - Cyberpunk Theme\n"
            "═══════════════════════════════════════════════════════════════\n"
            
            "{\n"
            "  \"ThemeName\": \"Cyberpunk_Neon_District\",\n"
            "  \"bModifyProps\": true,\n"
            "  \"Props\": [\n"
            "    {\n"
            "      \"TagName\": \"Floor\",\n"
            "      \"PropColor\": [80, 100, 140],\n"
            "      \"Texture\": {\n"
            "        \"BaseColorPath\": \"Metal\",\n"
            "        \"NormalPath\": \"Metal_N\",\n"
            "        \"RoughnessPath\": \"Metal_R\",\n"
            "        \"MetallicPath\": \"Metal_M\",\n"
            "        \"AOPath\": \"Metal_AO\"\n"
            "      },\n"
            "      \"ParticleEffects\": \"\"\n"
            "    },\n"
            "    {\n"
            "      \"TagName\": \"Wall\",\n"
            "      \"PropColor\": [60, 60, 80],\n"
            "      \"Texture\": {\n"
            "        \"BaseColorPath\": \"Concrete\",\n"
            "        \"NormalPath\": \"Concrete_N\",\n"
            "        \"RoughnessPath\": \"\",\n"
            "        \"MetallicPath\": \"\",\n"
            "        \"AOPath\": \"\"\n"
            "      },\n"
            "      \"ParticleEffects\": \"\"\n"
            "    },\n"
            "    {\n"
            "      \"TagName\": \"Furniture\",\n"
            "      \"PropColor\": [255, 100, 200],\n"
            "      \"Texture\": {\n"
            "        \"BaseColorPath\": \"Plastic\",\n"
            "        \"NormalPath\": \"\",\n"
            "        \"RoughnessPath\": \"\",\n"
            "        \"MetallicPath\": \"\",\n"
            "        \"AOPath\": \"\"\n"
            "      },\n"
            "      \"ParticleEffects\": \"\"\n"
            "    }\n"
            "  ]\n"
            "}\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "VALIDATION CHECKLIST\n"
            "═══════════════════════════════════════════════════════════════\n"
            
            "Before submitting, verify:\n"
            "✓ Every TagName matches Site Survey list exactly\n"
            "✓ Every BaseColorPath matches Warehouse Inventory exactly\n"
            "✓ All PropColor values are integers 0-255\n"
            "✓ Optional texture paths are either valid names or empty strings\n"
            "✓ JSON is valid (proper brackets, commas, quotes)\n"
            "✓ bModifyProps is set to true\n"
            "✓ ParticleEffects is empty string for all entries\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "EDGE CASE HANDLING\n"
            "═══════════════════════════════════════════════════════════════\n"
            
            "• If client brief is vague: Default to neutral theme with [255,255,255] colors\n"
            "• If no perfect material match: Use closest semantic alternative\n"
            "• If inventory is empty: Return minimal schema with empty Props array\n"
            "• If tag list is empty: Return valid schema noting no surfaces available\n\n"
            
            "═══════════════════════════════════════════════════════════════\n"
            "OUTPUT INSTRUCTIONS\n"
            "═══════════════════════════════════════════════════════════════\n"
            
            "1. Return ONLY valid JSON (no markdown, no commentary, no explanations)\n"
            "2. Start with '{' and end with '}'\n"
            "3. Do not include ```json code blocks\n"
            "4. Ensure proper escaping of any special characters\n"
            "5. Double-check all array brackets and commas\n\n"
            
            "GENERATE SURFACE SPECIFICATION PLAN NOW:"
        ),
        *UserPrompt,
        *TagString,
        *MaterialString
        );

    return MasterPrompt;
}

void UTextureResolverLLM::OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSucessfull)
{
	if (!bWasSucessfull || !Response.IsValid()) { OnTexturePlanReady.Broadcast("{}", ""); return; }

	FString ResponseString = Response->GetContentAsString();
	TSharedPtr<FJsonObject> Object;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

	if (FJsonSerializer::Deserialize(Reader, Object) && Object.IsValid())
	{
		FString LLMResponseString;

		// 6. PARSE GEMINI RESPONSE STRUCTURE (candidates -> content -> parts -> text)
		const TArray<TSharedPtr<FJsonValue>>* Candidates;
		if (Object->TryGetArrayField(TEXT("candidates"), Candidates) && Candidates->Num() > 0)
		{
			TSharedPtr<FJsonObject> ContentObj = (*Candidates)[0]->AsObject()->GetObjectField(TEXT("content"));
			const TArray<TSharedPtr<FJsonValue>>* Parts;
			if (ContentObj->TryGetArrayField(TEXT("parts"), Parts) && Parts->Num() > 0)
			{
				LLMResponseString = (*Parts)[0]->AsObject()->GetStringField(TEXT("text"));
			}
		}
		else
		{
			// Log Raw Error if Gemini fails
			UE_LOG(LogTemp, Error, TEXT("TextureResolver (Gemini) Error: %s"), *ResponseString);
			OnTexturePlanReady.Broadcast("{}", "");
			return;
		}

		// 7. Prune & Broadcast
		USceneStateTracker* Tracker = UGameplayStatics::GetGameInstance(GetWorld())->GetSubsystem<USceneStateTracker>();
		if (Tracker && Tracker->AssetIndexer)
		{
			FString Pruned = URandomAssetSelector::PruneTextureAssets(LLMResponseString, Tracker->AssetIndexer);
			UE_LOG(LogTemp, Log, TEXT("TextureResolver: Success! Pruned Context size: %d"), Pruned.Len());
			OnTexturePlanReady.Broadcast(LLMResponseString, Pruned);
		}
		else
		{
			OnTexturePlanReady.Broadcast(LLMResponseString, "");
		}
	}
	else
	{
		OnTexturePlanReady.Broadcast("{}", "");
	}

	
}

void UTextureResolverLLM::RequestPlan(FString UserPrompt, UWorld* World, class USceneHistoryManager* HistoryManager)
{
	if (!World || !IsValid(World))
	{
		UE_LOG(LogTemp, Error, TEXT("TextureResolver: Invalid World Context"));
		OnTexturePlanReady.Broadcast("{}", "");
		return;
	}
	USceneStateTracker* Tracker = UGameplayStatics::GetGameInstance(World)->GetSubsystem<USceneStateTracker>();
	if (!Tracker || !Tracker->AssetIndexer) return;
	
	FString PlanPrompt=CreateMasterPrompt(UserPrompt,Tracker->AssetIndexer);
	// 3. SWITCH TO GEMINI KEY
	FString GeminiKey = API_KEY::GemKey(); 

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    
	// 4. USE GEMINI URL
	FString Url = FString::Printf(TEXT("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash-lite:generateContent?key=%s"), *GeminiKey);
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// 5. USE GEMINI PAYLOAD STRUCTURE
	FString Payload = FString::Printf(TEXT(
	   "{"
	   "  \"contents\": [{"
	   "    \"parts\": [{"
	   "      \"text\": \"%s\""
	   "    }]"
	   "  }],"
	   "  \"generationConfig\": {"
	   "    \"temperature\": 0.25,"
	   "    \"responseMimeType\": \"application/json\"" 
	   "  }"
	   "}"
	), *PlanPrompt.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n")));

	Request->SetContentAsString(Payload);
	Request->OnProcessRequestComplete().BindUObject(this, &UTextureResolverLLM::OnResponseReceived);
	Request->ProcessRequest();
}

void UTextureResolverLLM::RequestPlan_Pruned(FString UserPrompt, FString& PrunedAssets, UWorld* World, USceneHistoryManager* HistoryManager)
{
	if (!World || !IsValid(World))
	{
		UE_LOG(LogTemp, Error, TEXT("TextureResolver: Invalid World Context"));
		OnTexturePlanReady.Broadcast("{}", "");
		return;
	}
	USceneStateTracker* Tracker = UGameplayStatics::GetGameInstance(World)->GetSubsystem<USceneStateTracker>();
	if (!Tracker || !Tracker->AssetIndexer) return;
	
	FString PlanPrompt=CreatePrunedTexturePayload(UserPrompt,PrunedAssets,Tracker->AssetIndexer);
	// 3. SWITCH TO GEMINI KEY
	FString GeminiKey = API_KEY::GemKey(); 

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    
	// 4. USE GEMINI URL
	FString Url = FString::Printf(TEXT("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash-lite:generateContent?key=%s"), *GeminiKey);
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// 5. USE GEMINI PAYLOAD STRUCTURE
	FString Payload = FString::Printf(TEXT(
	   "{"
	   "  \"contents\": [{"
	   "    \"parts\": [{"
	   "      \"text\": \"%s\""
	   "    }]"
	   "  }],"
	   "  \"generationConfig\": {"
	   "    \"temperature\": 0.25,"
	   "    \"responseMimeType\": \"application/json\"" 
	   "  }"
	   "}"
	), *PlanPrompt.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n")));

	Request->SetContentAsString(Payload);
	Request->OnProcessRequestComplete().BindUObject(this, &UTextureResolverLLM::OnResponseReceived);
	Request->ProcessRequest();
}
