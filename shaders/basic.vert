#version 330 core

// Vertice compactado: 2 uint32 em vez de 8 floats.
//   palavra 0:  0-4 x local | 5-13 y | 14-18 z local
//               19-21 face  | 22-23 nivel de AO | 24-25 indice do tint
//   palavra 1:  0-7 tile do atlas | 8-12 repeticao em U | 13-17 repeticao em V
layout (location = 0) in uvec2 aData;

out vec3 ourColor;
out vec2 TexCoord;
out vec2 TileBase;

// Posicao no mundo e normal da face, pro calculo de sombra no fragment.
// A normal e flat porque todo vertice de um quad de voxel tem a mesma: nao
// ha o que interpolar, e interpolar custaria.
out vec3 WorldPos;
flat out vec3 Normal;

// Profundidade ao longo do eixo da camera. E por ela que o fragment escolhe
// a cascata, e tem que ser a distancia em -Z, nao a radial, porque foi assim
// que as fatias do frustum foram cortadas no lado da CPU.
out float ViewDepth;

// Distancia ate a camera, usada pelo fog.
out float FogDist;

uniform mat4 view;
uniform mat4 projection;

// Canto do chunk no mundo. E o que permite guardar so a posicao local.
uniform vec3 chunkOrigin;

// Cores de tint, na ordem do enum TintIndex (Block.h). Enviadas uma vez.
uniform vec3 tints[3];

// Luz do ciclo dia/noite, de ~0.22 a 1.0.
uniform float dayLight;

// Luz fixa por orientacao, na ordem do enum BlockFace.
const float FACE_LIGHT[6] = float[6](0.80, 0.80, 0.60, 0.60, 1.00, 0.50);

// Normal de cada face, na ordem do enum BlockFace (Block.h).
const vec3 FACE_NORMAL[6] = vec3[6](
    vec3( 0.0,  0.0,  1.0),   // frente +Z
    vec3( 0.0,  0.0, -1.0),   // tras   -Z
    vec3(-1.0,  0.0,  0.0),   // esquerda -X
    vec3( 1.0,  0.0,  0.0),   // direita  +X
    vec3( 0.0,  1.0,  0.0),   // cima   +Y
    vec3( 0.0, -1.0,  0.0)    // baixo  -Y
);

// Quanto cada nivel de oclusao escurece. Nivel 3 e ceu aberto.
const float AO_LEVEL[4] = float[4](0.50, 0.70, 0.85, 1.00);

// Grade do atlas: 16x16 tiles.
const float TILE_STEP = 1.0 / 16.0;

void main()
{
    uint x    =  aData.x        & 31u;
    uint y    = (aData.x >> 5u) & 511u;
    uint z    = (aData.x >> 14u) & 31u;
    uint face = (aData.x >> 19u) & 7u;
    uint ao   = (aData.x >> 22u) & 3u;
    uint tint = (aData.x >> 24u) & 3u;

    uint tile = aData.y & 255u;
    // Quantos tiles o quad cobre ate este canto. Com greedy meshing um quad
    // pode cobrir varios blocos, entao isso vai de 0 ate a largura do quad.
    float ru = float((aData.y >> 8u) & 31u);
    float rv = float((aData.y >> 13u) & 31u);

    vec3 localPos = vec3(float(x), float(y), float(z));
    vec3 worldPos = chunkOrigin + localPos;
    vec4 viewPos = view * vec4(worldPos, 1.0);

    gl_Position = projection * viewPos;

    WorldPos = worldPos;
    Normal = FACE_NORMAL[face];
    ViewDepth = -viewPos.z;

    // Em espaco de camera a distancia ate a origem JA e a distancia ate o
    // observador, entao nao precisa mandar a posicao da camera como uniform.
    FogDist = length(viewPos.xyz);

    // Canto inferior esquerdo do tile no atlas. A linha e contada de cima
    // pra baixo na imagem, mas a textura carrega espelhada em V, por isso
    // o 1.0 menos.
    TileBase = vec2(float(tile % 16u) * TILE_STEP,
                    1.0 - float(tile / 16u) * TILE_STEP - TILE_STEP);

    // Vai de 0 ate a largura do quad em tiles. O fragment aplica fract pra
    // repetir a textura dentro do tile em vez de esticar uma copia so.
    TexCoord = vec2(ru, rv);

    ourColor = tints[tint] * (FACE_LIGHT[face] * AO_LEVEL[ao] * dayLight);
}
