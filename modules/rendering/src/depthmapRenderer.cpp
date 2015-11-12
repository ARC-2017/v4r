#include <v4r/rendering/depthmapRenderer.h>

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>
#include <GL/gl.h>

namespace v4r
{

bool DepthmapRenderer::glfwRunning=false;



int DepthmapRenderer::search_midpoint(int &index_start, int &index_end, unsigned &n_vertices, int &edge_walk,
    std::vector<int> &midpoint, std::vector<int> &start, std::vector<int> &end, std::vector<float> &vertices)
{
  int i;
  for (i = 0; i < edge_walk; i++)
    if ((start[i] == index_start && end[i] == index_end) || (start[i] == index_end && end[i] == index_start)) {
      int res = midpoint[i];

      /* update the arrays */
      start[i] = start[edge_walk - 1];
      end[i] = end[edge_walk - 1];
      midpoint[i] = midpoint[edge_walk - 1];
      edge_walk--;

      return res;
    }

  /* vertex not in the list, so we add it */
  start[edge_walk] = index_start;
  end[edge_walk] = index_end;
  midpoint[edge_walk] = n_vertices;

  /* create new vertex */
  vertices[3 * n_vertices] = (vertices[3 * index_start] + vertices[3 * index_end]) / 2.0f;
  vertices[3 * n_vertices + 1] = (vertices[3 * index_start + 1] + vertices[3 * index_end + 1]) / 2.0f;
  vertices[3 * n_vertices + 2] = (vertices[3 * index_start + 2] + vertices[3 * index_end + 2]) / 2.0f;

  /* normalize the new vertex */
  float length = sqrt(
      vertices[3 * n_vertices] * vertices[3 * n_vertices] + vertices[3 * n_vertices + 1] * vertices[3 * n_vertices + 1]
          + vertices[3 * n_vertices + 2] * vertices[3 * n_vertices + 2]);
  length = 1 / length;
  vertices[3 * n_vertices] *= length;
  vertices[3 * n_vertices + 1] *= length;
  vertices[3 * n_vertices + 2] *= length;

  n_vertices++;
  edge_walk++;
  return midpoint[edge_walk - 1];
}

void DepthmapRenderer::subdivide(unsigned &n_vertices, unsigned &n_edges, unsigned &n_faces, std::vector<float> &vertices, std::vector<int> &faces)
{
    //Code i stole from thomas mörwald:
    int n_vertices_new = n_vertices + 2 * n_edges;
    int n_faces_new = 4 * n_faces;
    unsigned i;

    int edge_walk = 0;
    n_edges = 2 * n_vertices + 3 * n_faces;

    std::vector<int> start(n_edges);
    std::vector<int> end(n_edges);
    std::vector<int> midpoint(n_edges);

    std::vector<int> faces_old = faces;
    vertices.resize(3 * n_vertices_new);
    faces.resize(3 * n_faces_new);
    n_faces_new = 0;

    for (i = 0; i < n_faces; i++) {
      int a = faces_old[3 * i];
      int b = faces_old[3 * i + 1];
      int c = faces_old[3 * i + 2];

      int ab_midpoint = search_midpoint(b, a, n_vertices, edge_walk, midpoint, start, end, vertices);
      int bc_midpoint = search_midpoint(c, b, n_vertices, edge_walk, midpoint, start, end, vertices);
      int ca_midpoint = search_midpoint(a, c, n_vertices, edge_walk, midpoint, start, end, vertices);

      faces[3 * n_faces_new] = a;
      faces[3 * n_faces_new + 1] = ab_midpoint;
      faces[3 * n_faces_new + 2] = ca_midpoint;
      n_faces_new++;
      faces[3 * n_faces_new] = ca_midpoint;
      faces[3 * n_faces_new + 1] = ab_midpoint;
      faces[3 * n_faces_new + 2] = bc_midpoint;
      n_faces_new++;
      faces[3 * n_faces_new] = ca_midpoint;
      faces[3 * n_faces_new + 1] = bc_midpoint;
      faces[3 * n_faces_new + 2] = c;
      n_faces_new++;
      faces[3 * n_faces_new] = ab_midpoint;
      faces[3 * n_faces_new + 1] = b;
      faces[3 * n_faces_new + 2] = bc_midpoint;
      n_faces_new++;
    }
    n_faces = n_faces_new;
}

DepthmapRenderer::DepthmapRenderer(int resx, int resy)
{
    //First of all: create opengl context:
    res=glm::ivec2(resx,resy);
    //init glfw if it is not running already
    if (!DepthmapRenderer::glfwRunning) {
        glfwInit();

        DepthmapRenderer::glfwRunning=true;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
    context = glfwCreateWindow(640 , 480, "", NULL, 0);

    glfwWindowHint(GLFW_VISIBLE, GL_TRUE);
    glfwMakeContextCurrent(context);

    glewExperimental = GL_TRUE;
    GLenum err=glewInit();

    if(err!=GLEW_OK){
        std::stringstream s; s << "glewInit failed, aborting. " << err;
        throw std::runtime_error(s.str());
    }

    glGetError();
    //create framebuffer:

    //create shader:
    const char *vertex=
            "#version 450 \n\
            in vec4 pos;\n\
            out vec4 colorIn;\n\
            void main(){\n\
               gl_Position=vec4(pos.xyz,1);\n\
colorIn=vec4(1,0.5,0.5,1);\n\
colorIn=unpackUnorm4x8(floatBitsToUint(pos.w));\n\
            }";
    const char *geometry=
            "#version 450\n\
            \
            layout(binding=0, offset=0) uniform atomic_uint faceCount;\n\
            //layout(binding=0, offset=4) uniform atomic_uint backFacing;\n\
            layout(std430,binding=1) buffer Buffer{\n\
                vec2 AnPixCnt[];\n\
            };\n\
            layout (triangles) in;//triangles\n\
            layout(triangle_strip,max_vertices=3) out;\n\
            noperspective out float z;\n\
            in vec4 colorIn[];\n\
            flat out unsigned int index;\n\
            out vec4 color;\n\
            uniform vec4 projection;\n\
            uniform mat4 transformation;\n\
            uniform ivec2 viewportRes;\n\
            vec4 project(vec4 pos){\n\
                return vec4((pos.x*projection.x/pos.z+projection.z)*2.0-1.0,(pos.y*projection.y/pos.z+projection.w)*2.0-1.0,0.1/pos.z,1);//max draw distance is 10 m\n\
            }\n\
            void main(){\
                unsigned int ind = atomicCounterIncrement(faceCount);\n\
                index= ind+1;\n\
                gl_Position=project(transformation*gl_in[0].gl_Position);\n\
                vec4 p1=transformation*gl_in[0].gl_Position;\n\
                vec2 pp1=gl_Position.xy;\n\
                z=-(transformation*gl_in[0].gl_Position).z;\n\
                color=colorIn[0];\n\
                EmitVertex();\n\
                gl_Position=project(transformation*gl_in[1].gl_Position);\n\
                vec2 pp2=gl_Position.xy;\n\
                vec4 p2=transformation*gl_in[1].gl_Position;\n\
                z=-(transformation*gl_in[1].gl_Position).z;\n\
color=colorIn[1];\n\
                EmitVertex();\n\
                gl_Position=project(transformation*gl_in[2].gl_Position);\n\
                vec4 p3=transformation*gl_in[2].gl_Position;\n\
                vec2 pp3=gl_Position.xy;\n\
                z=-(transformation*gl_in[2].gl_Position).z;\n\
color=colorIn[2];\n\
                EmitVertex();\n\
                //calc triangle surface area\n\
                float A= length(cross(vec3(p1)/p1.w-vec3(p3)/p3.w,vec3(p2)/p2.w-vec3(p3)/p3.w));//TODO: Change this to correct pixel area calculation\n\
                vec3 a=vec3((pp2.x-pp1.x)*float(viewportRes.x),(pp2.y-pp1.y)*float(viewportRes.y),0)*0.5;\n\
                vec3 b=vec3((pp3.x-pp1.x)*float(viewportRes.x),(pp3.y-pp1.y)*float(viewportRes.y),0)*0.5;\n\
                float Apix=length(cross(a,b))*0.5;\n\
//A=1;\n\
//Apix=1;\n\
                AnPixCnt[ind]=vec2(A,Apix);\n\
               /*if(cross(vec3(p1)/p1.w-vec3(p3)/p3.w,vec3(p2)/p2.w-vec3(p3)/p3.w).z>0){\n\
                    atomicCounterIncrement(frontFacing);\n\
                }else{\n\
                    atomicCounterIncrement(backFacing);\n\
                }*/\n\
            }";
    const char *fragment=
            "#version 450 \n\
            \
            noperspective in float z;\n\
            flat in unsigned int index;\n\
            in vec4 color;\n\
            out vec4 depthOutput;\n\
            out vec4 colorOutput;\n\
            out unsigned int indexOutput;\n\
            \n\
            uniform float zNear;\n\
            uniform float zFar;\n\
            void main(){\n\
            \
               depthOutput=vec4(z,z,z,1);\n\
               depthOutput=vec4(vec3(-0.1/((gl_FragCoord.z/gl_FragCoord.w)*2.0-1.0)),1);\n\
               //depthOutput=vec4(0.1,z,z,1);\n\
               indexOutput=index;//uintBitsToFloat(1234);\n\
               colorOutput=color;\n\
            }";
    //load and compile shader:
    std::cout << "compiling vertex shader:" << std::endl;
    GLuint vertexShader =glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader,1,(const GLchar**)&vertex,NULL);
    glCompileShader(vertexShader);
    GLint status;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &status);
    char buffer[512];
    glGetShaderInfoLog(vertexShader, 512, NULL, buffer);
    std::cout << buffer << std::endl;

