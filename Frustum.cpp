#include "Frustum.h"

#include <cmath>

static glm::vec4 normalizePlane(const glm::vec4& p)
{
    float len = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
    if (len <= 0.0f)
        return p;

    return p / len;
}

Frustum extractFrustum(const glm::mat4& m)
{
    //glm guarda por coluna: m[coluna][linha]. A linha i e (m[0][i], m[1][i], m[2][i], m[3][i]).
    glm::vec4 row0(m[0][0], m[1][0], m[2][0], m[3][0]);
    glm::vec4 row1(m[0][1], m[1][1], m[2][1], m[3][1]);
    glm::vec4 row2(m[0][2], m[1][2], m[2][2], m[3][2]);
    glm::vec4 row3(m[0][3], m[1][3], m[2][3], m[3][3]);

    Frustum f;
    f.planes[0] = normalizePlane(row3 + row0);  // esquerda
    f.planes[1] = normalizePlane(row3 - row0);  // direita
    f.planes[2] = normalizePlane(row3 + row1);  // baixo
    f.planes[3] = normalizePlane(row3 - row1);  // cima
    f.planes[4] = normalizePlane(row3 + row2);  // perto
    f.planes[5] = normalizePlane(row3 - row2);  // longe

    return f;
}

bool aabbVisible(const Frustum& frustum, const glm::vec3& min, const glm::vec3& max)
{
    for (int i = 0; i < 6; i++)
    {
        const glm::vec4& p = frustum.planes[i];

        //Canto da caixa que esta mais longe na direcao da normal. Se ATE ELE
        //estiver atras do plano, a caixa inteira esta. Testar so esse canto
        //em vez dos 8 e o que torna o teste barato.
        glm::vec3 positive(
            (p.x >= 0.0f) ? max.x : min.x,
            (p.y >= 0.0f) ? max.y : min.y,
            (p.z >= 0.0f) ? max.z : min.z);

        if (p.x * positive.x + p.y * positive.y + p.z * positive.z + p.w < 0.0f)
            return false;
    }

    return true;
}
