# **Hydrogen Quantum Orbital Visualizer**

Here is the raw code for the atom simulation, includes raytracer version, realtime runner, and 2D version

web version: [kavang.com/atom](https://www.kavang.com/atom)

What the model does:
1. Takes the quantum numbers (n, l, m) that describe an orbital's shape
2. Using the schrodinger equation, sample r, theta, and phi coordinates from those quantum numbers
3. Render those possible positions and color code them relative to their probabilities (brighter areas have higher probability)

## **Building Requirements**

- **macOS only** — uses Apple Metal for GPU rendering
- C++17 or newer
- [CMake](https://cmake.org/)
- GLFW3, GLM (metal-cpp is fetched automatically)

## **Build Instructions**

### macOS (Homebrew)

```bash
brew install cmake glfw glm
cmake -B build -S .
cmake --build build
```

### Executables

After building, the following executables are in the `build` folder:

| Executable      | Description                                                       |
|-----------------|-------------------------------------------------------------------|
| `atom`          | 2D Bohr model                                                     |
| `atom_raytracer`| GPU ray-traced 3D visualization (Metal)                           |
| `atom_realtime` | Real-time 3D particle visualization (Metal, instanced rendering) |
| `wave_atom_2d`  | 2D wave function visualization                                    |
| `metal_test`    | Minimal Metal window test                                         |

## **How the code works**

Source files: `atom.cpp` (2D Bohr model), `atom_raytracer.cpp` (ray-traced 3D), `atom_realtime.cpp` (real-time 3D), `wave_atom_2d.cpp` (2D wave).

**Performance note:** Run the realtime model with <100k particles first to verify stability. The raytracer is compute-intensive—ensure your system can handle it.