    std::cout << "compiling geometry shader:" << std::endl;
    GLuint geometryShader =glCreateShader(GL_GEOMETRY_SHADER);
    glShaderSource(geometryShader,1,(const GLchar**)&geometry,NULL);
    glCompileShader(geometryShader);

    glGetShaderiv(geometryShader, GL_COMPILE_STATUS, &status);
    glGetShaderInfoLog(geometryShader, 512, NULL, buffer);
    std::cout << buffer << std::endl;


    std::cout << "compiling fragment shader:" << std::endl;
    GLuint fragmentShader =glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader,1,(const GLchar**)&fragment,NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &status);
    glGetShaderInfoLog(fragmentShader, 512, NULL, buffer);
    std::cout << buffer << std::endl;

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, geometryShader);
    glAttachShader(shaderProgram, fragmentShader);

    glBindFragDataLocation(shaderProgram, 0, "depthOutput");
    glBindFragDataLocation(shaderProgram, 1, "indexOutput");
    glBindFragDataLocation(shaderProgram, 2, "colorOutput");
    if(glGetError()){
        std::cout << "something went wrong 0.2" << std::endl;
    }

    glLinkProgram(shaderProgram);

    glGetProgramiv( shaderProgram, GL_LINK_STATUS, &status);
    if( GL_FALSE == status ) {
        // Store log and return false
        int length = 0;
        std::string logString;

        glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &length );

        if( length > 0 ) {
            char * c_log = new char[length];
            int written = 0;
            glGetProgramInfoLog(shaderProgram, length, &written, c_log);
            logString = c_log;
            std::cout << logString << std::endl;
            delete [] c_log;
        }
    }



    if(glGetError()){
        std::cout << "something went wrong 0.3" << std::endl;
    }

    projectionUniform=glGetUniformLocation(shaderProgram,"projection");

    if(glGetError()){
        std::cout << "something went wrong 0.4" << std::endl;
    }

    poseUniform = glGetUniformLocation(shaderProgram,"transformation");


    viewportResUniform = glGetUniformLocation(shaderProgram,"viewportRes");

    if(glGetError()){
        std::cout << "something went wrong 1" << std::endl;
    }

    //TODO: get the position attribute
    posAttribute=glGetAttribLocation(shaderProgram,"pos");

    if(glGetError()){
        std::cout << "something went wrong" << std::endl;
    }


    if(glGetError()){
        std::cout << "Something wrong with opengl3" << std::endl;
    }

    //generate framebuffer:

    glGenFramebuffers(1,&FBO);
    glBindFramebuffer(GL_FRAMEBUFFER,FBO);


    if(glGetError()){
        std::cout << "Something wrong with opengl2.5" << std::endl;
    }


    glGenTextures(1,&depthTex);
    glBindTexture(GL_TEXTURE_2D,depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0,GL_R32F, resx, resy, 0,GL_RED, GL_FLOAT, 0);



    glGenTextures(1,&indexTex);
    glBindTexture(GL_TEXTURE_2D,indexTex);
    glTexImage2D(GL_TEXTURE_2D, 0,GL_R32UI, resx, resy, 0,GL_RED_INTEGER, GL_UNSIGNED_INT, 0);


    glGenTextures(1,&colorTex);
    glBindTexture(GL_TEXTURE_2D,colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA8, resx, resy, 0,GL_RGBA, GL_UNSIGNED_BYTE, 0);



    glGenRenderbuffers(1, &zBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, zBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, resx, resy);//GL_DEPTH_COMPONENT_32F //without the 32F?
    //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrenderbuffer);

    if(glGetError()){
        std::cout << "Something wrong with opengl2.1" << std::endl;
    }

    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,zBuffer);
    GLuint buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1,GL_COLOR_ATTACHMENT2};//last one is for debug
    glDrawBuffers(3,buffers);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,depthTex,0);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,indexTex,0);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT2,GL_TEXTURE_2D,colorTex,0);


    if(glGetError()){
        std::cout << "Something wrong with opengl2" << std::endl;
    }


    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
        std::cout << "Framebuffer not complete" << std::endl;
    }
    glViewport(0,0,resx,resy);

    if(glGetError()){
        std::cout << "Something wrong with opengl1" << std::endl;
    }

    //two atomic counters to count thte front and backfacing triangles
    glGenBuffers(1, &atomicCounterBuffer);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBuffer);
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint)*2, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomicCounterBuffer);



    glGenBuffers(1,&SSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,SSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec2)*maxMeshSize, 0, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, SSBO);

    /*glGenBuffers(1, &backFacingAtomicBuffer);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, backFacingAtomicBuffer);
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), NULL, GL_DYNAMIC_DRAW);*/



    glGenVertexArrays(1,&VAO);
    /*glBindVertexArray(VAO);

    //add SSBO  to vertex array?
    glBindVertexArray(0);*/

    if(glGetError()){
        std::cout << "Something wrong with opengl" << std::endl;
    }



}

