#version 330 core

in vec3 ourColor;
in vec2 TexCoord;
in vec2 TileBase;
in float FogDist;

in vec3 WorldPos;
flat in vec3 Normal;
in float ViewDepth;

out vec4 FragColor;

uniform sampler2D blockTexture;

// Fog. A cor acompanha o ceu, pra que o terreno distante se dissolva nele
// em vez de terminar num corte reto no limite do render distance.
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;

// ---- sombra ----
// Array de profundidade, uma camada por cascata. O sufixo Shadow no tipo
// significa que a amostra ja vem comparada: o hardware devolve 0 ou 1 em vez
// da profundidade gravada, e com filtro LINEAR interpola DEPOIS de comparar,
// o que e um PCF 2x2 de graca.
uniform sampler2DArrayShadow shadowMaps;

uniform mat4 cascadeMatrix[3];
uniform float cascadeSplit[3];
uniform float cascadeTexel[3];

// Direcao do fragmento PRO sol.
uniform vec3 sunDir;

// Quanto a sombra chega a escurecer. Zero desliga o calculo inteiro, e e o
// que acontece de noite: sem sol nao ha o que projetar.
uniform float shadowStrength;
uniform float shadowDistance;

const float TILE_STEP = 1.0 / 16.0;

// Tem que bater com SHADOW_CASCADES e SHADOW_RES em ShadowMap.h.
const int CASCADES = 3;
const float SHADOW_RES = 1024.0;

// Raio do filtro, em texels. Cada amostra ja cobre 2x2 pelo hardware, entao
// com 4 amostras nessas diagonais o filtro efetivo pega uns 4x4 texels.
const float PCF_RADIUS = 1.0;

// 1.0 totalmente iluminado, 0.0 totalmente na sombra.
float sombraDoSol(float ndl)
{
    // Cascata pela profundidade em espaco de camera. As faixas vem ordenadas,
    // entao a primeira que couber e a certa.
    int c = CASCADES - 1;

    for (int i = 0; i < CASCADES - 1; i++)
    {
        if (ViewDepth < cascadeSplit[i])
        {
            c = i;
            break;
        }
    }

    float texel = cascadeTexel[c];

    // Normal offset: em vez de mexer na profundidade, empurra o PONTO
    // amostrado pra fora da superficie. O erro do shadow map e proporcional
    // ao tamanho do texel, e esse texel muda de cascata pra cascata, entao
    // e a unica forma de bias que escala junto sem descolar a sombra do pe
    // do objeto. O termo com slope cresce quando a luz chega rasante, que e
    // exatamente quando um texel cobre mais superficie.
    float slope = clamp(1.0 - ndl, 0.0, 1.0);
    vec3 p = WorldPos + Normal * (texel * (1.0 + 2.0 * slope));

    // Ortografica: o w sai 1, entao nao precisa da divisao perspectiva.
    vec4 luz = cascadeMatrix[c] * vec4(p, 1.0);
    vec3 proj = luz.xyz * 0.5 + 0.5;

    // Alem do plano de longe da luz nao ha nada gravado.
    if (proj.z > 1.0)
        return 1.0;

    float o = PCF_RADIUS / SHADOW_RES;
    float soma = 0.0;

    soma += texture(shadowMaps, vec4(proj.xy + vec2(-o, -o), float(c), proj.z));
    soma += texture(shadowMaps, vec4(proj.xy + vec2( o, -o), float(c), proj.z));
    soma += texture(shadowMaps, vec4(proj.xy + vec2(-o,  o), float(c), proj.z));
    soma += texture(shadowMaps, vec4(proj.xy + vec2( o,  o), float(c), proj.z));

    return soma * 0.25;
}

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

    float luz = 1.0;

    // O ramo depende so de uniform, entao a GPU inteira vai pro mesmo lado:
    // de noite o custo da sombra some de verdade, nao fica pago a toa.
    if (shadowStrength > 0.0)
    {
        float ndl = dot(Normal, sunDir);

        // Face virada pro lado oposto ao sol nao recebe luz direta nenhuma,
        // independente do que o mapa diga. Sem isso a parede norte ficaria
        // clara enquanto o chao ao lado dela esta escuro.
        float geometria = smoothstep(0.0, 0.25, ndl);

        float s = min(sombraDoSol(max(ndl, 0.0)), geometria);

        // Apaga a sombra antes do fim do alcance, senao aparece um circulo
        // nitido no chao onde a ultima cascata acaba.
        float fade = 1.0 - smoothstep(shadowDistance * 0.8, shadowDistance, ViewDepth);
        s = mix(1.0, s, fade);

        luz = 1.0 - shadowStrength * (1.0 - s);
    }

    // ourColor ja traz tint, luz por face, ambient occlusion e a luz do dia.
    vec4 cor = texel * vec4(ourColor * luz, 1.0);

    float fog = smoothstep(fogStart, fogEnd, FogDist);
    FragColor = vec4(mix(cor.rgb, fogColor, fog), cor.a);
}
