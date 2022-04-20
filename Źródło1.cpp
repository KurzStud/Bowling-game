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


pinModel = obj::loadModelFromFile("models/pin.obj");
cout << pinModel.vertex.size() / 3;
