#version 460 core

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
    vec2 numTiles;
    float padding2;
    float padding3;
} ubo;

layout(binding = 1) uniform sampler2D texSampler;

struct ForwardPlusLight {
    vec3 position;
    float radius;
    vec3 color;
    float intensity;
};

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

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

vec3 calculateLight(ForwardPlusLight light, vec3 worldPos, vec3 normal, vec3 baseColor)
{
    vec3 lightDir = light.position - worldPos;
    float distance = length(lightDir);
    if (distance > light.radius) return vec3(0.0);
    
    lightDir = normalize(lightDir);
    
    float attenuation = 1.0 - (distance / light.radius);
    attenuation = attenuation * attenuation;
    
    float diff = max(dot(normal, lightDir), 0.0);
    
    vec3 ambient = baseColor * 0.05;
    vec3 diffuse = baseColor * diff * light.color * light.intensity;
    
    return (ambient + diffuse) * attenuation;
}

void main()
{
    vec4 texColor = texture(texSampler, fragTexCoord);
    vec3 baseColor = fragColor * texColor.rgb;
    vec3 normal = normalize(fragNormal);
    
    uint tileX = uint(gl_FragCoord.x) / TILE_SIZE;
    uint tileY = uint(gl_FragCoord.y) / TILE_SIZE;
    uint numTilesX = uint(ubo.numTiles.x);
    
    uint tileIndex = tileY * numTilesX + tileX;
    
    vec3 resultColor = vec3(0.0);
    
    // Add ambient
    resultColor += baseColor * 0.1;
    
    // Read light count for this tile
    uint lightCount = tileCountBuffer.tileLightCounts[tileIndex];
    lightCount = min(lightCount, MAX_LIGHTS_PER_TILE);
    
    // Loop through culled lights for this tile
    for (uint i = 0; i < lightCount; i++) {
        uint offset = tileIndex * MAX_LIGHTS_PER_TILE + i;
        uint lightIndex = tileLightIndexBuffer.tileLightIndices[offset];
        
        if (lightIndex < 256) {
            resultColor += calculateLight(lightBuffer.lights[lightIndex], fragWorldPos, normal, baseColor);
        }
    }
    
    // Just output base color (no lighting)
    resultColor = baseColor;
    
    outColor = vec4(resultColor, 1.0);
    return;
    
    // Lighting code (commented out - no real lights yet)
    /*
    // Add ambient
    resultColor += baseColor * 0.15;
    
    // Add directional light
    vec3 dir = normalize(vec3(-0.5, -1.0, -0.5));
    float d = max(dot(normal, -dir), 0.0);
    resultColor += baseColor * d * 0.6 * vec3(1.0, 0.95, 0.9);
    
    // Add point lights (use first 8 lights directly for now)
    for (uint i = 0; i < 8; i++) {
        ForwardPlusLight light = lightBuffer.lights[i];
        vec3 lightVec = light.position - fragWorldPos;
        float dist = length(lightVec);
        if (dist < light.radius && dist > 0.01) {
            vec3 L = normalize(lightVec);
            float diff = max(dot(normal, L), 0.0);
            float atten = 1.0 - (dist / light.radius);
            atten = atten * atten;
            resultColor += baseColor * diff * light.color * light.intensity * atten;
        }
    }
    
    // Tone mapping
    resultColor = resultColor / (resultColor + vec3(1.0));
    resultColor = pow(resultColor, vec3(1.0 / 2.2));
    */
}
