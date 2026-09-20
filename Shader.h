#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

class Shader
{
public:
    //ID do shader program linkado. Fica 0 se algo falhou.
    unsigned int ID;

    //Le, compila e linka os dois shaders a partir dos arquivos.
    Shader(const char* vertexPath, const char* fragmentPath);

    //Ativa o programa.
    void use() const;

    //Helpers de uniform.
    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setMat4(const std::string& name, const glm::mat4& value) const;

private:
    static unsigned int compile(GLenum type, const char* source, const char* label);
    static bool checkErrors(unsigned int object, const char* label, bool isProgram);
};

#endif
