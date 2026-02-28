#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 inWorldPosition;
layout(location = 1) in vec3 inWorldNormal;
layout(location = 2) in vec3 inAlbedo;
layout(location = 3) in vec2 inTexCoord;

// G-Buffer outputs
layout(location = 0) out vec4 outPosition;    // World position (xyz)
layout(location = 1) out vec4 outNormal;      // World normal (xyz) + unused (w)
layout(location = 2) out vec4 outAlbedo;      // Albedo (rgb) + unused (w)
layout(location = 3) out vec4 outPBR;         // roughness, metallic, ao, unused

void main() {
    // Sample texture for albedo
    vec4 texColor = texture(texSampler, inTexCoord);
    vec3 finalAlbedo = inAlbedo * texColor.rgb;
    
    // Output world position
    outPosition = vec4(inWorldPosition, 1.0);
    
    // Output world normal (normalized)
    outNormal = vec4(normalize(inWorldNormal), 1.0);
    
    // Output albedo color
    outAlbedo = vec4(finalAlbedo, 1.0);
    
    // Output PBR data: roughness, metallic, ambient occlusion
    // For now, using default values - can be extended with more texture inputs
    outPBR = vec4(0.5, 0.5, 1.0, 1.0);  // roughness=0.5, metallic=0.5, ao=1.0
}