DepthmapRenderer::~DepthmapRenderer()
{
    /*VERY VERY BIG TODO!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
    //TODO: destruction and stuff


    glDeleteTextures(1,&depthTex);
    glDeleteBuffers(1,&zBuffer);
}

std::vector<Eigen::Vector3f> DepthmapRenderer::createSphere(float r, int subdivisions)
{

    std::vector<Eigen::Vector3f> result;
    int i;

    float t = (1 + sqrt(5.0f)) / 2;
    float tau = t / sqrt(1 + t * t);
    float one = 1 / sqrt(1 + t * t);

    float icosahedron_vertices[] = { tau, one, 0.0, -tau, one, 0.0, -tau, -one, 0.0, tau, -one, 0.0, one, 0.0, tau, one, 0.0, -tau,
        -one, 0.0, -tau, -one, 0.0, tau, 0.0, tau, one, 0.0, -tau, one, 0.0, -tau, -one, 0.0, tau, -one };
    int icosahedron_faces[] = { 4, 8, 7, 4, 7, 9, 5, 6, 11, 5, 10, 6, 0, 4, 3, 0, 3, 5, 2, 7, 1, 2, 1, 6, 8, 0, 11, 8, 11, 1, 9,
        10, 3, 9, 2, 10, 8, 4, 0, 11, 0, 5, 4, 9, 3, 5, 3, 10, 7, 8, 1, 6, 1, 11, 7, 2, 9, 6, 10, 2 };

    unsigned int n_vertices = 12;
    unsigned int n_faces = 20;
    unsigned int n_edges = 30;


    std::vector<float> vertices;
    std::vector<int> faces;
    for (unsigned int i = 0; i < (3 * n_vertices); i++)
      vertices.push_back(icosahedron_vertices[i]);
    for (unsigned i = 0; i < (3 * n_faces); i++)
      faces.push_back(icosahedron_faces[i]);



    for (i = 0; i < subdivisions; i++)
      subdivide(n_vertices, n_edges, n_faces, vertices, faces);

    // Copy vertices
    for (i = 0; i < n_vertices; i++) {
        Eigen::Vector3f v;
        v[0] = r * vertices[3 * i + 0];
        v[1] = r * vertices[3 * i + 1];
        v[2] = r * vertices[3 * i + 2];
        result.push_back(v);
    }

    return result;

}

void DepthmapRenderer::setIntrinsics(float fx, float fy, float cx, float cy)
{
    //set and calculate projection matrix
    fxycxy=glm::vec4(fx,fy,cx,cy);
}

void DepthmapRenderer::setModel(DepthmapRendererModel *model)
{
    this->model=model;

    //bind shader:
    glBindVertexArray(VAO);
    glUseProgram(shaderProgram);
    this->model->loadToGPU();

    //set vertexAttribArray
    glEnableVertexAttribArray(posAttribute);
    glVertexAttribPointer(posAttribute,4,GL_FLOAT,GL_FALSE,sizeof(glm::vec4),0);
    glBindVertexArray(0);
    //maybe upload it to
}

Eigen::Matrix4f DepthmapRenderer::getPoseLookingToCenterFrom(Eigen::Vector3f position)
{
    glm::vec3 up(0,0,1);
    if(position[0]==0 && position[1]==0){
        up=glm::vec3(1,0,0);
    }
    glm::vec3 pos(position[0],position[1],position[2]);
    glm::vec3 center(0,0,0);

    glm::mat4 pose=glm::lookAt(pos,center,up);
    //transform to Eigen Matrix type
    Eigen::Matrix4f ePose;
    for(int i=0;i<4;i++){
        for(int j=0;j<4;j++){
            ePose(i,j)=pose[i][j];
        }
    }
    return ePose;
}

void DepthmapRenderer::setCamPose(Eigen::Matrix4f pose)
{
    glm::mat4 gPose;
    for(int i=0;i<4;i++){
        for(int j=0;j<4;j++){
            gPose[i][j]=pose(i,j);
        }
    }
    this->pose=gPose;
}



cv::Mat DepthmapRenderer::renderDepthmap(float &visible,cv::Mat &color)
{
    //load shader:
    glUseProgram(shaderProgram);
    //set uniforms:
   // glm::mat4 emptyPose;
    glUniformMatrix4fv(poseUniform,1,GL_FALSE,(float*)&pose);
    glUniform4f(projectionUniform,fxycxy.x/(float)res.x,fxycxy.y/(float)res.y,fxycxy.z/(float)res.x,fxycxy.w/(float)res.y);
    glUniform2i(viewportResUniform,res.x,res.y);
    //glm::mat4 projection=projectionMatrixFromIntrinsics(fxycxy);//right now this is empty
    //glUniformMatrix4fv(projectionUniform,1,GL_FALSE,(float*)&projection);

    glm::vec4 test(fxycxy.x/(float)res.x,fxycxy.y/(float)res.y,fxycxy.z/(float)res.x,fxycxy.w/(float)res.y);


    //use vertex array object:
    glBindVertexArray(VAO);



    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,1,  SSBO);


    //activate fbo
    glBindFramebuffer(GL_FRAMEBUFFER,FBO);
/*
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,zBuffer);
    GLuint buffers[]={GL_COLOR_ATTACHMENT0};//last one is for debug
    glDrawBuffers(1,buffers);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);
*/
    glViewport(0,0,res.x,res.y);
    glClearColor(0.0,0.0,0.0,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);


    //setup the atomic variables for counting the triangles
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBuffer);
    GLuint* ptr = (GLuint*)glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint),
                                            GL_MAP_WRITE_BIT |
                                            GL_MAP_INVALIDATE_BUFFER_BIT |
                                            GL_MAP_UNSYNCHRONIZED_BIT);
    ptr[0] = 0;
    glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
    //glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);


    if(glGetError()){
        std::cout << "Something wrong with opengl at rendering 1" << std::endl;
    }

    //disable culling
    glFrontFace(GL_FRONT_AND_BACK);
    GLenum err = glGetError();
    if( err != GL_NO_ERROR){
        std::cerr << "Something wrong with opengl at rendering0.435 (" << err << ")" << std::endl;
    }

    //render
    glDrawElements(
        GL_TRIANGLES,      // mode
        model->getIndexCount(),    // count
        GL_UNSIGNED_INT,   // type
        (void*)0           // element array buffer offset
    );
    err = glGetError();
    if( err != GL_NO_ERROR){
        std::cerr << "Something wrong with opengl at rendering0.qwerw (" << err << ")" << std::endl;
    }

    glFinish();
    err = glGetError();
    if( err != GL_NO_ERROR){
        std::cerr << "Something wrong with opengl at rendering0.9 (" << err << ")" << std::endl;
    }
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_SHADER_STORAGE_BARRIER_BIT);//not helping either
    //read buffers:
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBuffer);
    ptr = (GLuint*)glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint)*2,
                                            GL_MAP_READ_BIT );

    //std::cout << ptr[0] << " visible triangles and " << ptr[1] << " invisible triangles" << std::endl;
    unsigned int faceCount=ptr[0];
    glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

    std::cout << "face count " << faceCount << std::endl;
    //download fbo

    //GET DEPTH TEXTURE
    cv::Mat depthmap(res.y,res.x,CV_32FC1);//FC1
    glBindTexture(GL_TEXTURE_2D,depthTex);
    glGetTexImage(GL_TEXTURE_2D,0,GL_RED,GL_FLOAT,depthmap.data);
    //glGetTexImage(GL_TEXTURE_2D,0,GL_RED,GL_FLOAT,depthmap.data);



    //GET INDEX TEXTURE
    cv::Mat indexMap(res.y,res.x,CV_32SC1);
    glBindTexture(GL_TEXTURE_2D,indexTex);
    glGetTexImage(GL_TEXTURE_2D,0,GL_RED_INTEGER,GL_UNSIGNED_INT,indexMap.data);


    //GET SSBO DATA
    glm::vec2* faceSurfaceArea=new glm::vec2[faceCount];
    glBindBuffer(GL_ARRAY_BUFFER,SSBO);//GL_SHADER_STORAGE_BUFFER
    glGetBufferSubData(GL_ARRAY_BUFFER,0,sizeof(glm::vec2)*faceCount,faceSurfaceArea);
    int* facePixelCount=new int[faceCount]();//hopefully initzialized with zero

    //GET COLOR TEXTURE
    cv::Mat colorMat(res.y,res.x,CV_8UC4);
    glBindTexture(GL_TEXTURE_2D,colorTex);
    glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,colorMat.data);
    imshow("colorMat",colorMat);
    color=colorMat;

    for(int u=0;u<depthmap.rows;u++){
        for(int v=0;v<depthmap.cols;v++){
            if(indexMap.at<unsigned int>(u,v)!=0){
                facePixelCount[indexMap.at<unsigned int>(u,v)-1]++;
            }
        }
    }
    float visibleArea=0;
    float fullArea=0;
    int fullPixelCount=0;
    for(int i=0; i<faceCount; i++){
        //std::cout << "pixel count face " << i << ": " << facePixelCount[i]<< std::endl;
        fullPixelCount+=facePixelCount[i];
        fullArea+=faceSurfaceArea[i].x;
        float pixelForFace=faceSurfaceArea[i].y;
        if(pixelForFace!=0){
            visibleArea+=faceSurfaceArea[i].x*float(facePixelCount[i])/pixelForFace;
        }
        //if(facePixelCount[i]>0){
        //    std::cout << "face" << i << std::endl;
        //    std::cout << "visible pixel: " << facePixelCount[i] << " theoretically: " <<  faceSurfaceArea[i].y << std::endl;
        //}
    }
    //std::cout << "full pixel count" << fullPixelCount << std::endl;
    //std::cout << "full Surface" << fullArea << std::endl;
    //std::cout << "visible Area" << visibleArea << "in %:" << visibleArea/fullArea*100.0f<< std::endl;

    visible=visibleArea/fullArea;



    std::cout << "index value " << indexMap.at<unsigned int>(240,320) << std::endl;
    //read depthbuffer
    //glBindRenderbuffer(GL_RENDERBUFFER, zBuffer);
    //glReadPixels(0,0,res.x,res.y,GL_DEPTH_COMPONENT,GL_FLOAT,depthmap.data);
    std::cout << "depth value " << depthmap.at<float>(240,320) << std::endl;

    if(glGetError()){
        std::cout << "Something wrong with opengl at rendering" << std::endl;
    }

    return depthmap;
}

