#version 330 core

in vec3 ourColor;
in vec2 TexCoord;
in vec2 TileBase;
in float FogDist;

out vec4 FragColor;

uniform sampler2D blockTexture;

// Fog. A cor acompanha o ceu, pra que o terreno distante se dissolva nele
// em vez de terminar num corte reto no limite do render distance.
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;

const float TILE_STEP = 1.0 / 16.0;

void main()
{
    // Com greedy meshing um quad cobre varios blocos, entao a TexCoord vai
    // de 0 ate a largura do quad. O fract traz de volta pra dentro de um
    // tile, repetindo a textura em vez de esticar uma copia so.
    // Funciona porque o atlas usa GL_NEAREST e nao tem mipmap: nao ha
    // filtragem cruzando a emenda.
    vec2 uv = TileBase + fract(TexCoord) * TILE_STEP;

    vec4 texel = texture(blockTexture, uv);

    // Folha tem buracos transparentes na textura. Sem isso eles virariam
    // pixels pretos. discard descarta o fragmento sem precisar de blending,
    // que e mais barato e nao depende de ordem de desenho.
    if (texel.a < 0.5)
        discard;

    // ourColor ja traz tint, luz por face, ambient occlusion e a luz do dia.
    vec4 cor = texel * vec4(ourColor, 1.0);

    float fog = smoothstep(fogStart, fogEnd, FogDist);
    FragColor = vec4(mix(cor.rgb, fogColor, fog), cor.a);
}
