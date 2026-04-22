# DaffyChat Recipes

Recipes are the declarative way to stand up a room with predefined behavior, services, and defaults.

## Concept

A room recipe is intended to describe:

- room metadata and policy
- enabled services and extension hooks
- frontend bridge configuration
- voice-related defaults where appropriate
- saved room images that can be recreated later

The long-term design points to Daffyscript as the authoring language for these recipes.

## Current Bootstrap State

The repository already contains recipe-adjacent tooling:

- `daffyscript/` for the compiler bootstrap
- `toolchain/dfc-mkrecipe.py` for recipe-oriented helper workflows

What is still missing:

- a stable recipe schema
- room image serialization and restore flow
- recipe validation and linting
- sample recipes under version control

## Suggested Recipe Lifecycle

1. Author a Daffyscript recipe.
2. Compile it into a room artifact.
3. Attach service declarations and frontend bridge metadata.
4. Launch the room under the configured isolation provider.
5. Save the effective room state as a reusable image.

## Recommended Next Implementation Step

The next valuable recipe milestone is a minimal checked-in example that compiles, serializes room metadata, and can be referenced by tests even before full room restoration exists.
