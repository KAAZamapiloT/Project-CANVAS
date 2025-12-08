# 🎨 **PROJECT CANVAS**  
## **Real-Time Game World Manipulation with LLM-Driven Scene Generation + Context-Aware Combat AI**

<p align="center">
  <img src="https://img.shields.io/badge/Engine-Unreal_Engine_5.2+-black?logo=unrealengine" />
  <img src="https://img.shields.io/badge/Language-C++17-blue?logo=cplusplus" />
  <img src="https://img.shields.io/badge/AI-Generative_LLM-green?logo=openai" />
  <img src="https://img.shields.io/badge/Status-Prototype-orange" />
  <img src="https://img.shields.io/badge/License-MIT-purple" />
</p>

---

<p align="center">
  <strong>🧐 “A Dual-Pipeline Architecture for Real-Time Scene Generation & Combat Intelligence.”</strong>
</p>

---

# 🚀 Overview

**Project CANVAS** merges *Generative AI* with *real-time gameplay systems* inside Unreal Engine 5:

### **1. Scene Generation Pipeline (LLM → JSON → In-Game Scene)**
Players type prompts like:

> “Turn this into a neon cyberpunk night with rain and pink fog.”

The system generates a strict JSON scene plan, resolves assets, finds safe spawn locations, and builds a **new 3D scene live**.

### **2. Context-Aware Combat Pipeline (State → Context Vector → Decision Engine)**
The AI analyzes:
- distance  
- gameplay tags  
- cooldowns  
- orientation  
- player intent  

and executes the **optimal combat move**, forming emergent combo chains.

---

# ✨ Key Features

## 🎨 Generative AI Scene Manipulation
- Natural language → **Structured JSON Scene Plan**
- 5-tier asset resolution
- 11-tier location resolution
- Dynamic lighting, fog, textures & props
- Unified spawning (Meshes + Niagara FX)

---

## ⚔️ Context-Aware Combat AI
- Data-driven **Context Vector**
- Rule-based Decision Engine
- Emergent combo graph
- Millisecond decision latency

---

## 🧱 Modular Architecture
- `GenAISystem`  
- `AssetIndexer`  
- `LocationQueryEngine`  
- `SceneBuilder`  
- `SceneStateTracker`  

---

# 🏗 System Architecture

```
PLAYER INPUT                          TEXT PROMPT
    │                                      │
    ▼                                      ▼
Context Builder                    Prompt Augmenter
    │                                      │
    ▼                                      ▼
Combat Decision Engine              LLM Engine (JSON)
    │                                      │
    ▼                                      ▼
Action Command                   JSON Parser → Scene Plan
    │                                      │
    ▼                                      ▼
CHARACTER SYSTEMS            Scene Builder → Lighting / FX / Props
                                           │
                              Scene State Tracker (Orchestrator)
                                           ▼
                                     GAME OUTPUT
```

---

# 🔬 Technical Highlights

## 🤌 5-Tier Asset Resolution
1. Exact match  
2. Substring match  
3. Keyword match  
4. Fuzzy match  
5. Random fallback  

---

## 📍 11-Tier Location Query Engine
Handles:
- Named semantic zones  
- Player/Enemy-relative positions  
- Map bounds & corners  
- LLM-based coordinate generation  
- Iterative + random fallback  

---

## 🧠 Scene Plan Data Types
- `FLightingPlan`  
- `FEnvironmentPlan`  
- `FPropsModification`  
- `FSpawnRequest`  
- `FEnhancedScenePlan`  

Executed by **USceneBuilder** into:
- Lighting
- Fog/Weather
- Post-process
- Prop modification
- Actor spawning

---

# 📦 Installation & Setup

### **1. Requirements**
- Unreal Engine 5.2+
- Visual Studio 2022
- Groq API or Local LLM (Ollama)

---

### **2. Clone**
```bash
git clone https://github.com/KAAZamapiloT/Project-CANVAS.git
```

---

### **3. Add API Key**
Create: `Source/Project_CANVAS/API_KEY.h`
```cpp
#pragma once
class API_KEY {
public:
    FString GetGroqKey() { return TEXT("YOUR_GROQ_API_KEY"); }
};
```

---

### **4. Asset Database Structure**
```
Content/
  └── DATABASE/
       ├── meshes/
       ├── textures/
       ├── particles/
       └── postprocess/
```
(Assets auto-index at runtime.)

---

### **5. Build**
- Generate VS Project Files  
- Build in **Development Editor**

---

# 🎮 Usage

1. Play In Editor  
2. Press **I** to open AI prompt window  
3. Try prompts:
   - “Make the world cyberpunk neon with rain.”
   - “Spawn rocks near the center.”
   - “Switch to a desert sunset theme.”
4. Watch real-time generation.

---

# 📊 Testing Summary

### Scene Generation
- Schema validation
- Asset resolution consistency
- Location fallback success
- Runtime performance

### Combat System
- Rule validation
- Combo integrity
- Tag/cooldown handling
- Edge-case stability

---

# 👤 Author

**Uday Singh**  
IIIT Vadodara  
Email: 202351150@iiitvadodara.ac.in

