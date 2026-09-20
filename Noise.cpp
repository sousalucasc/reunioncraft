#include "Noise.h"

#include <cmath>

//Curva 6t^5 - 15t^4 + 10t^3. Vale 0 em 0 e 1 em 1, com derivada zero nas pontas,
//que e o que evita a quina visivel na fronteira de cada celula.
static float fade(float t)
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static float lerp(float a, float b, float t)
{
    return a + t * (b - a);
}

//Produto escalar entre um gradiente sorteado e a distancia ate o canto.
//8 direcoes bastam em 2D.
static float grad(int hash, float x, float y)
{
    switch (hash & 7)
    {
    case 0:  return  x + y;
    case 1:  return -x + y;
    case 2:  return  x - y;
    case 3:  return -x - y;
    case 4:  return  x;
    case 5:  return -x;
    case 6:  return  y;
    default: return -y;
    }
}

//Mesma ideia do grad 2D, com 12 direcoes cobrindo as arestas de um cubo.
static float grad3(int hash, float x, float y, float z)
{
    switch (hash & 15)
    {
    case 0:  case 12: return  x + y;
    case 1:  case 13: return -x + y;
    case 2:           return  x - y;
    case 3:           return -x - y;
    case 4:           return  x + z;
    case 5:           return -x + z;
    case 6:           return  x - z;
    case 7:           return -x - z;
    case 8:           return  y + z;
    case 9:  case 14: return -y + z;
    case 10:          return  y - z;
    default:          return -y - z;
    }
}

Noise::Noise(unsigned int seed)
{
    for (int i = 0; i < 256; i++)
        perm[i] = i;

    //Fisher-Yates com um LCG simples, so pra embaralhar de forma reproduzivel.
    unsigned int state = seed;
    for (int i = 255; i > 0; i--)
    {
        state = state * 1664525u + 1013904223u;
        int j = (int)(state % (unsigned int)(i + 1));

        int tmp = perm[i];
        perm[i] = perm[j];
        perm[j] = tmp;
    }

    //Duplica pra que perm[x] + y nunca estoure o array.
    for (int i = 0; i < 256; i++)
        perm[256 + i] = perm[i];
}

float Noise::perlin(float x, float y) const
{
    float fx = std::floor(x);
    float fy = std::floor(y);

    //Canto da celula.
    int xi = (int)fx & 255;
    int yi = (int)fy & 255;

    //Posicao dentro da celula.
    float xf = x - fx;
    float yf = y - fy;

    float u = fade(xf);
    float v = fade(yf);

    int aa = perm[perm[xi] + yi];
    int ab = perm[perm[xi] + yi + 1];
    int ba = perm[perm[xi + 1] + yi];
    int bb = perm[perm[xi + 1] + yi + 1];

    //Interpola os 4 cantos da celula.
    float x1 = lerp(grad(aa, xf, yf), grad(ba, xf - 1.0f, yf), u);
    float x2 = lerp(grad(ab, xf, yf - 1.0f), grad(bb, xf - 1.0f, yf - 1.0f), u);

    return lerp(x1, x2, v);
}

float Noise::perlin3(float x, float y, float z) const
{
    float fx = std::floor(x);
    float fy = std::floor(y);
    float fz = std::floor(z);

    int xi = (int)fx & 255;
    int yi = (int)fy & 255;
    int zi = (int)fz & 255;

    float xf = x - fx;
    float yf = y - fy;
    float zf = z - fz;

    float u = fade(xf);
    float v = fade(yf);
    float w = fade(zf);

    int a = perm[xi] + yi;
    int aa = perm[a] + zi;
    int ab = perm[a + 1] + zi;
    int b = perm[xi + 1] + yi;
    int ba = perm[b] + zi;
    int bb = perm[b + 1] + zi;

    //Interpola os 8 cantos do cubo: primeiro em x, depois em y, depois em z.
    float x1 = lerp(grad3(perm[aa], xf, yf, zf), grad3(perm[ba], xf - 1.0f, yf, zf), u);
    float x2 = lerp(grad3(perm[ab], xf, yf - 1.0f, zf), grad3(perm[bb], xf - 1.0f, yf - 1.0f, zf), u);
    float y1 = lerp(x1, x2, v);

    float x3 = lerp(grad3(perm[aa + 1], xf, yf, zf - 1.0f), grad3(perm[ba + 1], xf - 1.0f, yf, zf - 1.0f), u);
    float x4 = lerp(grad3(perm[ab + 1], xf, yf - 1.0f, zf - 1.0f), grad3(perm[bb + 1], xf - 1.0f, yf - 1.0f, zf - 1.0f), u);
    float y2 = lerp(x3, x4, v);

    return lerp(y1, y2, w);
}

float Noise::fbm(float x, float y, int octaves, float lacunarity, float gain) const
{
    float sum = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float total = 0.0f;

    for (int i = 0; i < octaves; i++)
    {
        sum += perlin(x * frequency, y * frequency) * amplitude;
        total += amplitude;

        frequency *= lacunarity;
        amplitude *= gain;
    }

    //Divide pela soma das amplitudes pra voltar ao intervalo -1..1.
    return sum / total;
}
