#include "ChunkMesher.h"
#include "Texture.h"

#include <glad/glad.h>

//Cantos de cada face num cubo unitario (0..1), na ordem baixo-esquerda,
//baixo-direita, cima-direita, cima-esquerda, vista DE FORA.
//Essa ordem garante winding CCW, que e o que o GL_CULL_FACE espera.
//Como o cubo vai de 0 a 1, basta somar (x, y, z) do bloco pra posicionar.
static const float FACE_POS[6][4][3] = {
    // frente (+Z)
    { { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 } },
    // tras (-Z)
    { { 1, 0, 0 }, { 0, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 } },
    // esquerda (-X)
    { { 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 1 }, { 0, 1, 0 } },
    // direita (+X)
    { { 1, 0, 1 }, { 1, 0, 0 }, { 1, 1, 0 }, { 1, 1, 1 } },
    // topo (+Y)
    { { 0, 1, 1 }, { 1, 1, 1 }, { 1, 1, 0 }, { 0, 1, 0 } },
    // fundo (-Y)
    { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 1 }, { 0, 0, 1 } }
};

//Vizinho de cada face, na mesma ordem do BlockFace.
static const int FACE_DIR[6][3] = {
    {  0,  0,  1 },  // frente
    {  0,  0, -1 },  // tras
    { -1,  0,  0 },  // esquerda
    {  1,  0,  0 },  // direita
    {  0,  1,  0 },  // topo
    {  0, -1,  0 }   // fundo
};

//Luz fixa por orientacao, sem fonte de luz nenhuma. E o truque mais barato
//que existe pra dar volume: sem isso as 6 faces saem com a mesma cor e o
//mundo parece uma colagem chapada.
static const float FACE_LIGHT[6] = {
    0.80f,  // frente (+Z)
    0.80f,  // tras   (-Z)
    0.60f,  // esquerda (-X)
    0.60f,  // direita  (+X)
    1.00f,  // topo   (+Y), recebe o "ceu"
    0.50f   // fundo  (-Y), o mais escuro
};

//Os dois eixos que percorrem a face, no plano dela. Precisam casar com a
//ordem dos cantos do FACE_POS: o canto i fica em (U[i] * tu + V[i] * tv).
static const int FACE_TU[6][3] = {
    {  1, 0,  0 },  // frente
    { -1, 0,  0 },  // tras
    {  0, 0,  1 },  // esquerda
    {  0, 0, -1 },  // direita
    {  1, 0,  0 },  // topo
    {  1, 0,  0 }   // fundo
};
static const int FACE_TV[6][3] = {
    { 0,  1,  0 },  // frente
    { 0,  1,  0 },  // tras
    { 0,  1,  0 },  // esquerda
    { 0,  1,  0 },  // direita
    { 0,  0, -1 },  // topo
    { 0,  0,  1 }   // fundo
};

//Sinal de cada canto nos eixos da face, na ordem BL, BR, TR, TL.
static const int CORNER_U[4] = { -1, 1, 1, -1 };
static const int CORNER_V[4] = { -1, -1, 1, 1 };

//Quanto cada nivel de oclusao escurece o vertice. Nivel 3 e ceu aberto.
static const float AO_LEVEL[4] = { 0.50f, 0.70f, 0.85f, 1.00f };

//Le um bloco preferindo o acesso direto ao array do proprio chunk.
//So sai pro World quando a coordenada cai fora dele, o que so acontece na borda.
static BlockID readBlock(const World& world, const Chunk* chunk,
    int originX, int originZ, int wx, int wy, int wz)
{
    int lx = wx - originX;
    int lz = wz - originZ;

    if (lx >= 0 && lx < CHUNK_SIZE && lz >= 0 && lz < CHUNK_SIZE)
        return chunk->getBlock(lx, wy, lz);

    return world.getBlock(wx, wy, wz);
}

//Bloco que tapa luz. Vidro e folha nao contam: da pra ver atraves.
static bool occludes(const World& world, const Chunk* chunk,
    int originX, int originZ, int wx, int wy, int wz)
{
    const BlockInfo& info = blockInfo(readBlock(world, chunk, originX, originZ, wx, wy, wz));

    return info.solid && !info.transparent;
}

