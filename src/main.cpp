#include "glew.h"
#include "freeglut.h"
#include "glm.hpp"
#include "ext.hpp"
#include "stb_image.h"
#include <stdlib.h>

#include <iostream>
#include <cmath>
#include <vector>

#include <conio.h>
#include "Shader_Loader.h"
#include "Render_Utils.h"
#include "Camera.h"
#include "Texture.h"
#include "Physics.h"

using namespace std;

// scene properties
namespace Objects {
    struct Properties {
        glm::vec3 size, pos;
    };

    const int numPins = 10;

    Properties sphere{ { 0.4, 0.4, 0.4 },{ -3, 0.25, 0 } },
    pins[numPins] = {
        { { 0.4, 0.4, 0.4 },{ 2, 0.0, 0 } },
        { { 0.4, 0.4, 0.4 },{ 2.5, 0.0, 0.25 } },
        { { 0.4, 0.4, 0.4 },{ 2.5, 0.0, -0.25 } },
        { { 0.4, 0.4, 0.4 },{ 3.0, 0.0, 0.5 } },
        { { 0.4, 0.4, 0.4 },{ 3.0, 0.0, -0.5 } },
        { { 0.4, 0.4, 0.4 },{ 3.0, 0.0, 0 } },
        { { 0.4, 0.4, 0.4 },{ 3.5, 0.0, 0.25 } },
        { { 0.4, 0.4, 0.4 },{ 3.5, 0.0, -0.25 } },
        { { 0.4, 0.4, 0.4 },{ 3.5, 0.0, 0.75 } },
        { { 0.4, 0.4, 0.4 },{ 3.5, 0.0, -0.75 } }
    };
}


Core::Shader_Loader shaderLoader;
GLuint programColor;
GLuint programTexture;
GLuint programSkybox;

obj::Model planeModel, sphereModel, pinModel;
GLuint pinTexture, ballTexture, groundTexture, cubemapTexture;

glm::vec3 cameraPos = glm::vec3(-10, 2, 0);
glm::vec3 cameraDir;
glm::vec3 cameraSide;
float cameraAngle = 165;
glm::mat4 cameraMatrix, perspectiveMatrix;

glm::vec3 lightDir = glm::normalize(glm::vec3(0.5, -1, -0.5));

Physics pxScene(9.8);

// fixed timestep for stable and deterministic simulation
const double physicsStepTime = 1.f / 60.f;
double physicsTimeToProcess = 0;

// physical objects
PxMaterial      *material = nullptr;
PxRigidStatic   *bodyGround = nullptr;
PxRigidDynamic* bodySphere = nullptr,
                *bodyPins[Objects::numPins] = { nullptr, nullptr, nullptr };

// renderable objects
struct Renderable {
    obj::Model *model;
    glm::mat4 localTransform, physicsTransform;
    GLuint textureId;
};
Renderable rendGround, rendSphere, rendPins[10];
vector<Renderable*> renderables;

