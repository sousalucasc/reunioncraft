#version 330 core

in vec3 FarPoint;

out vec4 FragColor;

uniform vec3 cameraPos;
uniform vec3 skyZenith;
uniform vec3 skyHorizon;

void main()
{
    vec3 dir = normalize(FarPoint - cameraPos);

    // O deslocamento faz a cor do horizonte invadir um pouco o que esta
    // logo abaixo da linha, senao a emenda com o fog fica visivel.
    float t = clamp(dir.y * 1.6 + 0.12, 0.0, 1.0);

    // Curva suave pra concentrar o gradiente perto do horizonte, que e
    // onde a variacao de cor do ceu real acontece.
    t = t * t;

    FragColor = vec4(mix(skyHorizon, skyZenith, t), 1.0);
}
