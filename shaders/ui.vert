#version 330 core

// Posicao ja em pixels de tela, com origem no canto superior esquerdo.
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec4 aColor;

out vec2 UV;
out vec4 Color;

uniform mat4 projection;

void main()
{
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
    UV = aUV;
    Color = aColor;
}
