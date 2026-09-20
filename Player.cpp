#include "Player.h"

#include <cmath>

static const float HALF_WIDTH = PLAYER_WIDTH * 0.5f;

//Folga usada em dois lugares: encolhe a caixa no teste de colisao e afasta
//o jogador da face ao encostar. Sem ela, um jogador exatamente encostado
//num bloco e lido como dentro dele e fica preso.
static const float EPS = 0.001f;

static const float GRAVITY = -32.0f;// -32.0f;
static const float JUMP_SPEED = 9.0f;
static const float WALK_SPEED = 4.5f;
static const float FLY_SPEED = 14.0f;
//Teto de queda, pra nao atravessar bloco por andar demais num frame so.
static const float TERMINAL_VELOCITY = 60.0f;

Player::Player(const glm::vec3& spawn)
    : position(spawn), velocity(0.0f), onGround(false), flying(false)
{
}

glm::vec3 Player::eyePosition() const
{
    return glm::vec3(position.x, position.y + PLAYER_EYE, position.z);
}

bool Player::collides(const World& world, const glm::vec3& pos) const
{
    //Encolhe um tico pra que encostar nao conte como penetrar.
    float minX = pos.x - HALF_WIDTH + EPS;
    float maxX = pos.x + HALF_WIDTH - EPS;
    float minY = pos.y + EPS;
    float maxY = pos.y + PLAYER_HEIGHT - EPS;
    float minZ = pos.z - HALF_WIDTH + EPS;
    float maxZ = pos.z + HALF_WIDTH - EPS;

    int x0 = (int)std::floor(minX);
    int x1 = (int)std::floor(maxX);
    int y0 = (int)std::floor(minY);
    int y1 = (int)std::floor(maxY);
    int z0 = (int)std::floor(minZ);
    int z1 = (int)std::floor(maxZ);

    for (int y = y0; y <= y1; y++)
    {
        for (int z = z0; z <= z1; z++)
        {
            for (int x = x0; x <= x1; x++)
            {
                BlockID id = world.getBlock(x, y, z);

                //Agua nao bloqueia: da pra nadar atravessado por enquanto.
                if (id == BLOCK_AIR || id == BLOCK_WATER)
                    continue;

                if (blockInfo(id).solid)
                    return true;
            }
        }
    }

    return false;
}

void Player::moveAxis(const World& world, int axis, float amount)
{
    if (amount == 0.0f)
        return;

    glm::vec3 next = position;
    next[axis] += amount;

    if (!collides(world, next))
    {
        position = next;
        return;
    }

    //Bateu. Em vez de so parar, encosta exatamente na face do bloco:
    //parar cru deixaria uma folga de ate um frame de movimento, e o jogador
    //ficaria flutuando um pouco acima do chao.
    if (axis == 0)
    {
        if (amount > 0.0f)
            next.x = std::floor(next.x + HALF_WIDTH) - HALF_WIDTH - EPS;
        else
            next.x = std::floor(next.x - HALF_WIDTH) + 1.0f + HALF_WIDTH + EPS;
    }
    else if (axis == 1)
    {
        if (amount > 0.0f)
        {
            //Bateu a cabeca.
            next.y = std::floor(next.y + PLAYER_HEIGHT) - PLAYER_HEIGHT - EPS;
        }
        else
        {
            //Aterrissou.
            next.y = std::floor(next.y) + 1.0f + EPS;
            onGround = true;
        }
    }
    else
    {
        if (amount > 0.0f)
            next.z = std::floor(next.z + HALF_WIDTH) - HALF_WIDTH - EPS;
        else
            next.z = std::floor(next.z - HALF_WIDTH) + 1.0f + HALF_WIDTH + EPS;
    }

    velocity[axis] = 0.0f;

    //Se ainda assim colide (canto apertado, bloco colocado em cima do
    //jogador), fica onde estava em vez de entrar na parede.
    if (!collides(world, next))
        position = next;
}

void Player::update(const World& world, const glm::vec3& wishDir, bool jump, float dt)
{
    if (flying)
    {
        velocity = wishDir * FLY_SPEED;
        onGround = false;

        moveAxis(world, 0, velocity.x * dt);
        moveAxis(world, 1, velocity.y * dt);
        moveAxis(world, 2, velocity.z * dt);

        return;
    }

    //Controle horizontal e direto: sem inercia, o movimento responde na hora.
    velocity.x = wishDir.x * WALK_SPEED;
    velocity.z = wishDir.z * WALK_SPEED;

    //So pula se os pes estao apoiados. Usa o onGround do frame anterior.
    if (jump && onGround)
    {
        velocity.y = JUMP_SPEED;
        onGround = false;
    }

    velocity.y += GRAVITY * dt;
    if (velocity.y < -TERMINAL_VELOCITY)
        velocity.y = -TERMINAL_VELOCITY;

    //O moveAxis religa isso se a descida for bloqueada.
    onGround = false;

    //Ordem importa pouco, mas resolver um eixo por vez e o que faz deslizar
    //ao raspar numa parede em vez de grudar nela.
    moveAxis(world, 0, velocity.x * dt);
    moveAxis(world, 1, velocity.y * dt);
    moveAxis(world, 2, velocity.z * dt);
}
