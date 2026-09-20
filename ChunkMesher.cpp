#include "ChunkMesher.h"
#include "Texture.h"

#include <glad/glad.h>
#include <vector>

//Cantos de cada face num cubo unitario (0..1), na ordem baixo-esquerda,
//baixo-direita, cima-direita, cima-esquerda, vista DE FORA.
//Essa ordem garante winding CCW, que e o que o GL_CULL_FACE espera.
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

//Os dois eixos que percorrem a face, no plano dela. Precisam casar com a
//ordem dos cantos do FACE_POS: o canto i fica em (U[i] * tu + V[i] * tv).
static const int FACE_TU[6][3] = {
    {  1, 0,  0 }, { -1, 0,  0 }, {  0, 0,  1 },
    {  0, 0, -1 }, {  1, 0,  0 }, {  1, 0,  0 }
};
static const int FACE_TV[6][3] = {
    { 0,  1,  0 }, { 0,  1,  0 }, { 0,  1,  0 },
    { 0,  1,  0 }, { 0,  0, -1 }, { 0,  0,  1 }
};

//Sinal de cada canto nos eixos da face, na ordem BL, BR, TR, TL.
static const int CORNER_U[4] = { -1, 1, 1, -1 };
static const int CORNER_V[4] = { -1, -1, 1, 1 };

//As mesmas tabelas acima, reduzidas a indice de eixo e sentido, pro greedy
//varrer o plano da face. Indice de eixo: 0 = x, 1 = y, 2 = z.
static const int SLICE_AXIS[6] = { 2, 2, 0, 0, 1, 1 };
static const int U_AXIS[6] = { 0, 0, 2, 2, 0, 0 };
static const int U_SIGN[6] = { 1, -1, 1, -1, 1, 1 };
static const int V_AXIS[6] = { 1, 1, 1, 1, 2, 2 };
static const int V_SIGN[6] = { 1, 1, 1, 1, -1, 1 };

//A luz por face, a tabela de AO e as cores de tint vivem no
//shaders/basic.vert: o vertice so carrega os indices.

void captureSnapshot(const World& world, ChunkPos pos, ChunkSnapshot& out)
{
    out.pos = pos;
    out.blocks.assign((size_t)SNAP_SIZE * SNAP_SIZE * CHUNK_HEIGHT, (uint8_t)BLOCK_AIR);

    const Chunk* center = world.getChunk(pos);
    out.highestBlock = (center != NULL) ? center->highestBlock : -1;

    if (center == NULL)
        return;

    //Copia ate um nivel acima do topo: o mesher olha o vizinho de cima
    //pra decidir se emite a face do topo.
    int maxY = out.highestBlock + 1;
    if (maxY >= CHUNK_HEIGHT)
        maxY = CHUNK_HEIGHT - 1;

    //Cache dos 9 chunks, pra nao repetir busca no mapa a cada coluna.
    const Chunk* around[3][3];
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++)
            around[dx + 1][dz + 1] = world.getChunk(ChunkPos{ pos.x + dx, pos.z + dz });

    for (int lz = -1; lz <= CHUNK_SIZE; lz++)
    {
        for (int lx = -1; lx <= CHUNK_SIZE; lx++)
        {
            //Em qual dos 9 chunks esta coluna cai, e onde dentro dele.
            int cx = (lx < 0) ? -1 : ((lx >= CHUNK_SIZE) ? 1 : 0);
            int cz = (lz < 0) ? -1 : ((lz >= CHUNK_SIZE) ? 1 : 0);

            const Chunk* src = around[cx + 1][cz + 1];
            if (src == NULL)
                continue;

            int sx = lx - cx * CHUNK_SIZE;
            int sz = lz - cz * CHUNK_SIZE;

            for (int y = 0; y <= maxY; y++)
            {
                out.blocks[(size_t)(lx + 1) + SNAP_SIZE * ((size_t)(lz + 1) + SNAP_SIZE * (size_t)y)] =
                    (uint8_t)src->getBlock(sx, y, sz);
            }
        }
    }
}

//Bloco que tapa luz. Vidro e folha nao contam: da pra ver atraves.
static bool occludes(const ChunkSnapshot& snap, int lx, int y, int lz)
{
    const BlockInfo& info = blockInfo(snap.get(lx, y, lz));

    return info.solid && !info.transparent;
}

