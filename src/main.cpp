#include <cassert>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <deque>
#include <memory>
#include <random>

#include "glad/glad.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/io.hpp>

const float SPRAY_WIDTH = 0.4f;
const float PELICAN_WIDTH = 0.2f;
const float WAVE_SPEED = 0.4f;
const float BOAT_POS_X = 0.1f;
const float BOAT_WIDTH = 0.2f;
const float SEA_LEVEL = 0.8f;
const float GRAVITY = 0.0009f;

void gladPostCallback(const char* name, void*, int, ...) {
    const auto code = glad_glGetError();
    if (code != GL_NO_ERROR) {
        std::cerr << "glad: " << name << " " << code << std::endl;
    }
}

class Shader {
public:
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&&) = default;

    Shader(const std::string& filename, GLenum type) : type_(type) {
        assert(type == GL_VERTEX_SHADER
                || type == GL_FRAGMENT_SHADER
                || type == GL_GEOMETRY_SHADER);

        std::ifstream ifs(filename.c_str(), std::ios::in);
        std::ostringstream ss;
        ss << ifs.rdbuf();
        const auto source = ss.str();

        const auto str = source.c_str();
        const auto length = static_cast<GLint>(source.size());

        id_ = glCreateShader(type);
        glShaderSource(id_, 1, &str, &length);
        glCompileShader(id_);

        GLint infoLogLength;
        glGetShaderiv(id_, GL_INFO_LOG_LENGTH, &infoLogLength);

        if (infoLogLength > 0) {
            std::vector<GLchar> infoLog(infoLogLength);
            glGetShaderInfoLog(id_, infoLogLength, nullptr, infoLog.data());

            std::cerr << "Shader: " << infoLog.data() << std::endl;
        }

        GLint compileStatus;
        glGetShaderiv(id_, GL_COMPILE_STATUS, &compileStatus);
        assert(compileStatus == GL_TRUE);
    }

    virtual ~Shader() {
        glDeleteShader(id_);
    }

    GLuint getId() const {
        return id_;
    }

private:
    GLuint id_;
    GLenum type_;
};

class ShaderProgram {
public:
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&&) = default;

    ShaderProgram(const std::vector<std::shared_ptr<Shader>>& shaders) {
        id_ = glCreateProgram();
        for (const auto& shader : shaders) {
            glAttachShader(id_, shader->getId());
        }
        glLinkProgram(id_);
        for (const auto& shader : shaders) {
            glDetachShader(id_, shader->getId());
        }

        GLint infoLogLength;
        glGetProgramiv(id_, GL_INFO_LOG_LENGTH, &infoLogLength);

        if (infoLogLength > 0) {
            std::vector<GLchar> infoLog(infoLogLength);
            glGetProgramInfoLog(id_, infoLogLength, nullptr, infoLog.data());

            std::cerr << "ShaderProgram: " << infoLog.data() << std::endl;
        }

        GLint linkStatus;
        glGetProgramiv(id_, GL_LINK_STATUS, &linkStatus);
        assert(linkStatus == GL_TRUE);
    }

    void use() const {
        glUseProgram(id_);
    }

    void setUniform(const char* name, GLuint value) const {
        glUniform1i(glGetUniformLocation(id_, name), value);
    }

    void setUniform(const char* name, const glm::fvec2& value) const {
        glUniform2fv(glGetUniformLocation(id_, name), 1, glm::value_ptr(value));
    }

    void setUniform(const char* name, const glm::fvec4& value) const {
        glUniform4fv(glGetUniformLocation(id_, name), 1, glm::value_ptr(value));
    }

private:
    GLuint id_;
};

