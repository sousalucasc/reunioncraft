#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>
#include <unordered_set>
#include <string>
#include <cstdio>
#include <cmath>

#include "Shader.h"
#include "Camera.h"
#include "Texture.h"
#include "Block.h"
#include "Chunk.h"
#include "World.h"
#include "ChunkMesher.h"
#include "TerrainGenerator.h"
#include "Raycast.h"
#include "Player.h"
#include "Frustum.h"
#include "ChunkWorkers.h"
#include "ChunkStorage.h"
#include "UIRenderer.h"
#include "Sky.h"
#include "ShadowMap.h"

//Globais porque os callbacks do GLFW sao funcoes livres e nao carregam contexto.
Camera camera(glm::vec3(64.0f, 70.0f, 170.0f));
float lastX = 640.0f;
float lastY = 360.0f;
bool firstMouse = true;

//Quantos chunks de raio ficam carregados em volta do jogador.
const int RENDER_DISTANCE = 24;
//Teto por frame de trabalho DESPACHADO pros workers. Nao e mais o custo da
//thread principal, so evita encher a fila sem necessidade.
const int JOBS_PER_FRAME = 8;
//Uploads pra GPU por frame. Esse sim e custo da thread principal: o contexto
//OpenGL e dela, e cada upload e um glBufferData que pode travar o driver.
const int UPLOADS_PER_FRAME = 4;
//Alcance do raycast, em blocos.
const float REACH = 6.0f;

//Intervalo de troca de buffer: 0 nao espera o monitor, 1 espera um refresh,
//2 espera dois (metade da taxa).
//Com 1 o FPS fica preso na taxa do monitor e nao da pra medir o custo real
//de nada. Com 0 aparece tearing, mas o numero passa a significar algo.
const int VSYNC = 0;

//Wireframe liga/desliga no F. Serve pra conferir que face interna nao existe.
bool wireframe = false;
bool wireKeyWasDown = false;

//Barra de blocos, teclas 1 a 9.
const BlockID HOTBAR[9] = {
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_COBBLESTONE,
    BLOCK_SAND,
    BLOCK_PLANKS_OAK,
    BLOCK_LOG_OAK,
    BLOCK_GLASS,
    BLOCK_LEAVES_OAK
};
int hotbarSlot = 0;

//Guardam o estado do frame anterior. Sem isso um clique de 100 ms viraria
//30 cliques, porque o botao continua pressionado em dezenas de frames.
bool leftWasDown = false;
bool rightWasDown = false;
bool flyKeyWasDown = false;
bool timeKeyWasDown = false;

//Hora do dia, de 0 a 1. Comeca de manha.
float dayTime = 0.30f;

//Estatistica do ultimo frame, so pra linha de status.
int drawnChunks = 0;
int totalChunks = 0;
//Draws gastos so pra encher os mapas de sombra. Somados os das cascatas que
//foram refeitas neste frame.
int shadowDraws = 0;

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    //No primeiro frame o delta seria gigante: so guarda a posicao.
    if (firstMouse)
    {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastX;
    //Invertido: o Y da tela cresce pra baixo.
    float yoffset = lastY - (float)ypos;

    lastX = (float)xpos;
    lastY = (float)ypos;

    camera.processMouse(xoffset, yoffset);
}

//Le o teclado e entrega o movimento ao Player. A camera so guarda a
//orientacao agora; quem tem posicao e o jogador.
void processInput(GLFWwindow* window, float deltaTime, const World& world, Player& player)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    //So na transicao de solto pra pressionado, senao alterna todo frame.
    bool wireKeyDown = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    if (wireKeyDown && !wireKeyWasDown)
    {
        wireframe = !wireframe;
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    }
    wireKeyWasDown = wireKeyDown;

    //T adianta o relogio em 1/8 de dia, pra nao esperar o ciclo inteiro.
    bool timeKeyDown = glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS;
    if (timeKeyDown && !timeKeyWasDown)
    {
        dayTime += 0.125f;
        if (dayTime >= 1.0f)
            dayTime -= 1.0f;
    }
    timeKeyWasDown = timeKeyDown;

    bool flyKeyDown = glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS;
    if (flyKeyDown && !flyKeyWasDown)
    {
        player.flying = !player.flying;
        player.velocity = glm::vec3(0.0f);
        std::cout << (player.flying ? "voando" : "andando") << std::endl;
    }
    flyKeyWasDown = flyKeyDown;

    //Teclas 1 a 9 escolhem o bloco da hotbar.
    for (int i = 0; i < 9; i++)
    {
        if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS && hotbarSlot != i)
        {
            hotbarSlot = i;
            std::cout << "slot " << (i + 1) << " | bloco " << (int)HOTBAR[i] << std::endl;
        }
    }

    //Direcao desejada, relativa a para onde a camera olha, mas achatada no
    //plano XZ: olhar pro chao nao pode empurrar o jogador pra baixo.
    glm::vec3 flatFront(camera.front.x, 0.0f, camera.front.z);
    glm::vec3 flatRight(camera.right.x, 0.0f, camera.right.z);

    //Olhando quase reto pra cima ou pra baixo o vetor achatado vira zero,
    //e normalizar zero da NaN. Nesse caso nao ha direcao horizontal.
    if (glm::length(flatFront) > 0.0001f)
        flatFront = glm::normalize(flatFront);
    else
        flatFront = glm::vec3(0.0f);

    if (glm::length(flatRight) > 0.0001f)
        flatRight = glm::normalize(flatRight);
    else
        flatRight = glm::vec3(0.0f);

    glm::vec3 wish(0.0f);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        wish += flatFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        wish -= flatFront;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        wish += flatRight;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        wish -= flatRight;

    bool spaceDown = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    bool shiftDown = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

    //Voando, espaco e shift viram subir e descer. Andando, espaco pula.
    if (player.flying)
    {
        if (spaceDown)
            wish.y += 1.0f;
        if (shiftDown)
            wish.y -= 1.0f;
    }

    //Normalizar impede que andar na diagonal seja 41% mais rapido.
    if (glm::length(wish) > 0.0001f)
        wish = glm::normalize(wish);

    player.update(world, wish, spaceDown, deltaTime);

    //A camera segue os olhos do jogador.
    camera.position = player.eyePosition();
}