//Nivel de oclusao (0 a 3) de um canto da face. Os 3 vizinhos consultados
//ficam na camada colada na face, por isso tudo parte de (nx, ny, nz).
static int aoLevelAt(const ChunkSnapshot& snap, int nx, int ny, int nz, int face, int corner)
{
    int su = CORNER_U[corner];
    int sv = CORNER_V[corner];

    bool side1 = occludes(snap,
        nx + FACE_TU[face][0] * su,
        ny + FACE_TU[face][1] * su,
        nz + FACE_TU[face][2] * su);

    bool side2 = occludes(snap,
        nx + FACE_TV[face][0] * sv,
        ny + FACE_TV[face][1] * sv,
        nz + FACE_TV[face][2] * sv);

    //Dois lados fechados ja formam uma quina: nem adianta consultar o canto,
    //a luz nao chega ali de jeito nenhum.
    if (side1 && side2)
        return 0;

    bool corn = occludes(snap,
        nx + FACE_TU[face][0] * su + FACE_TV[face][0] * sv,
        ny + FACE_TU[face][1] * su + FACE_TV[face][1] * sv,
        nz + FACE_TU[face][2] * su + FACE_TV[face][2] * sv);

    return 3 - ((side1 ? 1 : 0) + (side2 ? 1 : 0) + (corn ? 1 : 0));
}

//Assinatura de uma face. Duas faces so podem ser fundidas se a assinatura
//for igual, e ela inclui os 4 niveis de AO: fundir faces com AO diferente
//apagaria o sombreado de quina.
//Zero significa "nao ha face aqui".
static uint32_t faceKey(const ChunkSnapshot& snap, int x, int y, int z, int face)
{
    BlockID id = snap.get(x, y, z);
    if (id == BLOCK_AIR)
        return 0;

    const BlockInfo& info = blockInfo(id);

    int nx = x + FACE_DIR[face][0];
    int ny = y + FACE_DIR[face][1];
    int nz = z + FACE_DIR[face][2];

    BlockID neighborId = snap.get(nx, ny, nz);
    const BlockInfo& neighbor = blockInfo(neighborId);

    //Vizinho solido e opaco tampa esta face.
    if (neighbor.solid && !neighbor.transparent)
        return 0;

    //Dois transparentes iguais nao precisam de face entre eles.
    if (neighborId == id && neighbor.transparent)
        return 0;

    uint32_t key = 1u
        | ((uint32_t)id << 1)
        | ((uint32_t)info.tiles[face] << 9)
        | ((uint32_t)blockFaceTintIndex(id, face) << 17);

    for (int i = 0; i < 4; i++)
        key |= (uint32_t)aoLevelAt(snap, nx, ny, nz, face, i) << (19 + i * 2);

    return key;
}

//Converte (fatia, u, v) do plano da face pra coordenada local do chunk.
static void cellToBlock(int face, int slice, int u, int v, int maxY, int& x, int& y, int& z)
{
    int coord[3] = { 0, 0, 0 };
    int extent[3] = { CHUNK_SIZE, maxY, CHUNK_SIZE };

    //O eixo da fatia anda sempre no sentido positivo; o sentido da face ja
    //esta embutido no FACE_POS.
    coord[SLICE_AXIS[face]] = slice;

    //Eixo com sinal negativo percorre a fatia de tras pra frente.
    int ua = U_AXIS[face];
    coord[ua] = (U_SIGN[face] > 0) ? u : (extent[ua] - 1 - u);

    int va = V_AXIS[face];
    coord[va] = (V_SIGN[face] > 0) ? v : (extent[va] - 1 - v);

    x = coord[0];
    y = coord[1];
    z = coord[2];
}

