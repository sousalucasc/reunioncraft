#ifndef NOISE_H
#define NOISE_H

//Perlin 2D classico. Mesma seed sempre gera o mesmo terreno.
class Noise
{
public:
    Noise(unsigned int seed);

    //Resultado entre -1 e 1. Suave: valores proximos dao alturas proximas.
    float perlin(float x, float y) const;

    //Versao 3D, pra cavar caverna: o valor depende tambem da altura.
    float perlin3(float x, float y, float z) const;

    //Soma de octaves. Cada octave dobra a frequencia e reduz a amplitude,
    //entao a primeira faz as colinas e as seguintes so acrescentam detalhe.
    //Resultado normalizado, tambem entre -1 e 1.
    float fbm(float x, float y, int octaves, float lacunarity, float gain) const;

private:
    //Tabela de permutacao duplicada, pra nao precisar de modulo na consulta.
    int perm[512];
};

#endif
