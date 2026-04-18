# vkrt / vklib / dx12lib Architecture Analysis (Draft)

## Background and Goals

Current plan:

- Organize Vulkan-related code into `vklib`
- Add `dx12lib` in the future
- Use `vkrt` as the application entry point, and incrementally implement:
    1. Load PBRT scenes
    2. Preview scenes with a graphics API
    3. Run CPU ray tracing
    4. Add GPU ray tracing later

Core objective: this is a study project, so prioritize progress speed and clear structure.  
**The key is not building a full DX/Vulkan common abstraction first, but separating scene management from graphics API rendering.**

---

## Layering (Revised)

### 1) Application Layer (Entry)
- **Module**: `vkrt`
- **Responsibilities**: app lifecycle, argument parsing, mode switching (Preview / CPU RT / GPU RT), flow orchestration
- **Constraint**: should not contain low-level graphics API implementation details

### 2) Scene & Algorithm Layer (API-independent)
- **Modules**: `scene-core`, `scene-io-pbrt`, `cpu-rt`
- **Responsibilities**:
    - Unified scene data model (`Scene`, `Mesh`, `Material`, `Camera`, `Light`)
    - PBRT parsing into the unified scene model
    - CPU ray tracing (BVH, integrator, output image)
- **Constraint**: must not depend on `vklib` or `dx12lib`

### 3) Rendering Backend Layer (API-specific)
- **Modules**: `vklib` (Vulkan), `dx12lib` (future DX12)
- **Responsibilities**: device creation, resource upload, draw, swapchain, (future) GPU RT
- **Constraint**: backends are implemented independently; no forced full common interface at this stage

---

## Interface Strategy for Current Stage (Revised)

> Principle: define a clean data boundary first, then abstract only if needed.

Current minimum requirement:

- Scene side outputs unified render input data (e.g., `SceneSnapshot` / `RenderInput`)
- Vulkan/DX12 backends consume the same input data and render
- Do **not** pre-design a large universal renderer interface

Notes:
- Small shared data structures are acceptable (for example, resource descriptors)
- Avoid abstraction-for-abstraction; do not block learning progress

---

## `scene-core` Data Model (Single Source of Truth)

Recommended core structures:

- `Scene`
- `Node / Transform / Instance`
- `Mesh / Submesh / VertexStreams / Indices`
- `MaterialPBR` (`baseColor / metallic / roughness / normal / emissive / alpha`)
- `Texture`
- `Camera`
- `Light`

Requirement: PBRT output must first be written into `scene-core`, then converted into API resources by each backend.    

---

> Note: a unified `render-backend` interface layer is not a mandatory early milestone. Add it later only if duplication justifies it.

---

## Milestones (Revised)

### M1
- Create `scene-core`
- Complete PBRT -> `scene-core::Scene`
- Keep Vulkan preview running (static meshes)

### M2
- Get `cpu-rt` working (BVH + basic materials + multithreading)
- Display CPU RT result in the app window

### M3
- Fully separate scene-management logic from Vulkan rendering logic
- Stabilize bridge data (`SceneSnapshot` / `RenderInput`)
- Clean up resource lifetime and error handling

### M4
- Integrate Vulkan GPU RT (main path first)
- Keep data semantics aligned with `scene-core`

### M5
- Integrate a minimal `dx12lib` render path
- Reuse the same scene management and import pipeline
- Extract only high-value shared parts if needed

---

## Risks and Mitigations (Revised)

1. **Risk: early over-abstraction slows development**
    - Mitigation: focus on scene/render boundary first, not full interface unification

2. **Risk: PBRT parser becomes coupled to backend resources**
    - Mitigation: enforce `scene-core` as mandatory intermediate representation

3. **Risk: mismatch between CPU RT and GPU preview material semantics**
    - Mitigation: define `MaterialPBR` as the single source of truth

4. **Risk: duplicated code grows across backends**
    - Mitigation: evaluate and extract common code incrementally in M5

---

## Recommended Execution Strategy (Revised)

1. First do structure and dependency boundary cleanup with no behavior changes
2. Extract `scene-core` and migrate PBRT output to it
3. Stabilize Vulkan path for Preview / CPU RT display / GPU RT main flow
4. After DX12 integration, extract only the actually duplicated high-value common parts

This order keeps the project runnable while improving architecture incrementally.