//Quebrar e colocar. Recebe o hit ja calculado pelo raycast deste frame.
//Le o botao por polling em vez de callback: assim o mundo e o hit chegam
//como parametro, sem precisar de mais variavel global.
//O bloco que vai ser colocado invade a caixa do jogador?
//Sem isso da pra se emparedar clicando no proprio pe.
bool blockOverlapsPlayer(const Player& player, int bx, int by, int bz)
{
    float half = PLAYER_WIDTH * 0.5f;

    float minX = player.position.x - half;
    float maxX = player.position.x + half;
    float minY = player.position.y;
    float maxY = player.position.y + PLAYER_HEIGHT;
    float minZ = player.position.z - half;
    float maxZ = player.position.z + half;

    //Caixas se tocando de raspao nao contam, por isso o > e nao o >=.
    return (maxX > (float)bx && minX < (float)bx + 1.0f)
        && (maxY > (float)by && minY < (float)by + 1.0f)
        && (maxZ > (float)bz && minZ < (float)bz + 1.0f);
}

void handleBlockEdit(GLFWwindow* window, World& world, const RaycastHit& hit, const Player& player)
{
    bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool rightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    //So age na transicao de solto pra pressionado.
    bool leftClick = leftDown && !leftWasDown;
    bool rightClick = rightDown && !rightWasDown;

    leftWasDown = leftDown;
    rightWasDown = rightDown;

    if (!hit.hit)
        return;

    //Esquerdo quebra o bloco mirado.
    if (leftClick)
    {
        //Bedrock e o fundo do mundo: nao deixa furar.
        if (world.getBlock(hit.x, hit.y, hit.z) != BLOCK_BEDROCK)
            world.setBlock(hit.x, hit.y, hit.z, BLOCK_AIR);
    }

    //Direito coloca no espaco livre em frente a face atingida.
    if (rightClick)
    {
        int px = hit.x + hit.nx;
        int py = hit.y + hit.ny;
        int pz = hit.z + hit.nz;

        BlockID target = world.getBlock(px, py, pz);
        bool freeSpace = (target == BLOCK_AIR || target == BLOCK_WATER);

        if (freeSpace && !blockOverlapsPlayer(player, px, py, pz))
            world.setBlock(px, py, pz, HOTBAR[hotbarSlot]);
    }
}

//Cria janela + contexto OpenGL e carrega o GLAD. Devolve NULL se falhar.
GLFWwindow* initWindow(int width, int height, const char* title)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return NULL;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(VSYNC);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return NULL;
    }

    glViewport(0, 0, width, height);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    //Esconde e prende o cursor na janela, estilo FPS.
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);

    //Descarta fragmentos atras de outros.
    glEnable(GL_DEPTH_TEST);
    //Descarta faces viradas pra longe da camera (winding CCW = frente).
    glEnable(GL_CULL_FACE);

    return window;
}

typedef std::unordered_map<ChunkPos, ChunkMesh, ChunkPosHash> MeshMap;

//Conjuntos de trabalho em voo, pra nao submeter o mesmo chunk duas vezes.
//Um unico trabalho por chunk tambem garante que os resultados nao possam
//chegar fora de ordem e sobrescrever uma mesh mais nova com uma velha.
typedef std::unordered_set<ChunkPos, ChunkPosHash> ChunkSet;

