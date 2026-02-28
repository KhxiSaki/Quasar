#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 viewPos;
    vec3 lightPos;
    vec3 lightColor;
    float lightRadius;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 outWorldPosition;
layout(location = 1) out vec3 outWorldNormal;
layout(location = 2) out vec3 outAlbedo;
layout(location = 3) out vec2 outTexCoord;

void main() {
    // Calculate world position
    vec4 worldPosition = ubo.model * vec4(inPosition, 1.0);
    outWorldPosition = worldPosition.xyz;
    
    // Transform normal to world space
    mat3 normalMatrix = transpose(inverse(mat3(ubo.model)));
    outWorldNormal = normalize(normalMatrix * inColor); // Using color channel as normal for now
    
    // Albedo (using vertex color as base color)
    outAlbedo = inColor;
    
    // Pass texture coordinates
    outTexCoord = inTexCoord;
    
    gl_Position = ubo.proj * ubo.view * worldPosition;
}