class Texture {
public:
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) = default;

    Texture(const std::string& filename) {
        int numComponents;
        const auto data = stbi_load(filename.c_str(), &width_, &height_, &numComponents, STBI_rgb_alpha);
        assert(data);
        assert(numComponents == 4);

        glGenTextures(1, &id_);
        glBindTexture(GL_TEXTURE_2D, id_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

        stbi_image_free(data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    virtual ~Texture() {
        glDeleteTextures(1, &id_);
    }

    void bind(unsigned int textureUnit) const {
        assert(textureUnit < 32);

        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_2D, id_);
    }

    int getWidth() const {
        return width_;
    }

    int getHeight() const {
        return height_;
    }

private:
    GLuint id_;
    int width_, height_;
};

class ShaderProgramStore {
public:
    ShaderProgramStore() :
        spriteVert_(std::make_shared<Shader>("shaders/sprite.vert", GL_VERTEX_SHADER)),
        spriteGeom_(std::make_shared<Shader>("shaders/sprite.geom", GL_GEOMETRY_SHADER)),
        texFrag_(std::make_shared<Shader>("shaders/tex.frag", GL_FRAGMENT_SHADER)),
        spriteProg_({spriteVert_, spriteGeom_, texFrag_}) {}

    static ShaderProgramStore& getInstance() {
        static ShaderProgramStore instance;
        return instance;
    }

    const ShaderProgram& getSpriteProgram() const {
        return spriteProg_;
    }

private:
    const std::shared_ptr<Shader> spriteVert_, spriteGeom_, texFrag_;
    const ShaderProgram spriteProg_;
};

class Sprite {
public:
    Sprite(const std::string& filename, const glm::vec2& size) :
        texture_(filename),
        size_(size) {}

    void setPos(const glm::vec2& pos) {
        pos_ = pos;
    }

    const glm::vec2& getPos() const {
        return pos_;
    }

    const glm::vec2& getSize() const {
        return size_;
    }

    const Texture& getTexture() const {
        return texture_;
    }

    void draw() const {
        const auto& spriteProg = ShaderProgramStore::getInstance().getSpriteProgram();
        texture_.bind(0);
        spriteProg.use();
        spriteProg.setUniform("pos", pos_);
        spriteProg.setUniform("size", size_);
        spriteProg.setUniform("tex", 0);
        glDrawArrays(GL_POINTS, 0, 1);
    }

private:
    const Texture texture_;
    const glm::vec2 size_;
    glm::vec2 pos_;
};

class SpriteStore {
public:
    SpriteStore() :
        spraySprite_("spray.png", {SPRAY_WIDTH, 0.5f}),
        pelicanSprites_{
            std::make_shared<Sprite>("pelican0.png", glm::vec2(PELICAN_WIDTH, 0.2f)),
            std::make_shared<Sprite>("pelican1.png", glm::vec2(PELICAN_WIDTH, 0.33f))
        } {}

    static SpriteStore& getInstance() {
        static SpriteStore instance;
        return instance;
    }

    Sprite& getSpraySprite() {
        return spraySprite_;
    }

    const std::vector<std::shared_ptr<Sprite>>& getPelicanSprites() const {
        return pelicanSprites_;
    }

private:
    Sprite spraySprite_;
    const std::vector<std::shared_ptr<Sprite>> pelicanSprites_;
};

class Object {
public:
    Object(double spawnTime) :
        spawnTime_(spawnTime),
        visible_(true) {}

    bool isVisible() const {
        return visible_;
    }

    double getSpawnTime() const {
        return spawnTime_;
    }

    virtual void update(double t) = 0;
    virtual void draw() const = 0;
    virtual bool hit(float boatPosY) const = 0;

protected:
    double spawnTime_;
    glm::vec2 pos_;
    bool visible_;
};

class Spray : public Object {
public:
    using Object::Object;

    void update(double t) override {
        pos_ = {0.9f - WAVE_SPEED * (t - spawnTime_), 0.75f + 0.25 * std::cos(2 * (t - spawnTime_))};
        visible_ = t <= spawnTime_ + glm::pi<double>();
    }

    void draw() const override {
        auto& sprite = SpriteStore::getInstance().getSpraySprite();
        sprite.setPos(pos_);
        sprite.draw();
    }

    bool hit(float boatPosY) const override {
        return  BOAT_POS_X + BOAT_WIDTH > pos_.x + 0.5 * SPRAY_WIDTH
            && BOAT_POS_X < pos_.x + SPRAY_WIDTH
            && boatPosY > pos_.y;
    }
};

class Pelican : public Object {
public:
    Pelican(double spawnTime) :
        Object(spawnTime), 
        animIndex_(0) {}

    void update(double t) override {
        pos_ = {1.f - 0.8f * (t - spawnTime_), 0.05f};
        visible_ = pos_.x >= -PELICAN_WIDTH;
        animIndex_ = static_cast<int>((t - spawnTime_) / 0.25) % 2;
    }

    void draw() const override {
        const auto& sprite = SpriteStore::getInstance().getPelicanSprites().at(animIndex_);
        sprite->setPos(pos_);
        sprite->draw();

    }

    bool hit(float boatPosY) const override {
        return  BOAT_POS_X + BOAT_WIDTH > pos_.x
            && BOAT_POS_X < pos_.x + PELICAN_WIDTH
            && boatPosY - 0.2f < pos_.y + 0.2f;
    }

private:
    int animIndex_;
};

int main() {
    std::atexit(glfwTerminate);
    assert(glfwInit());
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    const auto window = glfwCreateWindow(1280, 720, "Wave", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetWindowAspectRatio(window, 16, 9);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int width, int height) {
        glViewport(0, 0, width, height);
    });

    glfwSwapInterval(1);
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);

    assert(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)));
    glad_set_post_callback(gladPostCallback);

    GLuint vertexArray;
    glGenVertexArrays(1, &vertexArray);
    glBindVertexArray(vertexArray);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.627f, 0.847f, 0.937f, 1.f);

    Sprite waveBaseSprite("wave_base.png", {1.f, 0.3f});
    Sprite boatSprite("boat.png", {BOAT_WIDTH, 0.4f});
    Sprite gameOverSprite("game_over.png", {0.5f, 0.5f});
    gameOverSprite.setPos((glm::vec2(1.f, 1.f) - gameOverSprite.getSize()) / 2.f);

    float boatPosY = SEA_LEVEL;
    float boatVelY = 0;
    bool grounded = true;

    std::deque<std::shared_ptr<Object>> objects;

    std::random_device randDevice;
    std::mt19937 randEngine(randDevice());
    std::uniform_real_distribution<double> intervalDist(1.0, 3.0);
    std::bernoulli_distribution typeDist;
    double interval = glfwGetTime();

    bool gameover = false;

    double time = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        const bool retryKeyPressed = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
        if (gameover) {
            if (retryKeyPressed) {
                gameover = false;
                objects.clear();
                boatPosY = SEA_LEVEL;
                boatVelY = 0;
                grounded = true;
            }
        } else {
            time = glfwGetTime();

            const bool jumpKeyPressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
            if (grounded) {
                if (jumpKeyPressed) {
                    boatVelY -= 0.03f;
                    grounded = false;
                }
            } else if (boatPosY > SEA_LEVEL) {
                grounded = true;
                boatPosY = SEA_LEVEL;
                boatVelY = 0.f;
            } else {
                boatPosY += boatVelY;
                boatVelY += GRAVITY;
            }

            for (const auto& object : objects) {
                if (object->hit(boatPosY)) {
                    gameover = true;
                    break;
                }
            }

            while (!objects.empty() && !objects.front()->isVisible()) {
                objects.pop_front();
            }

            if (objects.empty() || time > objects.back()->getSpawnTime() + interval) {
                if (typeDist(randEngine)) {
                    objects.emplace_back(std::make_shared<Spray>(time));
                } else {
                    objects.emplace_back(std::make_shared<Pelican>(time));
                }
                interval = intervalDist(randEngine);
            }

            for (const auto& object : objects) {
                object->update(time);
            }
        }

        glClear(GL_COLOR_BUFFER_BIT);

        float x;
        const float wavePos = -std::modf(WAVE_SPEED * time, &x);

        for (int i = 0; i < 2; ++i) {
            waveBaseSprite.setPos({wavePos + i, SEA_LEVEL + 0.05 * std::sin(3 * time)});
            waveBaseSprite.draw();
        }

        boatSprite.setPos({BOAT_POS_X, boatPosY - 0.3f + 0.05 * std::sin(3 * time)});
        boatSprite.draw();

        for (const auto& object : objects) {
            object->draw();
        }

        if (gameover) {
            gameOverSprite.draw();
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    return EXIT_SUCCESS;
}