//Recolhe o que os workers terminaram e devolve quantas meshes subiram pra GPU.
int collectResults(World& world, MeshMap& meshes, ChunkWorkers& workers,
    ChunkSet& genInFlight, ChunkSet& meshInFlight, int maxUploads)
{
    //Chunks gerados entram no mundo. Sem teto: inserir e barato, e um chunk
    //que fica na fila e um buraco no mundo.
    GenResult gen;
    while (workers.popGenerated(gen))
    {
        genInFlight.erase(gen.pos);
        world.insertChunk(gen.pos, std::move(gen.chunk));
    }

    //Meshes prontas sobem pra GPU, com teto.
    int uploaded = 0;
    MeshResult mesh;

    while (uploaded < maxUploads && workers.popMesh(mesh))
    {
        meshInFlight.erase(mesh.pos);

        //Pode ter sido descarregado enquanto o worker trabalhava.
        if (world.getChunk(mesh.pos) == NULL)
            continue;

        meshes[mesh.pos].upload(mesh.data);
        uploaded++;
    }

    return uploaded;
}

//Despacha geracao do que falta no raio e mesh dos chunks sujos.
void dispatchJobs(World& world, ChunkWorkers& workers,
    ChunkSet& genInFlight, ChunkSet& meshInFlight, ChunkPos center, int renderDistance, int maxJobs)
{
    int jobs = 0;

    //Do anel mais proximo pro mais distante, pra que o perto apareca antes.
    for (int r = 0; r <= renderDistance && jobs < maxJobs; r++)
    {
        for (int dz = -r; dz <= r && jobs < maxJobs; dz++)
        {
            for (int dx = -r; dx <= r && jobs < maxJobs; dx++)
            {
                if (std::max(std::abs(dx), std::abs(dz)) != r)
                    continue;

                ChunkPos pos{ center.x + dx, center.z + dz };

                if (world.getChunk(pos) != NULL)
                    continue;
                if (genInFlight.count(pos) != 0)
                    continue;

                genInFlight.insert(pos);
                workers.submitGenerate(pos);
                jobs++;
            }
        }
    }

    //Mesh dos sujos. O instantaneo e tirado aqui, na thread principal, que e
    //a unica que mexe no World. Dali pra frente o worker trabalha sozinho.
    for (ChunkMap::const_iterator it = world.allChunks().begin();
         it != world.allChunks().end() && jobs < maxJobs; ++it)
    {
        Chunk* chunk = it->second.get();
        if (!chunk->dirty)
            continue;

        //Ja tem trabalho em voo pra este chunk: deixa sujo e tenta de novo
        //quando o resultado chegar. Um trabalho por chunk de cada vez.
        if (meshInFlight.count(it->first) != 0)
            continue;

        ChunkSnapshot snap;
        captureSnapshot(world, it->first, snap);

        chunk->dirty = false;
        meshInFlight.insert(it->first);
        workers.submitMesh(std::move(snap));
        jobs++;
    }
}

//Descarta meshes de chunks que sairam do raio.
void evictMeshes(const World& world, MeshMap& meshes)
{
    for (MeshMap::iterator it = meshes.begin(); it != meshes.end(); )
    {
        if (world.getChunk(it->first) == NULL)
        {
            it->second.destroy();
            it = meshes.erase(it);
        }
        else
        {
            ++it;
        }
    }
}


//12 arestas de um cubo centrado na origem, pra desenhar com GL_LINES.
//Cada aresta sao 2 vertices, entao 24 no total.
unsigned int createWireCube()
{
    const float h = 0.5f;
    float v[] = {
        //base
        -h,-h,-h,  h,-h,-h,   h,-h,-h,  h,-h, h,   h,-h, h, -h,-h, h,  -h,-h, h, -h,-h,-h,
        //topo
        -h, h,-h,  h, h,-h,   h, h,-h,  h, h, h,   h, h, h, -h, h, h,  -h, h, h, -h, h,-h,
        //colunas
        -h,-h,-h, -h, h,-h,   h,-h,-h,  h, h,-h,   h,-h, h,  h, h, h,  -h,-h, h, -h, h, h
    };

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    return VAO;
}

//Dois triangulos cobrindo a tela inteira, em NDC. Usado pelo ceu.
unsigned int createFullscreenQuad()
{
    float v[] = {
        -1.0f, -1.0f,   1.0f, -1.0f,   1.0f,  1.0f,
        -1.0f, -1.0f,   1.0f,  1.0f,  -1.0f,  1.0f
    };

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    return VAO;
}