// big cube, returns Vertex Array Object
GLuint makeBigCube()
{
    float points[] = {
                        -50.0f, 50.0f,	-50.0f, -50.0f, -50.0f, -50.0f, 50.0f,	-50.0f, -50.0f,
                        50.0f,	-50.0f, -50.0f, 50.0f,	50.0f,	-50.0f, -50.0f, 50.0f,	-50.0f,

                        -50.0f, -50.0f, 50.0f,	-50.0f, -50.0f, -50.0f, -50.0f, 50.0f,	-50.0f,
                        -50.0f, 50.0f,	-50.0f, -50.0f, 50.0f,	50.0f,	-50.0f, -50.0f, 50.0f,

                        50.0f,	-50.0f, -50.0f, 50.0f,	-50.0f, 50.0f,	50.0f,	50.0f,	50.0f,
                        50.0f,	50.0f,	50.0f,	50.0f,	50.0f,	-50.0f, 50.0f,	-50.0f, -50.0f,

                        -50.0f, -50.0f, 50.0f,	-50.0f, 50.0f,	50.0f,	50.0f,	50.0f,	50.0f,
                        50.0f,	50.0f,	50.0f,	50.0f,	-50.0f, 50.0f,	-50.0f, -50.0f, 50.0f,

                        -50.0f, 50.0f,	-50.0f, 50.0f,	50.0f,	-50.0f, 50.0f,	50.0f,	50.0f,
                        50.0f,	50.0f,	50.0f,	-50.0f, 50.0f,	50.0f,	-50.0f, 50.0f,	-50.0f,

                        -50.0f, -50.0f, -50.0f, -50.0f, -50.0f, 50.0f,	50.0f,	-50.0f, -50.0f,
                        50.0f,	-50.0f, -50.0f, -50.0f, -50.0f, 50.0f,	50.0f,	-50.0f, 50.0f
    };

    GLuint cubeVBO;
    glGenBuffers(1, &cubeVBO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, 3 * 36 * sizeof(GLfloat), &points, GL_STATIC_DRAW);

    GLuint cubeVAO;
    glGenVertexArrays(1, &cubeVAO);
    glBindVertexArray(cubeVAO);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, 3 * 36 * sizeof(GLfloat), &points, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    return cubeVAO;
}

// use stb_image to load an image file into memory, and then into one side of a cube-map texture
bool loadCubemapSide(GLuint texture, GLenum side_target, const char* file_name)
{
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

    int x, y, n;
    int force_channels = 4;
    unsigned char* image_data = stbi_load(
        file_name, &x, &y, &n, force_channels);
    if (!image_data) {
        fprintf(stderr, "ERROR: could not load %s\n", file_name);
        return false;
    }
    // non-power-of-2 dimensions check
    if ((x & (x - 1)) != 0 || (y & (y - 1)) != 0) {
        fprintf(stderr,
            "WARNING: image %s is not power-of-2 dimensions\n",
            file_name);
    }

    // copy image data into 'target' side of cube map
    glTexImage2D(
        side_target,
        0,
        GL_RGBA,
        x,
        y,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        image_data
    );
    free(image_data);

    return true;
}

// load all 6 sides of the cube-map from images, then apply formatting to the final texture
GLuint createCubeMap(
    const char* front,
    const char* back,
    const char* top,
    const char* bottom,
    const char* left,
    const char* right,
    GLuint cubemapTexture
)
{
    // generate a cube-map texture to hold all the sides
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &cubemapTexture);

    // load each image and copy into a side of the cube-map texture
    loadCubemapSide(cubemapTexture, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, front);
    loadCubemapSide(cubemapTexture, GL_TEXTURE_CUBE_MAP_POSITIVE_Z, back);
    loadCubemapSide(cubemapTexture, GL_TEXTURE_CUBE_MAP_POSITIVE_Y, top);
    loadCubemapSide(cubemapTexture, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, bottom);
    loadCubemapSide(cubemapTexture, GL_TEXTURE_CUBE_MAP_NEGATIVE_X, left);
    loadCubemapSide(cubemapTexture, GL_TEXTURE_CUBE_MAP_POSITIVE_X, right);

    // format cube map texture
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return cubemapTexture;
}

PxVec3 vertexes[1647];