void buildChunkMesh(const World& world, ChunkPos pos, MeshData& out)
{
    out.solid.clear();
    out.water.clear();

    const Chunk* chunk = world.getChunk(pos);
    if (chunk == NULL)
        return;

    //Canto do chunk em coordenada de mundo.
    int originX = pos.x * CHUNK_SIZE;
    int originZ = pos.z * CHUNK_SIZE;

    //Para no bloco mais alto do chunk em vez de varrer os 256 niveis.
    for (int y = 0; y <= chunk->highestBlock; y++)
    {
        for (int z = 0; z < CHUNK_SIZE; z++)
        {
            for (int x = 0; x < CHUNK_SIZE; x++)
            {
                BlockID id = chunk->getBlock(x, y, z);
                if (id == BLOCK_AIR)
                    continue;

                const BlockInfo& info = blockInfo(id);

                int wx = originX + x;
                int wz = originZ + z;

                for (int face = 0; face < 6; face++)
                {
                    int nx = wx + FACE_DIR[face][0];
                    int ny = y + FACE_DIR[face][1];
                    int nz = wz + FACE_DIR[face][2];

                    //Consulta o mundo na borda: e isso que evita uma parede
                    //a cada 16 blocos na costura entre chunks.
                    BlockID neighborId = readBlock(world, chunk, originX, originZ, nx, ny, nz);
                    const BlockInfo& neighbor = blockInfo(neighborId);

                    //Vizinho solido e opaco tampa esta face: nem emite.
                    if (neighbor.solid && !neighbor.transparent)
                        continue;

                    //Dois blocos transparentes iguais (agua com agua, vidro com
                    //vidro) nao precisam de face entre eles: so viraria lixo dentro
                    //do volume. Sem isso um lago vira milhares de faces invisiveis.
                    if (neighborId == id && neighbor.transparent)
                        continue;

                    glm::vec4 uv = atlasUV(info.tiles[face]);
                    glm::vec3 tint = blockFaceTint(id, face);

                    const float faceUV[4][2] = {
                        { uv.x, uv.y },
                        { uv.z, uv.y },
                        { uv.z, uv.w },
                        { uv.x, uv.w }
                    };

                    //Ambient occlusion por vertice: quanto mais bloco em volta
                    //do canto, mais escuro. Os 3 vizinhos consultados ficam na
                    //camada colada na face, por isso tudo parte de (nx, ny, nz).
                    float ao[4];

                    for (int i = 0; i < 4; i++)
                    {
                        int su = CORNER_U[i];
                        int sv = CORNER_V[i];

                        bool side1 = occludes(world, chunk, originX, originZ,
                            nx + FACE_TU[face][0] * su,
                            ny + FACE_TU[face][1] * su,
                            nz + FACE_TU[face][2] * su);

                        bool side2 = occludes(world, chunk, originX, originZ,
                            nx + FACE_TV[face][0] * sv,
                            ny + FACE_TV[face][1] * sv,
                            nz + FACE_TV[face][2] * sv);

                        int level;

                        //Dois lados fechados ja formam uma quina: nem adianta
                        //consultar o canto, a luz nao chega ali de jeito nenhum.
                        if (side1 && side2)
                        {
                            level = 0;
                        }
                        else
                        {
                            bool corner = occludes(world, chunk, originX, originZ,
                                nx + FACE_TU[face][0] * su + FACE_TV[face][0] * sv,
                                ny + FACE_TU[face][1] * su + FACE_TV[face][1] * sv,
                                nz + FACE_TU[face][2] * su + FACE_TV[face][2] * sv);

                            level = 3 - ((side1 ? 1 : 0) + (side2 ? 1 : 0) + (corner ? 1 : 0));
                        }

                        ao[i] = AO_LEVEL[level];
                    }

                    float light = FACE_LIGHT[face];

                    //Agua vai pro buffer do segundo passe; todo o resto,
                    //inclusive vidro e folha, fica no primeiro.
                    MeshBuffer& target = info.translucent ? out.water : out.solid;

                    uint32_t base = (uint32_t)(target.vertices.size() / 8);

                    for (int i = 0; i < 4; i++)
                    {
                        float shade = light * ao[i];

                        target.vertices.push_back(FACE_POS[face][i][0] + (float)wx);
                        target.vertices.push_back(FACE_POS[face][i][1] + (float)y);
                        target.vertices.push_back(FACE_POS[face][i][2] + (float)wz);
                        target.vertices.push_back(tint.r * shade);
                        target.vertices.push_back(tint.g * shade);
                        target.vertices.push_back(tint.b * shade);
                        target.vertices.push_back(faceUV[i][0]);
                        target.vertices.push_back(faceUV[i][1]);
                    }

                    //Um quad vira dois triangulos, e da pra cortar por duas
                    //diagonais diferentes. Com AO a escolha importa: cortar pela
                    //diagonal que liga os dois cantos mais escuros deixa um vinco
                    //visivel atravessando o bloco. Entao escolhe sempre a outra.
                    if (ao[0] + ao[2] > ao[1] + ao[3])
                    {
                        target.indices.push_back(base + 0);
                        target.indices.push_back(base + 1);
                        target.indices.push_back(base + 2);
                        target.indices.push_back(base + 0);
                        target.indices.push_back(base + 2);
                        target.indices.push_back(base + 3);
                    }
                    else
                    {
                        target.indices.push_back(base + 1);
                        target.indices.push_back(base + 2);
                        target.indices.push_back(base + 3);
                        target.indices.push_back(base + 1);
                        target.indices.push_back(base + 3);
                        target.indices.push_back(base + 0);
                    }
                }
            }
        }
    }
}

