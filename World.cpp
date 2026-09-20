#include "World.h"
#include "TerrainGenerator.h"
#include "ChunkStorage.h"

#include <algorithm>
#include <cmath>

int floorDiv(int a, int b)
{
    int q = a / b;

    //Truncou pra cima porque os sinais diferem: corrige pra baixo.
    if ((a % b != 0) && ((a < 0) != (b < 0)))
        q--;

    return q;
}

int floorMod(int a, int b)
{
    int r = a % b;

    if (r != 0 && ((r < 0) != (b < 0)))
        r += b;

    return r;
}

void World::generateFlat(int chunksX, int chunksZ, int groundHeight)
{
    for (int cz = 0; cz < chunksZ; cz++)
    {
        for (int cx = 0; cx < chunksX; cx++)
        {
            ChunkPos pos{ cx, cz };

            std::unique_ptr<Chunk> chunk(new Chunk());
            chunk->fillFlat(groundHeight);

            chunks[pos] = std::move(chunk);
        }
    }
}

void World::generate(int chunksX, int chunksZ, const TerrainGenerator& gen)
{
    for (int cz = 0; cz < chunksZ; cz++)
    {
        for (int cx = 0; cx < chunksX; cx++)
        {
            ChunkPos pos{ cx, cz };

            std::unique_ptr<Chunk> chunk(new Chunk());
            gen.generate(*chunk, cx, cz);

            chunks[pos] = std::move(chunk);
        }
    }
}

ChunkPos World::chunkAt(float wx, float wz)
{
    int bx = (int)std::floor(wx);
    int bz = (int)std::floor(wz);

    return ChunkPos{ floorDiv(bx, CHUNK_SIZE), floorDiv(bz, CHUNK_SIZE) };
}

void World::insertChunk(ChunkPos pos, std::unique_ptr<Chunk> chunk)
{
    chunks[pos] = std::move(chunk);

    //Os vizinhos ja meshados emitiram uma parede virada pra ca, porque na
    //hora deles este chunk ainda nao existia. Sem remeshar fica um muro.
    markDirty(ChunkPos{ pos.x - 1, pos.z });
    markDirty(ChunkPos{ pos.x + 1, pos.z });
    markDirty(ChunkPos{ pos.x, pos.z - 1 });
    markDirty(ChunkPos{ pos.x, pos.z + 1 });
}

int World::unloadFar(ChunkPos center, int renderDistance)
{
    //Margem de 2 chunks alem do raio de render. Sem essa folga, andar pra
    //frente e pra tras na fronteira ficaria gerando e descartando sem parar.
    int unloadDistance = renderDistance + 2;
    int removed = 0;

    for (ChunkMap::iterator it = chunks.begin(); it != chunks.end(); )
    {
        int dx = std::abs(it->first.x - center.x);
        int dz = std::abs(it->first.z - center.z);

        if (dx > unloadDistance || dz > unloadDistance)
        {
            //Salva antes de jogar fora, senao o que o jogador construiu
            //some ao andar pra longe. Como isso roda na thread principal e
            //antes do proximo dispatchJobs, o arquivo ja esta completo se
            //algum worker for reler este mesmo chunk depois.
            if (it->second->modified)
                ChunkStorage::save(it->first, *it->second);

            it = chunks.erase(it);
            removed++;
        }
        else
        {
            ++it;
        }
    }

    return removed;
}

int World::streamAround(ChunkPos center, int renderDistance, const TerrainGenerator& gen, int maxPerCall)
{
    //Descarrega com uma margem de 2 chunks alem do raio de render.
    //Sem essa folga, andar pra frente e pra tras na fronteira ficaria
    //gerando e descartando o mesmo chunk sem parar.
    int unloadDistance = renderDistance + 2;

    for (ChunkMap::iterator it = chunks.begin(); it != chunks.end(); )
    {
        int dx = std::abs(it->first.x - center.x);
        int dz = std::abs(it->first.z - center.z);

        if (dx > unloadDistance || dz > unloadDistance)
            it = chunks.erase(it);
        else
            ++it;
    }

    //Gera do anel mais proximo pro mais distante, pra que o que esta
    //perto do jogador apareca primeiro.
    int generated = 0;

    for (int r = 0; r <= renderDistance && generated < maxPerCall; r++)
    {
        for (int dz = -r; dz <= r && generated < maxPerCall; dz++)
        {
            for (int dx = -r; dx <= r && generated < maxPerCall; dx++)
            {
                //So a casca do anel r; o miolo ja foi nas voltas anteriores.
                if (std::max(std::abs(dx), std::abs(dz)) != r)
                    continue;

                ChunkPos pos{ center.x + dx, center.z + dz };
                if (chunks.find(pos) != chunks.end())
                    continue;

                std::unique_ptr<Chunk> chunk(new Chunk());
                gen.generate(*chunk, pos.x, pos.z);
                chunks[pos] = std::move(chunk);

                //Os vizinhos ja meshados emitiram uma parede virada pra ca,
                //porque na hora deles este chunk ainda nao existia.
                //Sem remeshar, fica um muro no meio do terreno.
                markDirty(ChunkPos{ pos.x - 1, pos.z });
                markDirty(ChunkPos{ pos.x + 1, pos.z });
                markDirty(ChunkPos{ pos.x, pos.z - 1 });
                markDirty(ChunkPos{ pos.x, pos.z + 1 });

                generated++;
            }
        }
    }

    return generated;
}

int World::saveAll()
{
    int n = 0;

    for (ChunkMap::const_iterator it = chunks.begin(); it != chunks.end(); ++it)
    {
        if (it->second->modified && ChunkStorage::save(it->first, *it->second))
            n++;
    }

    return n;
}

Chunk* World::getChunk(ChunkPos pos) const
{
    ChunkMap::const_iterator it = chunks.find(pos);
    if (it == chunks.end())
        return NULL;

    return it->second.get();
}

BlockID World::getBlock(int wx, int wy, int wz) const
{
    ChunkPos pos{ floorDiv(wx, CHUNK_SIZE), floorDiv(wz, CHUNK_SIZE) };

    Chunk* chunk = getChunk(pos);
    if (chunk == NULL)
        return BLOCK_AIR;

    return chunk->getBlock(floorMod(wx, CHUNK_SIZE), wy, floorMod(wz, CHUNK_SIZE));
}

void World::markDirty(ChunkPos pos)
{
    Chunk* chunk = getChunk(pos);
    if (chunk != NULL)
        chunk->dirty = true;
}

void World::setBlock(int wx, int wy, int wz, BlockID id)
{
    ChunkPos pos{ floorDiv(wx, CHUNK_SIZE), floorDiv(wz, CHUNK_SIZE) };

    Chunk* chunk = getChunk(pos);
    if (chunk == NULL)
        return;

    int lx = floorMod(wx, CHUNK_SIZE);
    int lz = floorMod(wz, CHUNK_SIZE);

    chunk->setBlock(lx, wy, lz, id);

    //A partir daqui este chunk deixa de ser reproduzivel pelo noise.
    chunk->modified = true;

    //A face que o vizinho desenha depende deste bloco, entao ele tambem
    //precisa ser remeshado.
    if (lx == 0)
        markDirty(ChunkPos{ pos.x - 1, pos.z });
    if (lx == CHUNK_SIZE - 1)
        markDirty(ChunkPos{ pos.x + 1, pos.z });
    if (lz == 0)
        markDirty(ChunkPos{ pos.x, pos.z - 1 });
    if (lz == CHUNK_SIZE - 1)
        markDirty(ChunkPos{ pos.x, pos.z + 1 });
}
