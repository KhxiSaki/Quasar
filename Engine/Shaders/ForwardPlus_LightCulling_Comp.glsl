// Forward+ Light Culling Compute Shader (commented out for now - no real lights yet)

#version 460 core

layout(local_size_x = 16, local_size_y = 16) in;

// Uncomment when enabling Forward+ light culling:
/*
struct ForwardPlusLight {
    vec3 position;
    float radius;
    vec3 color;
    float intensity;
};

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    float padding1;
    vec3 lightPos;
    float lightRadius;
    vec3 lightColor;
    float exposure;
} ubo;

layout(binding = 2) uniform LightBuffer {
    ForwardPlusLight lights[256];
} lightBuffer;

layout(binding = 3) buffer TileLightIndexBuffer {
    uint tileLightIndices[];
} tileLightIndexBuffer;

layout(binding = 4) buffer TileCountBuffer {
    uint tileLightCounts[];
} tileCountBuffer;

const uint TILE_SIZE = 16;
const uint MAX_LIGHTS_PER_TILE = 64;
const uint MAX_LIGHTS = 256;

void main()
{
    uint tileX = gl_GlobalInvocationID.x;
    uint tileY = gl_GlobalInvocationID.y;
    
    uint screenWidth = gl_NumWorkGroups.x * TILE_SIZE;
    uint screenHeight = gl_NumWorkGroups.y * TILE_SIZE;
    
    if (tileX >= gl_NumWorkGroups.x || tileY >= gl_NumWorkGroups.y) {
        return;
    }
    
    uint tileIndex = tileY * gl_NumWorkGroups.x + tileX;
    
    // Calculate tile frustum (simplified)
    float tileMinX = float(tileX * TILE_SIZE);
    float tileMaxX = float((tileX + 1) * TILE_SIZE);
    float tileMinY = float(tileY * TILE_SIZE);
    float tileMaxY = float((tileY + 1) * TILE_SIZE);
    
    // Calculate tile center and radius in view space (simplified)
    vec2 tileCenter = vec2((tileMinX + tileMaxX) * 0.5, (tileMinY + tileMaxY) * 0.5);
    vec2 tileSize = vec2(tileMaxX - tileMinX, tileMaxY - tileMinY);
    
    // Convert to NDC
    vec2 ndcCenter = (tileCenter / vec2(screenWidth, screenHeight)) * 2.0 - 1.0;
    ndcCenter.y = -ndcCenter.y;
    
    float radius = length(tileSize) * 0.5;
    
    uint lightCount = 0;
    
    for (uint i = 0; i < MAX_LIGHTS && lightCount < MAX_LIGHTS_PER_TILE; i++) {
        vec3 lightPos = lightBuffer.lights[i].position;
        float lightRadius = lightBuffer.lights[i].radius;
        
        // Simple distance-based culling
        vec4 lightViewPos = ubo.view * vec4(lightPos, 1.0);
        
        // Project light position to screen space
        vec4 lightProjPos = ubo.proj * lightViewPos;
        vec2 lightScreenPos = lightProjPos.xy / lightProjPos.w;
        
        float dist = length(ndcCenter - lightScreenPos);
        float lightScreenRadius = (lightRadius / -lightViewPos.z) * ubo.proj[1][1];
        
        if (dist < lightScreenRadius + radius * 0.01) {
            uint offset = tileIndex * MAX_LIGHTS_PER_TILE + lightCount;
            tileLightIndexBuffer.tileLightIndices[offset] = i;
            lightCount++;
        }
    }
    
    tileCountBuffer.tileLightCounts[tileIndex] = lightCount;
}
*/
void main()
{
    // No-op - Forward+ light culling disabled
}