ChunkMesh::ChunkMesh()
{
    solid.VAO = 0; solid.VBO = 0; solid.EBO = 0; solid.indexCount = 0; solid.created = false;
    water.VAO = 0; water.VBO = 0; water.EBO = 0; water.indexCount = 0; water.created = false;
}

void ChunkMesh::uploadBuffer(Buffers& b, const MeshBuffer& data)
{
    if (!b.created)
    {
        glGenVertexArrays(1, &b.VAO);
        glGenBuffers(1, &b.VBO);
        glGenBuffers(1, &b.EBO);
        b.created = true;
    }

    b.indexCount = (int)data.indices.size();

    glBindVertexArray(b.VAO);

    //GL_DYNAMIC_DRAW: o chunk vai ser remeshado quando um bloco mudar.
    glBindBuffer(GL_ARRAY_BUFFER, b.VBO);
    glBufferData(GL_ARRAY_BUFFER, data.vertices.size() * sizeof(float), data.vertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, b.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.indices.size() * sizeof(uint32_t), data.indices.data(), GL_DYNAMIC_DRAW);

    //Stride 8: 3 de posicao + 3 de cor + 2 de uv.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void ChunkMesh::upload(const MeshData& data)
{
    uploadBuffer(solid, data.solid);
    uploadBuffer(water, data.water);
}

void ChunkMesh::drawBuffer(const Buffers& b)
{
    if (b.indexCount == 0)
        return;

    glBindVertexArray(b.VAO);
    glDrawElements(GL_TRIANGLES, b.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void ChunkMesh::drawSolid() const
{
    drawBuffer(solid);
}

void ChunkMesh::drawWater() const
{
    drawBuffer(water);
}

void ChunkMesh::destroyBuffer(Buffers& b)
{
    if (!b.created)
        return;

    glDeleteVertexArrays(1, &b.VAO);
    glDeleteBuffers(1, &b.VBO);
    glDeleteBuffers(1, &b.EBO);

    b.VAO = 0;
    b.VBO = 0;
    b.EBO = 0;
    b.indexCount = 0;
    b.created = false;
}

void ChunkMesh::destroy()
{
    destroyBuffer(solid);
    destroyBuffer(water);
}
