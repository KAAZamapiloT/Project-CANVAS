```markdown
# 🎨 **PROJECT CANVAS**  
## **Real-Time Game World Manipulation with LLM-Driven Scene Generation + Context-Aware Combat AI**

<div align="center">

![UE5](https://img.shields.io/badge/Engine-Unreal_Engine_5.2+-black?logo=unrealengine)
![C++](https://img.shields.io/badge/Language-C%2B%2B17-blue?logo=cplusplus)
![AI](https://img.shields.io/badge/AI-Generative_LLM-green?logo=openai)
![Status](https://img.shields.io/badge/Status-Prototype-orange)
![License](https://img.shields.io/badge/License-MIT-purple)

</div>

---

<div align="center">

### **🧠 “A Dual-Pipeline Architecture for Real-Time Scene Generation & Combat Intelligence.”**

</div>

---

# 🚀 Overview

**Project CANVAS** fuses **Generative AI** with **real-time gameplay systems** in Unreal Engine 5:

### **1. Scene Generation Pipeline (LLM → JSON → In-Game Scene)**  
Players type prompts like:  
> _“Turn this into a neon cyberpunk night with rain and pink fog.”_

The system generates a strict JSON scene plan, resolves assets, finds valid spawn positions, and **constructs a 3D environment in real time**.

### **2. Context-Aware Combat Pipeline (State → Context Vector → Decision Engine)**  
A data-driven AI evaluates:
- distance  
- tags  
- cooldowns  
- orientation  
- player intent  

…to pick the **optimal combat move**, producing emergent combo chains without hardcoded sequences.

---

# ✨ Key Features

## 🎨 **Generative AI Scene Manipulation**
- Natural language → **JSON Scene Plan**
- 5-tier mesh resolution (exact → substring → keyword → fuzzy → fallback)
- 11-tier location resolution (player-relative, enemy-relative, corners, LLM-assist, etc.)
- Dynamic lighting, fog, textures, props, and particle FX
- Unified spawn system for meshes + Niagara

---

## ⚔️ **Context-Aware Combat AI**
- Context Vector captures entire combat state  
- Decision Engine outputs an **Action Command**  
- Data-driven combo graph (emergent, not pre-scripted)  
- Millisecond-level rule-based reasoning  

---

## 🧱 **Modular Architecture**
- GenAISystem (LLM integration)  
- AssetIndexer (automatic content scanning)  
- LocationQueryEngine (semantic → world-space)  
- SceneBuilder (environment execution)  
- SceneStateTracker (orchestration + history)  

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
Action Command                   JSON Parser → Enhanced Scene Plan
│                                      │
▼                                      ▼
CHARACTER SYSTEMS            Scene Builder → Lighting / Props / FX
│
Scene State Tracker (Orchestrator)
│
▼
GAME OUTPUT

````

---

# 🔬 Technical Highlights

## 🧩 5-Tier Asset Resolution
1. Exact match  
2. Substring match  
3. Keyword match  
4. Fuzzy match  
5. Random fallback  

—

## 📌 11-Tier Location Query Engine
Handles:
- Named semantic zones  
- Player/Enemy-relative positions  
- Map bounds & corners  
- LLM-based coordinate generation  
- Iterative + random fallback  

—

## 🧠 Scene Plan Data Types
- `FLightingPlan`
- `FEnvironmentPlan`
- `FPropsModification`
- `FSpawnRequest`
- `FEnhancedScenePlan`

Converted by **USceneBuilder** into:
- Lighting changes  
- Post-process  
- Fog/weather  
- Prop modification  
- Actor spawning  

---

# 📦 Installation & Setup

### **1. Requirements**
- Unreal Engine 5.2+  
- Visual Studio 2022  
- Groq API key or local Ollama  

---

### **2. Clone Project**
```bash
git clone https://github.com/KAAZamapiloT/Project-CANVAS.git
````

---

### **3. Add API Key**

Create `API_KEY.h` in `Source/Project_CANVAS/`:

```cpp
#pragma once
class API_KEY {
public:
    FString GetGroqKey() { return TEXT("YOUR_GROQ_API_KEY"); }
};
```

---

### **4. Setup Asset Database**

Create this content structure:

```
Content/
 └── DATABASE/
      ├── meshes/
      ├── textures/
      ├── particles/
      └── postprocess/
```

Just drop your assets in — they are auto-indexed.

---

### **5. Build**

* Right-click `.uproject` → "Generate Visual Studio Project Files"
* Build in **Development Editor**

---

# 🎮 Usage

1. Play In Editor
2. Press **I** to open the AI prompt widget
3. Type prompts like:

   * “Make it cyberpunk neon with rainfall.”
   * “Spawn three rocks behind the player.”
   * “Turn the map into a desert sunset.”
4. Watch the full pipeline execute live.

---

# 🧪 Testing Summary

### **Scene Generation Tests**

* JSON schema validation
* Asset resolution
* Location fallback handling
* Runtime performance

### **Combat AI Tests**

* Move legality
* Tag + cooldown logic
* Combo chain sequencing
* Edge-case handling

---





# 👤 Author

**Uday Singh**
IIIT Vadodara
Email: [202351150@iiitvadodara.ac.in](mailto:202351150@iiitvadodara.ac.in)





```
```
