# AtomMetal — Hydrogen Quantum Orbital Visualizer (Native Metal)

A macOS-native real-time 3D visualization of hydrogen atom quantum orbitals,
built with Apple Metal, Cocoa, and Core Text. No OpenGL, no GLFW, no GLM.

**[Demo video](https://www.youtube.com/watch?v=iZVhUinZ9FU)**

![Quantum orbital visualization (5f, n=5 l=3 m=1)](assets/Screenshot%202026-02-21%20at%2010.04.58 PM.png)

## What it visualizes

Electron probability distributions derived from the time-independent Schrödinger
equation for hydrogen. Particles are sampled from |ψ_nlm(r, θ, φ)|² using
pre-computed cumulative distribution functions per quantum state.

## Requirements

- macOS 13.0 or later
- CMake 3.21+
- Xcode / Apple Command Line Tools

No third-party libraries. All dependencies are system frameworks: Metal, MetalKit,
Cocoa, CoreGraphics, CoreText.

## Build

```bash
cmake -G "Unix Makefiles" -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/AtomMetal.app/Contents/MacOS/AtomMetal
```

## Controls

| Key     | Action                        |
|---------|-------------------------------|
| W / S   | Increase / decrease shell (n)  |
| E / D   | Increase / decrease subshell (ℓ) |
| R / F   | Increase / decrease orientation (m) |
| T / G   | +/- 100,000 particles         |
| C       | Toggle color mode (fire/inferno) |
| V       | Toggle realtime / raytraced   |
| X       | Toggle cutaway view           |
| Space / P | Pause / resume animation (P works with Full Keyboard Access) |
| Drag    | Orbit camera                  |
| Scroll  | Zoom                          |

The HUD overlay shows the current orbital, quantum numbers (shell n, subshell ℓ,
orientation m), energy, particle count, and render mode, with key hints for all
controls.

## Quantum numbers

| n | l | m  | Orbital | Shape            |
|---|---|----|---------|------------------|
| 1 | 0 | 0  | 1s      | Sphere           |
| 2 | 0 | 0  | 2s      | Sphere + node    |
| 2 | 1 | 0  | 2pz     | Dumbbell (z)     |
| 2 | 1 | ±1 | 2px/y   | Dumbbell (x/y)   |
| 3 | 2 | 0  | 3dz²    | 4-lobe (z)       |
| 3 | 2 | ±2 | 3dxy    | 4-lobe flat      |

Starts at **n=2, l=1, m=0** (2pz, dumbbell shape).

## Render modes

- **Realtime** (default): MTLPrimitiveTypePoint with sphere impostor in the
  fragment shader. 250,000 particles at 60fps on M1+.
- **Raytraced** (V): Metal compute kernel dispatched per pixel. Each thread
  traces a ray and finds the closest sphere hit with shadow rays. Suited for
  ≤100,000 particles.

## Physics

- Probability density: |R_nl(r)|² × |P_l^|m|(cos θ)|²
- Radial part R_nl(r): associated Laguerre polynomial via 3-term recurrence
- Angular part: associated Legendre polynomial P_l^|m| (always uses |m|)
- CDF sampling: 4096 radial bins × 2048 theta bins per (n, l, m) state, cached
  in an unordered_map (rebuilt only on quantum number change)
- Probability current animation: v_φ = ℏm / (mₑ r sin θ), advances φ per frame
  at dt = 0.5 while holding r and θ constant

## Architecture

```
Source/
  main.mm               NSApplication entry point
  AppDelegate.mm        Window creation
  MetalView.mm          MTKView: render loop, camera, keyboard
  Renderer.mm           Metal pipelines, buffers, draw calls
  SimdMath.h            lookAt, perspective, inverse4x4 (no GLM)
  Physics/
    WaveFunction.cpp    Radial/Legendre/probability density
    CDFSampler.cpp      Inverse-CDF sampling, (n,l,m) cache map
    ProbabilityCurrent.cpp  v_phi velocity field + particle update
  Scene/
    ParticleSystem.mm   Particle generation + animation
    PointCharge.mm      Coulomb potential
    Grid.mm             XZ reference grid
  HUD/
    HUDRenderer.mm      Core Text -> MTLTexture overlay
Shaders/
  ShaderTypes.h         Shared CPU/GPU structs
  OrbitalShaders.metal  Point cloud + sphere impostor
  ChargeShaders.metal   Instanced charge spheres
  GridShaders.metal     XZ grid lines
  RaytracerShaders.metal  Compute kernel + blit
  HUDShaders.metal      HUD quad compositing
CMakeLists.txt
Info.plist
```

Shaders are compiled at runtime from `.metal` source files located in
`AtomMetal.app/Contents/Resources/Shaders/` (copied there by CMake post-build).
