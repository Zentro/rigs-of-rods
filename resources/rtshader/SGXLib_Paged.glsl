//-----------------------------------------------------------------------------
// Program Name: SGXLib_Paged
// Program Desc: PagedGeometry (trees, grass) effects for RTSS.
// Program Type: Vertex/Pixel shader
// Language: GLSL / HLSL (unified shader syntax)
//
// 1:1 ports of the vertex shaders PagedGeometry used to generate itself (BatchPage.cpp,
// GrassLoader.cpp), so that RTSS can add lighting, shadows and fog around them.
// All positions and the camera position are in object space.
//-----------------------------------------------------------------------------

// --- Trees (BatchPage) -------------------------------------------------------

// Fade out between the visible and invisible distance (horizontal distance only).
void SGX_Paged_TreeFade(in vec4 position, in vec3 camPos, in float invisibleDist, in float fadeGap, out float fade)
{
	float dist = distance(camPos.xz, position.xz);
	fade = (invisibleDist - dist) / fadeGap;
}

// --- Grass (GrassLoader) -----------------------------------------------------

void SGX_Paged_ToObjectSpace(in mat4 invWorld, in vec3 worldPos, out vec3 objPos)
{
	objPos = mul(invWorld, vec4(worldPos, 1.0)).xyz;
}

void SGX_Paged_GrassDistance(in vec4 position, in vec3 camPos, out float dist)
{
	dist = distance(camPos.xz, position.xz);
}

// FADETECH_ALPHA / FADETECH_ALPHAGROW: fully visible up to half of the range, then fades out.
void SGX_Paged_GrassFadeAlpha(in float dist, in float fadeRange, out float alpha)
{
	alpha = 2.0 - (2.0 * dist / fadeRange);
}

// GRASSTECH_SPRITE: the quad's corners are packed in the normal (x = sideways, y = up); turn them to face the camera.
void SGX_Paged_GrassSprite(inout vec4 position, in vec3 camPos, in vec3 corner)
{
	vec3 dirVec = position.xyz - camPos;
	vec3 p = normalize(cross(vec3(0.0, 1.0, 0.0), dirVec));
	position += vec4(p.x * corner.x, corner.y, p.z * corner.x, 0.0);
}

// Wave the top vertices (v == 0) in the breeze.
void SGX_Paged_GrassWave(inout vec4 position, in vec4 origPosition, in vec2 uv, in float time, in float frequency, in vec4 direction)
{
	if (uv.y == 0.0)
	{
		position += direction * sin(time + origPosition.x * frequency);
	}
}

// Blend the base (bottom vertices) of nearby grass into the terrain.
void SGX_Paged_GrassBlendBase(inout float alpha, in vec2 uv, in float dist, in float fadeRange)
{
	if (uv.y != 0.0)
	{
		alpha = clamp(alpha, 0.0, 1.0) * 4.0 * ((dist / fadeRange) - 0.1);
	}
}

// FADETECH_GROW / FADETECH_ALPHAGROW: sink distant grass into the ground.
void SGX_Paged_GrassGrow(inout vec4 position, in float dist, in float fadeRange, in float grassHeight)
{
	position.y -= grassHeight * clamp((2.0 * dist / fadeRange) - 1.0, 0.0, 1.0);
}

// --- Both --------------------------------------------------------------------

// Applies the fade to the final colour - before the alpha test, like the vertex alpha did before.
void SGX_Paged_ApplyFade(in float fade, inout vec4 colour)
{
	colour.a *= clamp(fade, 0.0, 1.0);
}
