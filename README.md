# 🎨 **PROJECT CANVAS**

## **Real-Time Game World Manipulation with LLM-Driven Scene Generation + Context-Aware Combat AI**

<p align="center">
  <img src="https://img.shields.io/badge/Engine-Unreal_Engine_5.2+-black?logo=unrealengine" />
  <img src="https://img.shields.io/badge/Language-C++-blue?logo=cplusplus" />
  <img src="https://img.shields.io/badge/AI-Generative_LLM-green?logo=groq" />
  <img src="https://img.shields.io/badge/Status-Prototype-orange" />
</p>


## 🎥 Demo

[![Watch Demo](https://img.shields.io/badge/Watch-Demo-red?logo=youtube)]([YOUR_YOUTUBE_LINK_HERE](https://youtu.be/33D2Y9a3EZM?si=KEKinwYB2p0a-5yy))

---

<p align="center"><strong>🧐 A Dual-Pipeline Architecture for Real-Time Scene Generation & Context-Aware Combat Intelligence.</strong></p>

---

# 🚧 Core Problem

> **How can an AI agent construct a coherent, high-quality 3D scene using thousands of messy, inconsistently named assets — while working under strict context limits and no built-in spatial understanding?**

Game scenes require structure, coherence, optimization, and style consistency. Raw LLM prompts can't solve this alone.

Project CANVAS is an attempt in solving this

---

# 🚀 Overview

Project CANVAS merges **Generative AI** + **Gameplay AI** inside Unreal Engine 5.

## 🎨 1. Scene Generation Pipeline

Players type prompts like:

> “Turn this into a neon cyberpunk night with rain and pink fog.”

The system:

* Produces a strict **JSON scene plan**
* Resolves real assets via a multi-tier resolver
* Places them using UE5 systems
* Updates the world **in real time**

## ⚔️ 2. Combat Decision Pipeline

A fully modular combat AI that chooses optimal moves using:

* State vectors
* Gameplay tags
* Cooldowns
* AI intent
* Combo graph logic

Both pipelines run **simultaneously**, shaping dynamic environments and adaptive combat.

---

# ✨ Key Features

## 🎨 Generative Scene Manipulation

* Natural-language → **structured scene plan**
* 5-tier asset resolution
* 11-tier location query engine
* Dynamic lighting & fog
* Texture & prop modifications
* Unified spawning: meshes + FX

## ⚔️ Context-Aware Combat AI

* State-driven decision vectors
* Rule-based combo system
* Sub-millisecond decision time
* Emergent attack chaining

## 🧱 Modular Architecture

* `GenAISystem`
* `AssetIndexer`
* `SceneBuilder`
* `LocationQueryEngine`
* `SceneStateTracker`

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

## 📍 11-Tier Location Query Engine

Handles:

* Semantic areas
* Player-relative offsets
* Map bounds
* Deterministic UE checks
* LLM-based fallback positions

## 🧠 Scene Plan Data Types

* `FLightingPlan`
* `FEnvironmentPlan`
* `FPropsModification`
* `FSpawnRequest`
* `FEnhancedScenePlan`

Executed by **USceneBuilder** into:

* Lighting setup
* Fog & post-process
* Prop modifications
* Actor spawning

---

# 📦 Installation

## 1. Requirements

* UE 5.2+
* Visual Studio 2022
* Groq API Key (or local LLM)

## 2. Clone

```bash
git clone https://github.com/KAAZamapiloT/Project-CANVAS.git
```

## 3. Add API Key

File: `Source/Project_CANVAS/API_KEY.h`

```cpp
#pragma once
class API_KEY {
public:
    FString GetGroqKey() { return TEXT("YOUR_GROQ_API_KEY"); }
};
```

## 4. Asset Database Structure

```
Content/
  └── DATABASE/
       ├── meshes/
       ├── textures/
       ├── particles/
       └── postprocess/
```

Assets auto-index at runtime.

## 5. Build

* Generate Visual Studio project files
* Build in **Development Editor**

---

# 🎮 Usage

1. Launch PIE
2. Press **I** to open AI prompt window
3. Enter prompts like:

   * “Make the world cyberpunk neon with rain.”
   * “Spawn rocks near the center.”
   * “Switch to a desert sunset theme.”
4. Watch the scene rebuild live.

---

# 📊 Testing Summary

## Scene Generation

* Schema validation
* Asset resolution
* Location fallback
* Runtime perf checks

## Combat System

* Rule validation
* Combo integrity
* Tag/cooldown handling
* Edge-case handling

---

# 👤 Author

**Uday Singh**
IIIT Vadodara
Email: [202351150@iiitvadodara.ac.in](mailto:202351150@iiitvadodara.ac.in)

---

