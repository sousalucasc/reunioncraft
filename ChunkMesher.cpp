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

void buildChunkMesh(const World& world, ChunkPos pos, MeshData& out)
{
    out.vertices.clear();
    out.indices.clear();

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
                    int lx = x + FACE_DIR[face][0];
                    int ly = y + FACE_DIR[face][1];
                    int lz = z + FACE_DIR[face][2];

                    //Caminho rapido: vizinho dentro do proprio chunk e so um
                    //indice de array. So a borda precisa consultar o World,
                    //que custa floorDiv mais uma busca no unordered_map.
                    //Sao ~5% dos casos, mas era 100% das consultas antes disso.
                    BlockID neighborId;
                    if (lx >= 0 && lx < CHUNK_SIZE && lz >= 0 && lz < CHUNK_SIZE)
                        neighborId = chunk->getBlock(lx, ly, lz);
                    else
                        neighborId = world.getBlock(wx + FACE_DIR[face][0], ly, wz + FACE_DIR[face][2]);

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

                    uint32_t base = (uint32_t)(out.vertices.size() / 8);

                    for (int i = 0; i < 4; i++)
                    {
                        out.vertices.push_back(FACE_POS[face][i][0] + (float)wx);
                        out.vertices.push_back(FACE_POS[face][i][1] + (float)y);
                        out.vertices.push_back(FACE_POS[face][i][2] + (float)wz);
                        out.vertices.push_back(tint.r);
                        out.vertices.push_back(tint.g);
                        out.vertices.push_back(tint.b);
                        out.vertices.push_back(faceUV[i][0]);
                        out.vertices.push_back(faceUV[i][1]);
                    }

                    out.indices.push_back(base + 0);
                    out.indices.push_back(base + 1);
                    out.indices.push_back(base + 2);
                    out.indices.push_back(base + 0);
                    out.indices.push_back(base + 2);
                    out.indices.push_back(base + 3);
                }
            }
        }
    }
}

ChunkMesh::ChunkMesh()
    : VAO(0), VBO(0), EBO(0), indexCount(0), created(false)
{
}

void ChunkMesh::upload(const MeshData& data)
{
    if (!created)
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        created = true;
    }

    indexCount = (int)data.indices.size();

    glBindVertexArray(VAO);

    //GL_DYNAMIC_DRAW: o chunk vai ser remeshado quando um bloco mudar.
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, data.vertices.size() * sizeof(float), data.vertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
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

void ChunkMesh::destroy()
{
    if (!created)
        return;

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);

    VAO = 0;
    VBO = 0;
    EBO = 0;
    indexCount = 0;
    created = false;
}

void ChunkMesh::draw() const
{
    if (indexCount == 0)
        return;

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