void initRenderables()
{
    // load models
    pinModel = obj::loadModelFromFile("models/pin.obj");
    planeModel = obj::loadModelFromFile("models/plane.obj");
    sphereModel = obj::loadModelFromFile("models/sphere.obj");

    // load textures
    groundTexture = Core::LoadTexture("textures/wooden.jpg");
    pinTexture = Core::LoadTexture("textures/pin.jpg");
    ballTexture = Core::LoadTexture("textures/ball.jpg");
    const char* FRONT = "textures/front.jpg";
    const char* BACK = "textures/back.jpg";
    const char* TOP = "textures/top.jpg";
    const char* BOTTOM = "textures/bottom.jpg";
    const char* LEFT = "textures/right.jpg";
    const char* RIGHT = "textures/left.jpg";
    cubemapTexture = createCubeMap(RIGHT, LEFT, TOP, BOTTOM, BACK, FRONT, cubemapTexture);

    // create ground
    rendGround.model = &planeModel;
    rendGround.textureId = groundTexture;
    renderables.emplace_back(&rendGround);

    // create sphere
    rendSphere.model = &sphereModel;
    rendSphere.textureId = ballTexture;
    rendSphere.localTransform = glm::scale(Objects::sphere.size * 0.5f);
    renderables.emplace_back(&rendSphere);

    // create pins
    for (int i = 0; i < Objects::numPins; i++) {
        rendPins[i].model = &pinModel;
        rendPins[i].textureId = pinTexture;
        rendPins[i].localTransform = glm::scale(Objects::pins[i].size * 0.6f);
        renderables.emplace_back(&rendPins[i]);
    }
    float scaleMesh = 0.33f;
    int j = 0;
    for (int i = 0; i < pinModel.vertex.size(); i += 3) {
        vertexes[j]=PxVec3(pinModel.vertex[i], pinModel.vertex[i + 1], pinModel.vertex[i + 2])*scaleMesh;
        j++;
    }
}

// helper functions
PxTransform posToPxTransform(glm::vec3 const& pos) {
    return PxTransform(pos.x, pos.y, pos.z);
}
PxBoxGeometry sizeToPxBoxGeometry(glm::vec3 const& size) {
    auto h = size*0.35f;
    return PxBoxGeometry(h.x, h.y, h.z);
}

void createDynamicSphere(PxRigidDynamic* &body, Renderable *rend, glm::vec3 const& pos, float radius)
{
    body = pxScene.physics->createRigidDynamic(posToPxTransform(pos));
    PxShape* sphereShape = pxScene.physics->createShape(PxSphereGeometry(radius), *material);
    body->attachShape(*sphereShape);
    sphereShape->release();
    body->userData = rend;
    pxScene.scene->addActor(*body);
}
void createDynamicPin(PxRigidDynamic*& body, Renderable* rend, glm::vec3 const& pos, glm::vec3 const& size)
{

    PxConvexMeshDesc convexDesc;
    convexDesc.points.count = 1647;
    convexDesc.points.stride = sizeof(PxVec3);
    convexDesc.points.data = vertexes;
    convexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;
    PxDefaultMemoryOutputStream buf;
    PxConvexMeshCookingResult::Enum result;
    if (!pxScene.cooking->cookConvexMesh(convexDesc, buf, &result))
        cout << "Not possible to read";
    PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
    PxConvexMesh* convexMesh = pxScene.physics->createConvexMesh(input);

    body = pxScene.physics->createRigidDynamic(posToPxTransform(pos));
    PxShape* ConvexShape = PxRigidActorExt::createExclusiveShape(*body, PxConvexMeshGeometry(convexMesh), *material);
    body->attachShape(*ConvexShape);
    ConvexShape->release();
    body->userData = rend;
    pxScene.scene->addActor(*body);
}

void initPhysicsScene()
{
    // single common material
    material = pxScene.physics->createMaterial(0.5f, 0.5f, 0.6f);

    // create ground
    bodyGround = pxScene.physics->createRigidStatic(PxTransformFromPlaneEquation(PxPlane(0, 1, 0, 0)));
    PxShape* planeShape = pxScene.physics->createShape(PxPlaneGeometry(), *material);
    bodyGround->attachShape(*planeShape);
    planeShape->release();
    bodyGround->userData = &rendGround;
    pxScene.scene->addActor(*bodyGround);

    // create sphere
    createDynamicSphere(bodySphere, &rendSphere, Objects::sphere.pos, Objects::sphere.size.x*0.5f);
    PxRigidBodyExt::setMassAndUpdateInertia(*bodySphere, 6.0f);

    // create pins
    for (int i = 0; i < Objects::numPins; i++) {
        createDynamicPin(bodyPins[i], &rendPins[i], Objects::pins[i].pos, Objects::pins[i].size);
        PxRigidBodyExt::setMassAndUpdateInertia(*bodyPins[i], 0.4f);
    }
}

