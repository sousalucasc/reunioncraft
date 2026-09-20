#version 330 core

// Quad de tela cheia, ja em NDC.
layout (location = 0) in vec2 aPos;

out vec3 FarPoint;

// Inversa de projection * view. Desfaz a projecao pra descobrir pra onde
// cada pixel da tela esta olhando no mundo.
uniform mat4 invViewProjection;

void main()
{
    // z = 1 coloca o quad no plano distante, atras de tudo.
    gl_Position = vec4(aPos, 1.0, 1.0);

    vec4 p = invViewProjection * vec4(aPos, 1.0, 1.0);
    FarPoint = p.xyz / p.w;
}