pcl::PointCloud<pcl::PointXYZ> DepthmapRenderer::renderPointcloud(float &visibleSurfaceArea)
{
    const float bad_point = std::numeric_limits<float>::quiet_NaN();
    pcl::PointCloud<pcl::PointXYZ> cloud;
    cloud.width    = res.x;
    cloud.height   = res.y;
    cloud.is_dense = false;
    cloud.points.resize (cloud.width * cloud.height);

    //set pose inside pcl structure
    Eigen::Matrix4f ePose;
    for(int i=0;i<4;i++){
        for(int j=0;j<4;j++){
            ePose(i,j)=pose[i][j];
        }
    }
    cloud.sensor_orientation_ = Eigen::Quaternionf(Eigen::Matrix3f(ePose.block(0,0,3,3)));
    Eigen::Vector3f p=Eigen::Matrix3f(ePose.block(0,0,3,3))*Eigen::Vector3f(ePose(3,0),ePose(3,1),ePose(3,2));
    cloud.sensor_origin_ = Eigen::Vector4f(p(0),p(1),p(2),1.0f);

    cv::Mat color;
    cv::Mat depth=renderDepthmap(visibleSurfaceArea,color);
    for(int k=0;k<cloud.height;k++){
        for(int j=0;j<cloud.width;j++){
            float d=depth.at<float>(k,j);
            if(d==0){
                cloud.at(j,k)=pcl::PointXYZ(bad_point,bad_point,bad_point);
            }else{
                pcl::PointXYZ p;
                p.x=((float)j-fxycxy.z)/fxycxy.x*d;
                p.y=((float)k-fxycxy.w)/fxycxy.y*d;
                p.z=d;

                cloud.at(j,k)=p;
            }

        }
    }


    return cloud;
}

