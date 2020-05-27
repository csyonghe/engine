#ifndef GAME_ENGINE_LIMITS_H
#define GAME_ENGINE_LIMITS_H

namespace GameEngine
{
    const int MaxLightsPerTile = 128;
    const int LightTileSize = 16;
	const int MaxWorldRenderPasses = 8;
	const int MaxPostRenderPasses = 32;
	const int MaxShadowCascades = 8;
	const int MaxEnvMapCount = 128;
	const int EnvMapSize = 64;
	const int DynamicBufferLengthMultiplier = 2; // double buffering for dynamic uniforms
	const int MaxModuleInstances = 1<<20;
    const int MaxBlendShapes = 32;
    }

#endif