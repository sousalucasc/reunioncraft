#version 330 core

// Mesmo vertice compactado do basic.vert, mas aqui so interessa a posicao e
// o suficiente pra decidir se o fragmento existe. Nada de AO, tint ou fog:
// o passe grava profundidade e mais nada.
layout (location = 0) in uvec2 aData;

out vec2 TexCoord;
out vec2 TileBase;

uniform mat4 lightSpace;
uniform vec3 chunkOrigin;

const float TILE_STEP = 1.0 / 16.0;

void main()
{
    uint x = aData.x & 31u;
    uint y = (aData.x >> 5u) & 511u;
    uint z = (aData.x >> 14u) & 31u;

    uint tile = aData.y & 255u;
    float ru = float((aData.y >> 8u) & 31u);
    float rv = float((aData.y >> 13u) & 31u);

    TileBase = vec2(float(tile % 16u) * TILE_STEP,
                    1.0 - float(tile / 16u) * TILE_STEP - TILE_STEP);
    TexCoord = vec2(ru, rv);

    gl_Position = lightSpace * vec4(chunkOrigin + vec3(float(x), float(y), float(z)), 1.0);
}
