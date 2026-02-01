# vvhiskers

C++23 ECS-heavy voxel engine.

## TODO most of this is AI generated slop i'll sit down and write documentation soon

## Quick Start

```bash
./v build              # Build all
./v targets            # List available targets
./v run [target]       # Run target (fuzzy searched)
./v test               # Run tests
```

## Architecture

- **Engine**: The ECS orchestrator, also an entity itself. This lets us request components from systems that are owned by the Engine.  
- **Contexts**: Singleton services (RenderContext, WindowContext, etc.)
- **Domains**: Can be a system, a component, or an entity, or some combination of the three, depending on the usage and your design choices.

Design heavily inspired by my GOAT [John Lin](https://voxely.net/blog/object-oriented-entity-component-system-design/)

## Ok so, where engine?

The engine provides the following default contexts:  

TODO add links here  
Async  - Provides threading and coroutines  
Net    - Provides network communication via UDP
Render - Provides everything rendering related
Window - Provides windows and per-window input
SDL    - Provides access to low level SDL events

## More docs

### [Getting Started](guides/getting-started.md)
- Build system setup and configuration
- Running your first application
- Project structure overview

### [Architecture](architecture/)
- [Overview](architecture/overview.md) - System design
- [Patterns](architecture/patterns.md) - Core patterns

### [Rendering System](rendering/)
- [Render Domains](rendering/render-domains.md) - GPU rendering system using Daxa task graph

### [Guides](guides/)
- [Getting Started](guides/getting-started.md) - Setup and first app
- [Creating Domains](guides/creating-domains.md) - Custom game systems

### [Reference](reference/)
- [Build System](reference/build-system.md) - v.py commands
