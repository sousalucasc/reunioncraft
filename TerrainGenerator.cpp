#include "TerrainGenerator.h"

#include <algorithm>

//Altura media do terreno e quanto ele sobe e desce em volta dela.
//Cuidado: Perlin quase nunca chega a 1, e o fbm puxa mais pro centro ainda.
//Na pratica esta amplitude vale menos da metade do numero escrito aqui.
static const int BASE_HEIGHT = 40;
static const int AMPLITUDE = 40;

//Quanto menor, mais largas as colinas. 0.02 da um morro a cada ~50 blocos.
static const float FREQUENCY = 0.02f;

//4 octaves: a primeira faz o relevo, as outras quebram a regularidade.
static const int OCTAVES = 4;
static const float LACUNARITY = 2.0f;
static const float GAIN = 0.5f;

//Caverna: o noise 3D e esticado no eixo Y pra que os tuneis saiam mais
//horizontais do que verticais, como no Minecraft.
static const float CAVE_FREQUENCY = 0.045f;
static const float CAVE_Y_STRETCH = 2.0f;
static const float CAVE_THRESHOLD = 0.35f;

//Bioma muda devagar: frequencia bem menor que a do relevo.
static const float BIOME_FREQUENCY = 0.004f;

//Quantos blocos a copa se espalha pros lados.
static const int TREE_RADIUS = 2;
//1 coluna em 200 vira arvore, o que da 1 ou 2 por chunk.
static const unsigned int TREE_CHANCE = 200;

//Hash deterministico de uma coordenada. Duas chamadas com o mesmo (x, z)
//devolvem o mesmo numero em qualquer chunk e em qualquer execucao.
//E isso que faz a arvore da borda fechar sem os chunks conversarem.
static unsigned int hashCoord(int x, int z, unsigned int seed)
{
    unsigned int h = (unsigned int)(x * 374761393) + (unsigned int)(z * 668265263) + seed;
    h = (h ^ (h >> 13)) * 1274126177u;

    return h ^ (h >> 16);
}

TerrainGenerator::TerrainGenerator(unsigned int seed)
    : heightNoise(seed), caveNoise(seed + 977), biomeNoise(seed + 4241)
{
}

int TerrainGenerator::heightAt(int wx, int wz) const
{
    float n = heightNoise.fbm((float)wx * FREQUENCY, (float)wz * FREQUENCY, OCTAVES, LACUNARITY, GAIN);

    int height = BASE_HEIGHT + (int)(n * (float)AMPLITUDE);

    //Nunca deixa a superficie encostar no bedrock nem furar o teto.
    if (height < 1)
        height = 1;
    if (height > CHUNK_HEIGHT - 2)
        height = CHUNK_HEIGHT - 2;

    return height;
}

Biome TerrainGenerator::biomeAt(int wx, int wz) const
{
    float t = biomeNoise.perlin((float)wx * BIOME_FREQUENCY, (float)wz * BIOME_FREQUENCY);

    if (t < -0.25f)
        return BIOME_SNOW;
    if (t > 0.30f)
        return BIOME_DESERT;

    return BIOME_PLAINS;
}

bool TerrainGenerator::isCave(int wx, int wy, int wz) const
{
    float n = caveNoise.perlin3((float)wx * CAVE_FREQUENCY,
        (float)wy * CAVE_FREQUENCY * CAVE_Y_STRETCH,
        (float)wz * CAVE_FREQUENCY);

    return n > CAVE_THRESHOLD;
}

int TerrainGenerator::treeAt(int wx, int wz) const
{
    //Arvore so em planicie, e nao na praia.
    if (biomeAt(wx, wz) != BIOME_PLAINS)
        return 0;

    int height = heightAt(wx, wz);
    if (height <= SEA_LEVEL + 1)
        return 0;

    unsigned int h = hashCoord(wx, wz, 0x5EED);
    if (h % TREE_CHANCE != 0)
        return 0;

    //Tronco de 4 a 6 blocos, tambem tirado do hash.
    return 4 + (int)((h >> 8) % 3);
}

