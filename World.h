#ifndef WORLD_H
#define WORLD_H

#include "Chunk.h"

#include <memory>
#include <unordered_map>

struct ChunkPos
{
    int x;
    int z;

    bool operator==(const ChunkPos& other) const
    {
        return x == other.x && z == other.z;
    }
};

struct ChunkPosHash
{
    size_t operator()(const ChunkPos& p) const
    {
        //So precisa espalhar bem, nao precisa ser criptografico.
        return (size_t)(p.x * 73856093) ^ (size_t)(p.z * 19349663);
    }
};

//Divisao e resto que arredondam pra baixo, funcionando com negativo.
//Em C++, -1 / 16 da 0 e -1 % 16 da -1, o que jogaria o bloco no chunk errado
//e com coordenada local invalida. Essa e a armadilha numero 1 de voxel engine.
int floorDiv(int a, int b);
int floorMod(int a, int b);

typedef std::unordered_map<ChunkPos, std::unique_ptr<Chunk>, ChunkPosHash> ChunkMap;

class TerrainGenerator;

class World
{
public:
    //Cria uma grade de chunks com chao plano, comecando em (0,0).
    void generateFlat(int chunksX, int chunksZ, int groundHeight);

    //Mesma grade, mas com o relevo vindo do gerador de terreno.
    void generate(int chunksX, int chunksZ, const TerrainGenerator& gen);

    //Gera o que falta dentro do raio e descarta o que passou de raio + 2.
    //Gera no maximo maxPerCall chunks por chamada, pra nao travar o frame.
    //Devolve quantos gerou.
    int streamAround(ChunkPos center, int renderDistance, const TerrainGenerator& gen, int maxPerCall);

    //Em qual chunk uma posicao de mundo cai.
    static ChunkPos chunkAt(float wx, float wz);

    size_t chunkCount() const { return chunks.size(); }

    Chunk* getChunk(ChunkPos pos) const;

    //Coordenadas globais de bloco. Chunk nao carregado conta como ar.
    BlockID getBlock(int wx, int wy, int wz) const;

    //Marca o chunk dono como dirty. Se o bloco esta na borda,
    //marca o vizinho tambem, senao fica um buraco na costura.
    void setBlock(int wx, int wy, int wz, BlockID id);

    const ChunkMap& allChunks() const { return chunks; }

private:
    void markDirty(ChunkPos pos);

    ChunkMap chunks;
};

#endif