void updateTransforms()
{
    // transforms of the objects from the physical simulation.
    auto actorFlags = PxActorTypeFlag::eRIGID_DYNAMIC | PxActorTypeFlag::eRIGID_STATIC;
    PxU32 nbActors = pxScene.scene->getNbActors(actorFlags);
    if (nbActors)
    {
        vector<PxRigidActor*> actors(nbActors);
        pxScene.scene->getActors(actorFlags, (PxActor**)&actors[0], nbActors);
        for (auto actor : actors)
        {
            if (!actor->userData) continue;
            Renderable* renderable = (Renderable*)actor->userData;

            // get world matrix of the object (actor)
            PxMat44 transform = actor->getGlobalPose();
            auto& c0 = transform.column0;
            auto& c1 = transform.column1;
            auto& c2 = transform.column2;
            auto& c3 = transform.column3;

            // set up the model matrix used for the rendering
            renderable->physicsTransform = glm::mat4(
                c0.x, c0.y, c0.z, c0.w,
                c1.x, c1.y, c1.z, c1.w,
                c2.x, c2.y, c2.z, c2.w,
                c3.x, c3.y, c3.z, c3.w);
        }
    }
}


// Linear velocity for the sphere
float velocityx = 3.0;

void strikeSphere(float velx) {
    if (!bodySphere) return;
    if (velx > 0) {
        bodySphere->setLinearVelocity(PxVec3(velx, 0.f, 0.f));
    }
    
}

void moveSphere(float offset) {
    if (!bodySphere) return;
    Objects::sphere.pos.z += offset;
    bodySphere->setGlobalPose(posToPxTransform(Objects::sphere.pos));
}

void keyboard(unsigned char key, int x, int y)
{
    float angleSpeed = 0.1f;
    float sphereSpeed = 0.1f;
    float moveSpeed = 0.05f;



    switch (key)
    {
    case 'q': cameraAngle -= angleSpeed; break;
    case 'e': cameraAngle += angleSpeed; break;
    case 'w': cameraPos += cameraDir * moveSpeed; break;
    case 's': cameraPos -= cameraDir * moveSpeed; break;
    case 'd': cameraPos += cameraSide * moveSpeed; break;
    case 'a': cameraPos -= cameraSide * moveSpeed; break;
    case 'f': strikeSphere(velocityx);  break;
    case 'j': moveSphere(-moveSpeed);  break;
    case 'l': moveSphere(moveSpeed);  break;
    case 'p': velocityx +=2.0f; break;
    case 'o': velocityx -=2.0f; break;
    }
}


void mouse(int x, int y)
{
}

glm::mat4 createCameraMatrix()
{
    cameraDir = glm::normalize(glm::vec3(cosf(cameraAngle - glm::radians(90.0f)), 0, sinf(cameraAngle - glm::radians(90.0f))));
    glm::vec3 up = glm::vec3(0, 1, 0);
    cameraSide = glm::cross(cameraDir, up);

    return Core::createViewMatrix(cameraPos, cameraDir, up);
}

void drawObjectColor(obj::Model * model, glm::mat4 modelMatrix, glm::vec3 color)
{
    GLuint program = programColor;

    glUseProgram(program);

    glUniform3f(glGetUniformLocation(program, "objectColor"), color.x, color.y, color.z);
    glUniform3f(glGetUniformLocation(program, "lightDir"), lightDir.x, lightDir.y, lightDir.z);

    glm::mat4 transformation = perspectiveMatrix * cameraMatrix * modelMatrix;
    glUniformMatrix4fv(glGetUniformLocation(program, "modelViewProjectionMatrix"), 1, GL_FALSE, (float*)&transformation);
    glUniformMatrix4fv(glGetUniformLocation(program, "modelMatrix"), 1, GL_FALSE, (float*)&modelMatrix);

    Core::DrawModel(model);

    glUseProgram(0);
}