//Gradiente do ceu, desenhado antes do mundo e sem escrever profundidade.
void drawSky(const Shader& skyShader, unsigned int quadVAO, const SkyState& sky,
    const glm::mat4& view, const glm::mat4& projection)
{
    //O ceu fica atras de tudo: nao testa nem escreve profundidade.
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    skyShader.use();
    skyShader.setMat4("invViewProjection", glm::inverse(projection * view));
    skyShader.setVec3("cameraPos", camera.position);
    skyShader.setVec3("skyZenith", sky.zenith);
    skyShader.setVec3("skyHorizon", sky.horizon);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

//Contorno do bloco que a mira esta apontando.
void drawSelection(const Shader& lineShader, unsigned int wireVAO, const RaycastHit& hit,
    const glm::mat4& view, const glm::mat4& projection)
{
    if (!hit.hit)
        return;

    //Centro do bloco, levemente inflado pra nao brigar em z com a face.
    glm::mat4 model = glm::translate(glm::mat4(1.0f),
        glm::vec3((float)hit.x + 0.5f, (float)hit.y + 0.5f, (float)hit.z + 0.5f));
    model = glm::scale(model, glm::vec3(1.004f));

    lineShader.use();
    lineShader.setMat4("model", model);
    lineShader.setMat4("view", view);
    lineShader.setMat4("projection", projection);
    lineShader.setVec3("lineColor", glm::vec3(0.05f, 0.05f, 0.05f));

    glBindVertexArray(wireVAO);
    glDrawArrays(GL_LINES, 0, 24);
    glBindVertexArray(0);
}

//Desenha um frame.
//Enche os mapas de profundidade das cascatas. Nao desenha nada na tela: o
//resultado e consumido depois, pelo passe de cor.
void renderShadows(ShadowMap& shadows, const Shader& depthShader, const Texture& texture,
    const World& world, const MeshMap& meshes, const SkyState& sky,
    const glm::mat4& view, float aspect, int screenW, int screenH)
{
    shadows.update(view, camera.fov, aspect, sky.sunDir);

    depthShader.use();
    texture.bind(0);

    int originLoc = depthShader.uniformLocation("chunkOrigin");
    int lightLoc = depthShader.uniformLocation("lightSpace");

    //Um bloco que projeta sombra pode estar antes do plano de perto da luz.
    //Com depth clamp ele e achatado no plano em vez de ser recortado fora,
    //o que ainda produz a sombra certa e permite manter o volume apertado.
    glEnable(GL_DEPTH_CLAMP);

    //Empurra a profundidade gravada pra longe da luz, proporcional a
    //inclinacao da face. E a primeira linha de defesa contra acne, antes
    //mesmo do bias no passe de cor.
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);

    shadowDraws = 0;

    for (int c = 0; c < SHADOW_CASCADES; c++)
    {
        if (!shadows.refreshing(c))
            continue;

        shadows.beginCascade(c);

        const glm::mat4& lightSpace = shadows.cascade(c).lightSpace;
        depthShader.setMat4(lightLoc, lightSpace);

        //Recorte contra o volume da luz, nao o da camera. E o que impede o
        //passe de sombra de custar um draw por chunk carregado: a cascata de
        //perto cobre poucos blocos, entao ve pouquissimos chunks.
        Frustum lightFrustum = extractFrustum(lightSpace);

        for (MeshMap::const_iterator it = meshes.begin(); it != meshes.end(); ++it)
        {
            const Chunk* chunk = world.getChunk(it->first);
            if (chunk == NULL)
                continue;

            glm::vec3 mn((float)(it->first.x * CHUNK_SIZE), 0.0f, (float)(it->first.z * CHUNK_SIZE));
            glm::vec3 mx(mn.x + (float)CHUNK_SIZE, (float)(chunk->highestBlock + 1), mn.z + (float)CHUNK_SIZE);

            if (!aabbVisible(lightFrustum, mn, mx))
                continue;

            depthShader.setVec3(originLoc, mn);

            //So o solido. Agua nao projeta sombra, e mandar ela aqui
            //escureceria o proprio fundo do lago.
            it->second.drawSolid();
            shadowDraws++;
        }
    }

    shadows.end(screenW, screenH);

    glPolygonOffset(0.0f, 0.0f);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_DEPTH_CLAMP);
}

