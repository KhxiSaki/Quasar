#version 450

// Full-screen quad vertices for lighting pass
layout(location = 0) out vec2 outTexCoord;

void main() {
    // Generate full-screen quad from vertex index
    // Vertex positions: (-1, -1), (3, -1), (-1, 3) covering the screen
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2(3.0, -1.0),
        vec2(-1.0, 3.0)
    );

    // Texture coordinates for Vulkan (Y is down in image space)
    // This maps the full-screen triangle to UV space [0, 1]
    vec2 texCoords[3] = vec2[](
        vec2(0.0, 0.0),   // Bottom-left
        vec2(2.0, 0.0),   // Bottom-right
        vec2(0.0, 2.0)    // Top-left
    );

    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    outTexCoord = texCoords[gl_VertexIndex];
}
