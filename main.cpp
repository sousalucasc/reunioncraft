#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>
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

//Globais porque os callbacks do GLFW sao funcoes livres e nao carregam contexto.
Camera camera(glm::vec3(64.0f, 70.0f, 170.0f));
float lastX = 640.0f;
float lastY = 360.0f;
bool firstMouse = true;

//Quantos chunks de raio ficam carregados em volta do jogador.
const int RENDER_DISTANCE = 4;
//Teto por frame pra geracao e pra remesh, pra nao engasgar ao andar.
const int CHUNKS_PER_FRAME = 1;
const int REMESH_PER_FRAME = 2;
//Alcance do raycast, em blocos.
const float REACH = 6.0f;

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

//Estatistica do ultimo frame, so pra linha de status.
int drawnChunks = 0;
int totalChunks = 0;

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

//Remesha apenas os chunks marcados como sujos.
//O scratch e reaproveitado entre chamadas pra nao realocar os vetores toda vez.
//Remesha apenas os chunks sujos, no maximo maxRemesh por chamada,
//e joga fora as meshes dos chunks que o streaming ja descarregou.
//Devolve quantos remeshou.
int updateWorld(World& world, MeshMap& meshes, MeshData& scratch, int maxRemesh)
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

    int done = 0;

    for (ChunkMap::const_iterator it = world.allChunks().begin(); it != world.allChunks().end(); ++it)
    {
        if (done >= maxRemesh)
            break;

        Chunk* chunk = it->second.get();
        if (!chunk->dirty)
            continue;

        buildChunkMesh(world, it->first, scratch);
        meshes[it->first].upload(scratch);
        chunk->dirty = false;

        done++;
    }

    return done;
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
void render(const Shader& shader, const Shader& lineShader, const Texture& texture,
    const World& world, const MeshMap& meshes, unsigned int wireVAO, const RaycastHit& hit, float aspect)
{
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 view = camera.getView();
    glm::mat4 projection = camera.getProjection(aspect);

    shader.use();
    texture.bind(0);

    //O chunk ja nasce em coordenada de mundo, entao model e identidade.
    shader.setMat4("model", glm::mat4(1.0f));
    shader.setMat4("view", view);
    shader.setMat4("projection", projection);

    //Um draw call por chunk, e so pros que estao no campo de visao.
    Frustum frustum = extractFrustum(projection * view);

    drawnChunks = 0;
    totalChunks = (int)meshes.size();

    //Agua fica pra depois, e precisa sair na ordem certa.
    static std::vector<std::pair<float, const ChunkMesh*> > waterQueue;
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

        it->second.drawSolid();
        drawnChunks++;

        if (it->second.hasWater())
        {
            glm::vec3 center((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f);
            glm::vec3 d = center - camera.position;

            waterQueue.push_back(std::make_pair(d.x * d.x + d.y * d.y + d.z * d.z, &it->second));
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
            waterQueue[i].second->drawWater();

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    drawSelection(lineShader, wireVAO, hit, view, projection);
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

    TerrainGenerator terrain(1337);
    World world;

    MeshData meshScratch;
    MeshMap meshes;

    //Nasce em pe no terreno, nao no vazio.
    Player player(glm::vec3(8.5f, (float)terrain.heightAt(8, 8) + 1.0f, 8.5f));
    camera.position = player.eyePosition();

    //Primeira carga sem limite, senao o jogador comecaria olhando pro nada.
    //Do segundo frame em diante o limite por frame entra em acao.
    double fillStart = glfwGetTime();
    ChunkPos spawn = World::chunkAt(camera.position.x, camera.position.z);
    world.streamAround(spawn, RENDER_DISTANCE, terrain, 100000);
    updateWorld(world, meshes, meshScratch, 100000);
    double fillTime = glfwGetTime() - fillStart;

    std::cout << "carga inicial: " << world.chunkCount() << " chunks em "
        << (int)(fillTime * 1000.0) << " ms | raio " << RENDER_DISTANCE
        << " chunks (" << RENDER_DISTANCE * CHUNK_SIZE << " blocos)" << std::endl;
    std::cout << "WASD anda, espaco pula, V alterna voo | esq quebra, dir coloca | 1-9 bloco | F wireframe" << std::endl;

    //O sampler le da unidade 0. Precisa ser setado uma vez, com o shader ativo.
    basicShader.use();
    basicShader.setInt("blockTexture", 0);

    //Comeca do relogio atual, nao de zero: a carga inicial levou segundos,
    //e o primeiro deltaTime seria esse tempo todo de uma vez. Com uma tecla
    //pressionada, isso teleporta o jogador dezenas de blocos no frame 1.
    float lastFrame = (float)glfwGetTime();
    float statTimer = 0.0f;
    int frames = 0;

    //Cada volta desse loop e um frame. Roda ate o ESC ou o X da janela.
    while (!glfwWindowShouldClose(window))
    {
        //1. Quanto tempo passou desde o frame anterior, em segundos.
        //   E o que deixa a velocidade igual em qualquer FPS: a 30 fps o deltaTime
        //   e o dobro do de 60 fps, entao a camera anda o dobro por frame e chega junto.
        float currentFrame = (float)glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        //2. Le o teclado e move a camera.
        //   O mouse NAO passa por aqui: ele chega sozinho pelo mouse_callback,
        //   que quem dispara e o glfwPollEvents la no passo 9.
        processInput(window, deltaTime, world, player);

        //3. Streaming: gera o que entrou no raio, descarta o que saiu,
        //   e remesha os sujos. Tudo com teto por frame.
        ChunkPos center = World::chunkAt(player.position.x, player.position.z);
        world.streamAround(center, RENDER_DISTANCE, terrain, CHUNKS_PER_FRAME);
        updateWorld(world, meshes, meshScratch, REMESH_PER_FRAME);


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
                << " | chunks " << drawnChunks << "/" << totalChunks;
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
            render(basicShader, lineShader, atlas, world, meshes, wireVAO, hit, (float)fbWidth / (float)fbHeight);

        //8. Troca o buffer de tras com o da frente. So agora o frame aparece na tela.
        glfwSwapBuffers(window);

        //9. Processa a fila de eventos do sistema: teclado, mouse, resize, botao de fechar.
        //   E aqui que o mouse_callback e o framebuffer_size_callback sao chamados.
        //   Sem isso a janela congela e o Windows marca como "nao responde".
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