void drawObjectTexture(obj::Model * model, glm::mat4 modelMatrix, GLuint textureId)
{
    GLuint program = programTexture;

    glUseProgram(program);

    glUniform3f(glGetUniformLocation(program, "lightDir"), lightDir.x, lightDir.y, lightDir.z);
    Core::SetActiveTexture(textureId, "textureSampler", program, 0);

    glm::mat4 transformation = perspectiveMatrix * cameraMatrix * modelMatrix;
    glUniformMatrix4fv(glGetUniformLocation(program, "modelViewProjectionMatrix"), 1, GL_FALSE, (float*)&transformation);
    glUniformMatrix4fv(glGetUniformLocation(program, "modelMatrix"), 1, GL_FALSE, (float*)&modelMatrix);

    Core::DrawModel(model);

    glUseProgram(0);
}

void renderSkybox()
{
    GLuint cubeVAO = makeBigCube();

    // render a sky-box using the cube map texture
    glDepthMask(GL_FALSE);
    glUseProgram(programSkybox);

    Core::SetActiveTexture(cubemapTexture, "textureSampler", programSkybox, 0);

    glm::mat4 skyboxInitialTransformation = glm::translate(glm::vec3(0, -0.25f, 0));
    glm::mat4 skyboxMatrix = glm::translate(cameraPos + cameraDir * 0.5f) * skyboxInitialTransformation;
    glm::mat4 transformation = perspectiveMatrix * cameraMatrix * skyboxMatrix;
    glm::mat4 view = glm::mat4(glm::mat3(cameraMatrix));
    glUniformMatrix4fv(glGetUniformLocation(programSkybox, "projection"), 1, GL_FALSE, (float*)&transformation);
    glUniformMatrix4fv(glGetUniformLocation(programSkybox, "view"), 1, GL_FALSE, (float*)&view);

    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
    glDepthMask(GL_TRUE);
}

void renderScene()
{
    double time = glutGet(GLUT_ELAPSED_TIME) / 1000.0;
    static double prevTime = time;
    double dtime = time - prevTime;
    prevTime = time;

    // Update physics
    if (dtime < 1.f) {
        physicsTimeToProcess += dtime;
        while (physicsTimeToProcess > 0) {
            pxScene.step(physicsStepTime);
            physicsTimeToProcess -= physicsStepTime;
        }
    }

    // Update of camera and perspective matrices
    cameraMatrix = createCameraMatrix();
    perspectiveMatrix = Core::createPerspectiveMatrix();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.0f, 0.1f, 0.3f, 1.0f);

    // update transforms from physics simulation
    updateTransforms();

    // render models
    for (Renderable* renderable : renderables) {
        drawObjectTexture(renderable->model, renderable->physicsTransform * renderable->localTransform, renderable->textureId);
    }

    // render a sky-box using the cube map texture
    renderSkybox();

    glutSwapBuffers();
}

void init()
{
    srand(time(0));
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FRAMEBUFFER_SRGB);
    programColor = shaderLoader.CreateProgram("shaders/shader_color.vert", "shaders/shader_color.frag");
    programTexture = shaderLoader.CreateProgram("shaders/shader_tex.vert", "shaders/shader_tex.frag");
    programSkybox = shaderLoader.CreateProgram("shaders/skybox.vert", "shaders/skybox.frag");

    initRenderables();
    initPhysicsScene();
}

void shutdown()
{
    shaderLoader.DeleteProgram(programColor);
    shaderLoader.DeleteProgram(programTexture);
    shaderLoader.DeleteProgram(programSkybox);
}

void idle()
{
    glutPostRedisplay();
}

int main(int argc, char ** argv)
{
 
    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowPosition(200, 200);
    glutInitWindowSize(600, 600);
    glutCreateWindow("Bowling Simulator");
    glewInit();

    // cubemap VAO
    GLuint cube_vao = makeBigCube();

    init();
    glutKeyboardFunc(keyboard);
    glutPassiveMotionFunc(mouse);
    glutDisplayFunc(renderScene);
    glutIdleFunc(idle);

    glutMainLoop();

    shutdown();

    return 0;
}