void render(const Shader& shader, const Shader& lineShader, const Shader& skyShader,
    const Texture& texture, const World& world, const MeshMap& meshes,
    unsigned int wireVAO, unsigned int quadVAO, const RaycastHit& hit,
    const SkyState& sky, float fogEnd, float aspect,
    const ShadowMap& shadows, bool shadowsOn)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 view = camera.getView();
    glm::mat4 projection = camera.getProjection(aspect);

    //O ceu cobre a tela inteira, entao vem primeiro e dispensa cor de limpeza.
    drawSky(skyShader, quadVAO, sky, view, projection);

    shader.use();
    texture.bind(0);

    //Fog na cor do horizonte: o terreno distante se dissolve no ceu em vez
    //de terminar num corte reto no limite do render distance.
    shader.setVec3("fogColor", sky.horizon);
    shader.setFloat("fogStart", fogEnd * 0.55f);
    shader.setFloat("fogEnd", fogEnd);
    shader.setFloat("dayLight", sky.lightLevel);

    //---- sombra ----
    //Tudo aqui vai uma vez por frame, nao por draw: sao poucos uniforms e o
    //custo some perto dos ~700 draws que vem depois.
    if (shadowsOn)
    {
        shadows.bindTexture(1);

        //Volta a unidade ativa pra 0, que e onde todo o resto do frame
        //espera encontrar o atlas.
        glActiveTexture(GL_TEXTURE0);

        for (int c = 0; c < SHADOW_CASCADES; c++)
        {
            std::string i = std::to_string(c);
            shader.setMat4("cascadeMatrix[" + i + "]", shadows.cascade(c).lightSpace);
            shader.setFloat("cascadeSplit[" + i + "]", shadows.cascade(c).splitDepth);
            shader.setFloat("cascadeTexel[" + i + "]", shadows.cascade(c).texelWorld);
        }

        shader.setVec3("sunDir", sky.sunDir);
        shader.setFloat("shadowDistance", SHADOW_DISTANCE);

        //A sombra e do SOL. Com ele abaixo do horizonte nao ha o que
        //projetar, entao a forca vai a zero junto com a altura dele e o
        //shader pula o calculo inteiro.
        float sunUp = glm::smoothstep(0.0f, 0.25f, sky.sunHeight);
        shader.setFloat("shadowStrength", SHADOW_STRENGTH * sunUp);
    }
    else
    {
        shader.setFloat("shadowStrength", 0.0f);
    }

    //Consultado uma vez e reaproveitado nos ~700 draws do frame.
    int originLoc = shader.uniformLocation("chunkOrigin");

    //O vertice guarda so a posicao local do chunk; o deslocamento pro mundo
    //vai como uniform, um por chunk, logo antes de cada draw call.
    shader.setMat4("view", view);
    shader.setMat4("projection", projection);

    //Um draw call por chunk, e so pros que estao no campo de visao.
    Frustum frustum = extractFrustum(projection * view);

    drawnChunks = 0;
    totalChunks = (int)meshes.size();

    //Agua fica pra depois, e precisa sair na ordem certa.
    struct WaterItem { float dist; const ChunkMesh* mesh; glm::vec3 origin;
        bool operator<(const WaterItem& o) const { return dist < o.dist; } };
    static std::vector<WaterItem> waterQueue;
    waterQueue.clear();

    //---- Passe 1: opaco e recorte ----
    //Vidro e folha entram aqui: o alpha deles e 0 ou 255, entao o discard
    //no shader basta e nao importa a ordem de desenho.
    for (MeshMap::const_iterator it = meshes.begin(); it != meshes.end(); ++it)
    {
        const Chunk* chunk = world.getChunk(it->first);
        if (chunk == NULL)
            continue;

        //Caixa do chunk. A altura vai so ate o bloco mais alto dele: usar os
        //256 niveis inteiros deixaria a caixa tao alta que quase nada seria
        //cortado, porque uma caixa alta cruza o frustum de qualquer angulo.
        glm::vec3 mn((float)(it->first.x * CHUNK_SIZE), 0.0f, (float)(it->first.z * CHUNK_SIZE));
        glm::vec3 mx(mn.x + (float)CHUNK_SIZE, (float)(chunk->highestBlock + 1), mn.z + (float)CHUNK_SIZE);

        if (!aabbVisible(frustum, mn, mx))
            continue;

        shader.setVec3(originLoc, mn);
        it->second.drawSolid();
        drawnChunks++;

        if (it->second.hasWater())
        {
            glm::vec3 center((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f);
            glm::vec3 d = center - camera.position;

            WaterItem item;
            item.dist = d.x * d.x + d.y * d.y + d.z * d.z;
            item.mesh = &it->second;
            item.origin = mn;
            waterQueue.push_back(item);
        }
    }

    //---- Passe 2: agua ----
    if (!waterQueue.empty())
    {
        //Do mais longe pro mais perto. Com blending o resultado depende da
        //ordem: desenhar o proximo primeiro faria o distante sumir atras dele.
        std::sort(waterQueue.begin(), waterQueue.end());
        std::reverse(waterQueue.begin(), waterQueue.end());

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        //Continua TESTANDO profundidade, mas para de ESCREVER: assim uma
        //superficie de agua nao esconde a agua que vem atras dela.
        glDepthMask(GL_FALSE);

        for (size_t i = 0; i < waterQueue.size(); i++)
        {
            shader.setVec3(originLoc, waterQueue[i].origin);
            waterQueue[i].mesh->drawWater();
        }

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    drawSelection(lineShader, wireVAO, hit, view, projection);
}

//Desenha mira, hotbar e texto de debug. Roda depois do mundo, com o teste
//de profundidade desligado.
void drawUI(UIRenderer& ui, const Texture& atlas, const Player& player,
    const World& world, const ChunkWorkers& workers,
    int screenW, int screenH, int fps, float dayTime, const SkyState& sky)
{
    ui.begin(screenW, screenH);

    const glm::vec4 branco(1.0f, 1.0f, 1.0f, 1.0f);
    const glm::vec4 fundo(0.0f, 0.0f, 0.0f, 0.45f);

    //---- mira: duas barras cruzadas no centro ----
    float cx = screenW * 0.5f;
    float cy = screenH * 0.5f;
    const float BRACO = 9.0f;
    const float GROSSURA = 2.0f;

    glm::vec4 corMira(1.0f, 1.0f, 1.0f, 0.75f);
    ui.rect(cx - BRACO, cy - GROSSURA * 0.5f, BRACO * 2.0f, GROSSURA, corMira);
    ui.rect(cx - GROSSURA * 0.5f, cy - BRACO, GROSSURA, BRACO * 2.0f, corMira);

    //---- hotbar: 9 slots centralizados embaixo ----
    const float SLOT = 50.0f;
    const float PAD = 4.0f;
    const float LARGURA = 9.0f * SLOT;

    float hx = cx - LARGURA * 0.5f;
    float hy = (float)screenH - SLOT - 12.0f;

    ui.rect(hx - PAD, hy - PAD, LARGURA + PAD * 2.0f, SLOT + PAD * 2.0f, fundo);

    for (int i = 0; i < 9; i++)
    {
        float sx = hx + i * SLOT;

        //Slot escolhido ganha uma moldura clara.
        if (i == hotbarSlot)
        {
            ui.rect(sx - 2.0f, hy - 2.0f, SLOT + 4.0f, SLOT + 4.0f, glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));
            ui.rect(sx + 1.0f, hy + 1.0f, SLOT - 2.0f, SLOT - 2.0f, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
        }

        //Mostra a face lateral do bloco: e a que identifica melhor.
        const BlockInfo& info = blockInfo(HOTBAR[i]);
        glm::vec3 tint = tintColor(blockFaceTintIndex(HOTBAR[i], FACE_FRONT));

        ui.tile(sx + 5.0f, hy + 5.0f, SLOT - 10.0f, info.tiles[FACE_FRONT],
            glm::vec4(tint.r, tint.g, tint.b, 1.0f));

        //Numero da tecla no canto do slot.
        char num[2] = { (char)('1' + i), 0 };
        ui.text(num, sx + 5.0f, hy + SLOT - 18.0f, 14.0f, glm::vec4(1.0f, 1.0f, 1.0f, 0.8f));
    }

    //---- texto de debug ----
    const float T = 16.0f;
    float ty = 8.0f;

    char linha[160];

    std::snprintf(linha, sizeof(linha), "fps %d   %s", fps,
        player.flying ? "voando" : (player.onGround ? "no chao" : "no ar"));
    ui.rect(6.0f, ty - 2.0f, ui.textWidth(linha, T) + 8.0f, T + 4.0f, fundo);
    ui.text(linha, 10.0f, ty, T, branco);
    ty += T + 6.0f;

    std::snprintf(linha, sizeof(linha), "xyz %.1f %.1f %.1f",
        player.position.x, player.position.y, player.position.z);
    ui.rect(6.0f, ty - 2.0f, ui.textWidth(linha, T) + 8.0f, T + 4.0f, fundo);
    ui.text(linha, 10.0f, ty, T, branco);
    ty += T + 6.0f;

    std::snprintf(linha, sizeof(linha), "chunks %d/%d  fila %d",
        drawnChunks, totalChunks, workers.pending());
    ui.rect(6.0f, ty - 2.0f, ui.textWidth(linha, T) + 8.0f, T + 4.0f, fundo);
    ui.text(linha, 10.0f, ty, T, branco);
    ty += T + 6.0f;

    std::snprintf(linha, sizeof(linha), "bloco %d   disco %dR/%dW",
        (int)HOTBAR[hotbarSlot], ChunkStorage::chunksLoaded(), ChunkStorage::chunksSaved());
    ui.rect(6.0f, ty - 2.0f, ui.textWidth(linha, T) + 8.0f, T + 4.0f, fundo);
    ui.text(linha, 10.0f, ty, T, branco);
    ty += T + 6.0f;

    //Hora no formato 24h, pra leitura rapida do ciclo.
    int hora = (int)(dayTime * 24.0f);
    int minuto = (int)((dayTime * 24.0f - hora) * 60.0f);
    std::snprintf(linha, sizeof(linha), "%02d:%02d   luz %.2f   (T adianta)",
        hora, minuto, sky.lightLevel);
    ui.rect(6.0f, ty - 2.0f, ui.textWidth(linha, T) + 8.0f, T + 4.0f, fundo);
    ui.text(linha, 10.0f, ty, T, branco);

    ui.end(atlas);
}

int main()
{
    GLFWwindow* window = initWindow(1280, 720, "openglEngine");
    if (window == NULL)
        return -1;

    Shader basicShader("shaders/basic.vert", "shaders/basic.frag");
    Shader lineShader("shaders/line.vert", "shaders/line.frag");
    Texture atlas("textures/atlas.png");

    unsigned int wireVAO = createWireCube();

    Shader skyShader("shaders/sky.vert", "shaders/sky.frag");
    unsigned int quadVAO = createFullscreenQuad();

    Shader depthShader("shaders/depth.vert", "shaders/depth.frag");
    depthShader.use();
    depthShader.setInt("blockTexture", 0);

    ShadowMap shadows;
    bool shadowsOk = shadows.create();
    if (!shadowsOk)
        std::cout << "aviso: sombras desligadas" << std::endl;

    //O fog termina um pouco antes da borda carregada, pra esconder o corte.
    const float FOG_END = (float)(RENDER_DISTANCE * CHUNK_SIZE) * 0.92f;

    UIRenderer ui("shaders/ui.vert", "shaders/ui.frag", "textures/font.png");
    if (!ui.ready())
        std::cout << "aviso: UI indisponivel. Rode tools/build_font.ps1 pra gerar textures/font.png" << std::endl;

    TerrainGenerator terrain(1337);
    World world;

    ChunkStorage::setWorldPath("saves/world");

    MeshMap meshes;

    //Deixa um nucleo livre pra thread principal.
    int workerCount = (int)std::thread::hardware_concurrency() - 1;
    if (workerCount < 1)
        workerCount = 1;
    if (workerCount > 8)
        workerCount = 8;

    ChunkWorkers workers(terrain, workerCount);
    ChunkSet genInFlight;
    ChunkSet meshInFlight;

    //Nasce em pe no terreno, nao no vazio.
    Player player(glm::vec3(8.5f, (float)terrain.heightAt(8, 8) + 1.0f, 8.5f));
    camera.position = player.eyePosition();

    //Primeira carga: espera terminar, senao o jogador comeca olhando pro nada.
    //Aqui nao ha teto por frame; a partir do loop, ha.
    double fillStart = glfwGetTime();
    ChunkPos spawn = World::chunkAt(camera.position.x, camera.position.z);

    //Duas rodadas: a primeira gera os chunks, a segunda mesha o que nasceu.
    for (int fase = 0; fase < 2; fase++)
    {
        do
        {
            dispatchJobs(world, workers, genInFlight, meshInFlight, spawn, RENDER_DISTANCE, 100000);
            collectResults(world, meshes, workers, genInFlight, meshInFlight, 100000);
        }
        while (workers.pending() > 0 || !genInFlight.empty() || !meshInFlight.empty());
    }

    double fillTime = glfwGetTime() - fillStart;

    std::cout << "carga inicial: " << world.chunkCount() << " chunks em "
        << (int)(fillTime * 1000.0) << " ms | raio " << RENDER_DISTANCE
        << " chunks (" << RENDER_DISTANCE * CHUNK_SIZE << " blocos)" << std::endl;
    std::cout << "WASD anda, espaco pula, V voo, T hora | esq quebra, dir coloca | 1-9 bloco | F wireframe" << std::endl;

    //O sampler le da unidade 0. Precisa ser setado uma vez, com o shader ativo.
    basicShader.use();
    basicShader.setInt("blockTexture", 0);
    //Atlas na unidade 0, mapas de sombra na 1.
    basicShader.setInt("shadowMaps", 1);

    //Cores de tint vao uma vez so; o vertice carrega apenas o indice.
    for (int i = 0; i < TINT_COUNT; i++)
    {
        std::string nome = "tints[" + std::to_string(i) + "]";
        basicShader.setVec3(nome, tintColor(i));
    }

    //Comeca do relogio atual, nao de zero: a carga inicial levou segundos,
    //e o primeiro deltaTime seria esse tempo todo de uma vez. Com uma tecla
    //pressionada, isso teleporta o jogador dezenas de blocos no frame 1.
    float lastFrame = (float)glfwGetTime();
    float statTimer = 0.0f;
    int frames = 0;
    int lastFps = 0;

    //Cada volta desse loop e um frame. Roda ate o ESC ou o X da janela.
    while (!glfwWindowShouldClose(window))
    {
        //1. Quanto tempo passou desde o frame anterior, em segundos.
        //   E o que deixa a velocidade igual em qualquer FPS: a 30 fps o deltaTime
        //   e o dobro do de 60 fps, entao a camera anda o dobro por frame e chega junto.
        float currentFrame = (float)glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        //Relogio do mundo.
        dayTime += deltaTime / DAY_LENGTH;
        if (dayTime >= 1.0f)
            dayTime -= 1.0f;

        SkyState sky = skyAt(dayTime);

        //2. Le o teclado e move a camera.
        //   O mouse NAO passa por aqui: ele chega sozinho pelo mouse_callback,
        //   que quem dispara e o glfwPollEvents la no passo 9.
        processInput(window, deltaTime, world, player);

        //3. Streaming. A thread principal so coordena: recolhe o que os
        //   workers terminaram, descarta o que saiu do raio e despacha o que
        //   falta. Gerar terreno e montar mesh acontecem nas outras threads.
        ChunkPos center = World::chunkAt(player.position.x, player.position.z);

        collectResults(world, meshes, workers, genInFlight, meshInFlight, UPLOADS_PER_FRAME);
        world.unloadFar(center, RENDER_DISTANCE);
        evictMeshes(world, meshes);
        dispatchJobs(world, workers, genInFlight, meshInFlight, center, RENDER_DISTANCE, JOBS_PER_FRAME);


        //4. Raycast da camera pra frente, pra saber qual bloco esta na mira.
        RaycastHit hit = raycast(world, camera.position, camera.front, REACH);

        //5. Clique esquerdo quebra, direito coloca. O setBlock do World ja
        //   marca o chunk vizinho como dirty quando o bloco e de borda, entao
        //   a mesh do outro lado da costura tambem se atualiza.
        handleBlockEdit(window, world, hit, player);

        //Status uma vez por segundo, pra nao inundar o console.
        statTimer += deltaTime;
        frames++;
        if (statTimer >= 1.0f)
        {
            std::cout << "fps " << frames
                << " | pos " << (int)camera.position.x << "," << (int)camera.position.y << "," << (int)camera.position.z
                << (player.flying ? " | voando" : (player.onGround ? " | no chao" : " | no ar"))
                << " | vy " << (int)player.velocity.y
                << " | chunk (" << center.x << ", " << center.z << ")"
                << " | chunks " << drawnChunks << "/" << totalChunks
                << " | sombra " << shadowDraws
                << " | fila " << workers.pending()
                << " | disco " << ChunkStorage::chunksLoaded() << "R/" << ChunkStorage::chunksSaved() << "W";
            if (hit.hit)
            {
                std::cout << " | mira (" << hit.x << "," << hit.y << "," << hit.z
                    << ") bloco " << (int)world.getBlock(hit.x, hit.y, hit.z)
                    << " normal (" << hit.nx << "," << hit.ny << "," << hit.nz << ")";
            }
            else
            {
                std::cout << " | mira: nada";
            }
            std::cout << std::endl;
            lastFps = frames;
            statTimer = 0.0f;
            frames = 0;
        }

        //6. Pega o tamanho atual do framebuffer pra calcular o aspecto (largura/altura).
        //   Se le todo frame porque a janela pode ter sido redimensionada.
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

        //7. Desenha o frame no buffer de tras (limpa a tela, ativa o shader,
        //   manda as matrizes e desenha os chunks).
        //   Com a janela minimizada a altura vira 0: nao da pra dividir e nao ha o que desenhar.
        if (fbHeight > 0)
        {
            float aspect = (float)fbWidth / (float)fbHeight;

            //As sombras vem ANTES do passe de cor: ele precisa dos mapas
            //prontos pra consultar.
            if (shadowsOk)
                renderShadows(shadows, depthShader, atlas, world, meshes, sky,
                    camera.getView(), aspect, fbWidth, fbHeight);

            render(basicShader, lineShader, skyShader, atlas, world, meshes, wireVAO, quadVAO, hit, sky, FOG_END, aspect, shadows, shadowsOk);

            if (ui.ready())
                drawUI(ui, atlas, player, world, workers, fbWidth, fbHeight, lastFps, dayTime, sky);
        }

        //8. Troca o buffer de tras com o da frente. So agora o frame aparece na tela.
        glfwSwapBuffers(window);

        //9. Processa a fila de eventos do sistema: teclado, mouse, resize, botao de fechar.
        //   E aqui que o mouse_callback e o framebuffer_size_callback sao chamados.
        //   Sem isso a janela congela e o Windows marca como "nao responde".
        glfwPollEvents();
    }

    //Salva o que ainda esta carregado. O que ja saiu do raio foi gravado
    //na hora do descarregamento.
    shadows.destroy();

    int salvos = world.saveAll();
    std::cout << "saindo | " << salvos << " chunks gravados agora, "
        << ChunkStorage::chunksSaved() << " no total desta sessao, "
        << ChunkStorage::chunksLoaded() << " lidos do disco" << std::endl;

    glfwTerminate();
    return 0;
}
