//A implementacao do stb_image vive aqui, num unico .cpp do projeto.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Texture.h"

#include <iostream>

Texture::Texture(const char* path)
    : ID(0), width(0), height(0)
{
    //PNG tem origem no topo, OpenGL no rodape. Sem isso a textura sai de cabeca pra baixo.
    stbi_set_flip_vertically_on_load(true);

    int channels = 0;
    unsigned char* data = stbi_load(path, &width, &height, &channels, 0);
    if (data == NULL)
    {
        std::cout << "ERROR::TEXTURE::FILE_NOT_READ: " << path << std::endl;
        return;
    }

    GLenum format = GL_RGB;
    if (channels == 1)
        format = GL_RED;
    else if (channels == 4)
        format = GL_RGBA;

    glGenTextures(1, &ID);
    glBindTexture(GL_TEXTURE_2D, ID);

    //Evita lixo em imagens cuja linha nao e multipla de 4 bytes.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

    //GL_NEAREST mantem o pixel quadrado em vez de borrar. Sem mipmap por enquanto:
    //o MIN_FILTER padrao usa mipmap e deixaria a textura incompleta (preta).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(data);
}

void Texture::bind(unsigned int unit) const
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, ID);
}

glm::vec4 atlasUV(int tileIndex)
{
    int col = tileIndex % ATLAS_GRID;
    int row = tileIndex / ATLAS_GRID;

    float step = 1.0f / (float)ATLAS_GRID;

    float u0 = col * step;
    float u1 = u0 + step;

    //A textura carrega com flip vertical, entao a linha 0 do PNG (a de cima)
    //cai no V mais alto. Por isso o V e invertido aqui.
    float v1 = 1.0f - row * step;
    float v0 = v1 - step;

    return glm::vec4(u0, v0, u1, v1);
}