pcl::PointCloud<pcl::PointXYZRGB> DepthmapRenderer::renderPointcloudColor(float &visibleSurfaceArea)
{
    const float bad_point = std::numeric_limits<float>::quiet_NaN();
    pcl::PointCloud<pcl::PointXYZRGB> cloud;
    cloud.width    = res.x;
    cloud.height   = res.y;
    cloud.is_dense = false;
    cloud.points.resize (cloud.width * cloud.height);

    //set pose inside pcl structure
    Eigen::Matrix4f ePose;
    for(int i=0;i<4;i++){
        for(int j=0;j<4;j++){
            ePose(i,j)=pose[i][j];
        }
    }
    cloud.sensor_orientation_ = Eigen::Quaternionf(Eigen::Matrix3f(ePose.block(0,0,3,3)));
    Eigen::Vector3f p=Eigen::Matrix3f(ePose.block(0,0,3,3))*Eigen::Vector3f(ePose(3,0),ePose(3,1),ePose(3,2));
    cloud.sensor_origin_ = Eigen::Vector4f(p(0),p(1),p(2),1.0f);

    cv::Mat color;
    cv::Mat depth=renderDepthmap(visibleSurfaceArea,color);
    for(int k=0;k<cloud.height;k++){
        for(int j=0;j<cloud.width;j++){
            float d=depth.at<float>(k,j);
            if(d==0){
                cloud.at(j,k).x=bad_point;
                cloud.at(j,k).y=bad_point;
                cloud.at(j,k).z=bad_point;
            }else{
                pcl::PointXYZRGB p;
                p.x=((float)j-fxycxy.z)/fxycxy.x*d;
                p.y=((float)k-fxycxy.w)/fxycxy.y*d;
                p.z=d;
                glm::u8vec4 rgba=color.at<glm::u8vec4>(k,j);
                p.r=rgba.r;
                p.g=rgba.g;
                p.b=rgba.b;

                cloud.at(j,k)=p;
            }

        }
    }


    return cloud;
}

}