void buildChunkMesh(const ChunkSnapshot& snap, MeshData& out)
{
    out.solid.clear();
    out.water.clear();

    if (snap.highestBlock < 0)
        return;

    int maxY = snap.highestBlock + 1;
    int extent[3] = { CHUNK_SIZE, maxY, CHUNK_SIZE };

    //thread_local porque varios workers meshando ao mesmo tempo nao podem
    //dividir a mesma mascara.
    static thread_local std::vector<uint32_t> mask;

    for (int face = 0; face < 6; face++)
    {
        int sliceMax = extent[SLICE_AXIS[face]];
        int uMax = extent[U_AXIS[face]];
        int vMax = extent[V_AXIS[face]];

        for (int slice = 0; slice < sliceMax; slice++)
        {
            //1) Monta a mascara desta fatia.
            mask.assign((size_t)uMax * vMax, 0u);
            bool anything = false;

            for (int v = 0; v < vMax; v++)
            {
                for (int u = 0; u < uMax; u++)
                {
                    int x, y, z;
                    cellToBlock(face, slice, u, v, maxY, x, y, z);

                    uint32_t k = faceKey(snap, x, y, z, face);
                    mask[(size_t)v * uMax + u] = k;

                    if (k != 0)
                        anything = true;
                }
            }

            if (!anything)
                continue;

            //2) Varre a mascara juntando retangulos maximos de chave igual.
            for (int v = 0; v < vMax; v++)
            {
                for (int u = 0; u < uMax; )
                {
                    uint32_t k = mask[(size_t)v * uMax + u];
                    if (k == 0)
                    {
                        u++;
                        continue;
                    }

                    //Estica pra direita enquanto a chave for a mesma.
                    int w = 1;
                    while (u + w < uMax && mask[(size_t)v * uMax + u + w] == k)
                        w++;

                    //Estica pra cima, mas so se a linha inteira casar.
                    int h = 1;
                    bool grow = true;

                    while (v + h < vMax && grow)
                    {
                        for (int i = 0; i < w; i++)
                        {
                            if (mask[(size_t)(v + h) * uMax + u + i] != k)
                            {
                                grow = false;
                                break;
                            }
                        }

                        if (grow)
                            h++;
                    }

                    //3) Emite um unico quad pro retangulo inteiro.
                    BlockID id = (BlockID)((k >> 1) & 255u);
                    uint32_t tile = (k >> 9) & 255u;
                    uint32_t tint = (k >> 17) & 3u;

                    MeshBuffer& target = blockInfo(id).translucent ? out.water : out.solid;
                    uint32_t base = (uint32_t)(target.vertices.size() / 2);

                    //Cada canto vem da celula correspondente do retangulo:
                    //BL da celula inicial, BR da ultima coluna, e assim por diante.
                    const int cu[4] = { u, u + w - 1, u + w - 1, u };
                    const int cv[4] = { v, v,         v + h - 1, v + h - 1 };

                    //Coordenada de textura em unidades de tile. O shader aplica
                    //fract, entao a textura se repete ao longo do quad fundido.
                    const int repU[4] = { 0, w, w, 0 };
                    const int repV[4] = { 0, 0, h, h };

                    for (int i = 0; i < 4; i++)
                    {
                        int bx, by, bz;
                        cellToBlock(face, slice, cu[i], cv[i], maxY, bx, by, bz);

                        uint32_t lx = (uint32_t)(bx + (int)FACE_POS[face][i][0]);
                        uint32_t ly = (uint32_t)(by + (int)FACE_POS[face][i][1]);
                        uint32_t lz = (uint32_t)(bz + (int)FACE_POS[face][i][2]);

                        uint32_t ao = (k >> (19 + i * 2)) & 3u;

                        uint32_t w0 = lx
                            | (ly << 5)
                            | (lz << 14)
                            | ((uint32_t)face << 19)
                            | (ao << 22)
                            | (tint << 24);

                        uint32_t w1 = tile
                            | ((uint32_t)repU[i] << 8)
                            | ((uint32_t)repV[i] << 13);

                        target.vertices.push_back(w0);
                        target.vertices.push_back(w1);
                    }

                    //Um quad vira dois triangulos, e da pra cortar por duas
                    //diagonais. Com AO a escolha importa: cortar pela diagonal
                    //que liga os dois cantos mais escuros deixa um vinco
                    //visivel atravessando o bloco. Entao escolhe sempre a outra.
                    uint32_t a0 = (k >> 19) & 3u;
                    uint32_t a1 = (k >> 21) & 3u;
                    uint32_t a2 = (k >> 23) & 3u;
                    uint32_t a3 = (k >> 25) & 3u;

                    if (a0 + a2 > a1 + a3)
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

                    //4) Apaga o retangulo da mascara pra nao emitir de novo.
                    for (int dv = 0; dv < h; dv++)
                    {
                        for (int du = 0; du < w; du++)
                            mask[(size_t)(v + dv) * uMax + u + du] = 0;
                    }

                    u += w;
                }
            }
        }
    }
}

ChunkMesh::ChunkMesh()
    : highestBlock(-1)
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
    glBufferData(GL_ARRAY_BUFFER, data.vertices.size() * sizeof(uint32_t), data.vertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, b.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.indices.size() * sizeof(uint32_t), data.indices.data(), GL_DYNAMIC_DRAW);

    //Um atributo so, de 2 inteiros. Tem que ser AttribIPointer: o Pointer
    //normal converteria os bits pra float e destruiria o empacotamento.
    glVertexAttribIPointer(0, 2, GL_UNSIGNED_INT, 2 * sizeof(uint32_t), (void*)0);
    glEnableVertexAttribArray(0);

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

    //Nao desliga o VAO depois: quem desenha em seguida liga o proprio.
    //Desligar a cada chunk dobrava as trocas de estado por frame a toa.
    glBindVertexArray(b.VAO);
    glDrawElements(GL_TRIANGLES, b.indexCount, GL_UNSIGNED_INT, 0);
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
