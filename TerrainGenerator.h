#ifndef TERRAINGENERATOR_H
#define TERRAINGENERATOR_H

#include "Chunk.h"
#include "Noise.h"

//Nivel do mar: abaixo disso o vazio vira agua e a superficie vira areia.
constexpr int SEA_LEVEL = 32;

enum Biome
{
    BIOME_PLAINS,
    BIOME_DESERT,
    BIOME_SNOW
};

//Preenche um chunk a partir de um heightmap de noise, cava cavernas e
//planta arvores. Tudo deterministico: a mesma seed e a mesma coordenada
//dao sempre o mesmo bloco. E isso que permite descarregar e regerar um
//chunk sem salvar nada, e o que faz as arvores fecharem na borda.
class TerrainGenerator
{
public:
    TerrainGenerator(unsigned int seed = 1337);

    void generate(Chunk& chunk, int chunkX, int chunkZ) const;

    //Altura da superficie numa coordenada global.
    int heightAt(int wx, int wz) const;

    Biome biomeAt(int wx, int wz) const;

private:
    //true se este ponto foi escavado.
    bool isCave(int wx, int wy, int wz) const;

    //Altura do tronco se nasce arvore nessa coluna, ou 0 se nao nasce.
    int treeAt(int wx, int wz) const;

    //Escreve uma arvore cuja base esta em (wx, wz). So os blocos que caem
    //dentro deste chunk sao gravados; o resto o chunk vizinho grava sozinho.
    void placeTree(Chunk& chunk, int originX, int originZ, int wx, int wz) const;

    Noise heightNoise;
    Noise caveNoise;
    Noise biomeNoise;
};

#endif