void TerrainGenerator::placeTree(Chunk& chunk, int originX, int originZ, int wx, int wz) const
{
    int trunkHeight = treeAt(wx, wz);
    if (trunkHeight == 0)
        return;

    //Base fica um bloco acima da superficie.
    int base = heightAt(wx, wz) + 1;
    int top = base + trunkHeight - 1;

    //Coordenada local. Pode ser negativa ou passar de 15: o setBlock do Chunk
    //ignora o que cai fora, e o chunk vizinho grava a parte dele.
    int lx = wx - originX;
    int lz = wz - originZ;

    //Copa: duas camadas 5x5 sem os cantos, e duas camadas 3x3 em cima.
    for (int dy = -2; dy <= 1; dy++)
    {
        int y = top + dy;
        int radius = (dy <= -1) ? 2 : 1;

        for (int dz = -radius; dz <= radius; dz++)
        {
            for (int dx = -radius; dx <= radius; dx++)
            {
                //Tira os 4 cantos da camada larga, pra copa nao ficar cubica.
                if (radius == 2 && dx * dx == 4 && dz * dz == 4)
                    continue;

                //O tronco tem prioridade sobre a folha.
                if (dx == 0 && dz == 0 && y <= top)
                    continue;

                if (chunk.getBlock(lx + dx, y, lz + dz) == BLOCK_AIR)
                    chunk.setBlock(lx + dx, y, lz + dz, BLOCK_LEAVES_OAK);
            }
        }
    }

    for (int y = base; y <= top; y++)
        chunk.setBlock(lx, y, lz, BLOCK_LOG_OAK);
}

void TerrainGenerator::generate(Chunk& chunk, int chunkX, int chunkZ) const
{
    int originX = chunkX * CHUNK_SIZE;
    int originZ = chunkZ * CHUNK_SIZE;

    //1) Relevo e camadas, ja com o bioma escolhendo a superficie.
    for (int z = 0; z < CHUNK_SIZE; z++)
    {
        for (int x = 0; x < CHUNK_SIZE; x++)
        {
            int wx = originX + x;
            int wz = originZ + z;

            int height = heightAt(wx, wz);
            Biome biome = biomeAt(wx, wz);

            //Perto do nivel do mar a superficie vira praia.
            bool beach = height <= SEA_LEVEL + 1;

            BlockID surface = BLOCK_GRASS;
            BlockID filler = BLOCK_DIRT;

            if (beach || biome == BIOME_DESERT)
            {
                surface = BLOCK_SAND;
                filler = BLOCK_SAND;
            }
            else if (biome == BIOME_SNOW)
            {
                surface = BLOCK_SNOW;
            }

            //Acima disso e so ar, e o chunk ja nasce zerado em ar.
            //Sem esse corte, 80% das iteracoes seriam escrever ar em cima de ar.
            int top = std::max(height, SEA_LEVEL);

            for (int y = 0; y <= top; y++)
            {
                BlockID id = BLOCK_AIR;

                if (y == 0)
                    id = BLOCK_BEDROCK;
                else if (y < height - 4)
                    id = BLOCK_STONE;
                else if (y < height)
                    id = filler;
                else if (y == height)
                    id = surface;
                else if (y <= SEA_LEVEL)
                    id = BLOCK_WATER;

                //2) Caverna. Nao mexe no bedrock, na agua, nem nos 2 blocos
                //   logo abaixo da superficie, pra nao esburacar o chao inteiro.
                //   Subir esse limite ate height faz aparecerem entradas.
                if (id != BLOCK_AIR && id != BLOCK_WATER && y >= 1 && y < height - 2)
                {
                    if (isCave(wx, y, wz))
                        id = BLOCK_AIR;
                }

                chunk.setBlock(x, y, z, id);
            }
        }
    }

    //3) Arvores. Varre com folga de TREE_RADIUS em volta, porque a copa de uma
    //   arvore nascida no chunk vizinho invade este aqui. Como o treeAt so
    //   depende da coordenada global, os dois chunks calculam a mesma arvore
    //   e a copa fecha sem costura, sem ninguem precisar se falar.
    for (int dz = -TREE_RADIUS; dz < CHUNK_SIZE + TREE_RADIUS; dz++)
    {
        for (int dx = -TREE_RADIUS; dx < CHUNK_SIZE + TREE_RADIUS; dx++)
            placeTree(chunk, originX, originZ, originX + dx, originZ + dz);
    }

    chunk.dirty = true;
}
