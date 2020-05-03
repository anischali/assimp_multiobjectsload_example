#include <assert.h>

#include <GL4D/gl4duw_SDL2.h>
#include <SDL_image.h>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

static int _size = 0;
static int _alloc = 0;
static int _count = 1;

typedef struct objectScene
{
    uint id;
    struct aiScene *_scene;
    struct aiVector3D _scene_min, _scene_max, _scene_center;
    GLuint *_vaos, *_buffers, *_counts, *_textures, _nbMeshes, _nbTextures;
} objectScene_t;

static objectScene_t *_objects = NULL;



#define aisgl_min(x, y) (x < y ? x : y)
#define aisgl_max(x, y) (y > x ? y : x)

static void alloc_memory(void)
{
    if (!_objects)
    {
        _objects = (objectScene_t *)malloc((_size = 24) * sizeof *_objects);
        for (int iobj = 0; iobj < _size; ++iobj)
        {
            _objects[iobj].id = 0;
        }
        _alloc = 0;
    }
}

static void realloc_memory(void)
{
    _objects = (objectScene_t *)realloc(_objects, (_size *= 2) * sizeof *_objects);
    for (int iobj = _count; iobj < _size; ++iobj)
    {
        _objects[iobj].id = 0;
    }
}

int assimpInit(const char *filename);
void assimpDrawScene(int id);
void freeObj(int id);
void assimpQuit(void);
static void get_bounding_box_for_node(const struct aiNode *nd, struct aiVector3D *min, struct aiVector3D *max, struct aiMatrix4x4 *trafo, int id);
static void get_bounding_box(struct aiVector3D *min, struct aiVector3D *max, int id);
static void color4_to_float4(const struct aiColor4D *c, float f[4]);
static void set_float4(float f[4], float a, float b, float c, float d);
static void apply_material(const struct aiMaterial *mtl);
static void sceneMkVAOs(const struct aiScene *sc, const struct aiNode *nd, GLuint *ivao, int obj_id);
static void sceneDrawVAOs(const struct aiScene *sc, const struct aiNode *nd, GLuint *ivao, int obj_id);
static int sceneNbMeshes(const struct aiScene *sc, const struct aiNode *nd, int subtotal);
static int loadasset(const char *path, int obj_id);

int assimpInit(const char *filename)
{
    if (!_alloc)
    {
        alloc_memory();
    }
    if (!(_count < _size))
    {
        realloc_memory();
    }

    _objects[_count].id = _count;
    int i;
    GLuint ivao = 0;
    struct aiLogStream stream;
    stream = aiGetPredefinedLogStream(aiDefaultLogStream_STDOUT, NULL);
    aiAttachLogStream(&stream);
    stream = aiGetPredefinedLogStream(aiDefaultLogStream_FILE, "assimp_log.txt");
    aiAttachLogStream(&stream);
    if (loadasset(filename, _count) != 0)
    {
        fprintf(stderr, "Erreur lors du chargement du fichier %s\n", filename);
        exit(3);
    }
    /* XXX docs say all polygons are emitted CCW, but tests show that some aren't. */
    if (getenv("MODEL_IS_BROKEN"))
        glFrontFace(GL_CW);

    _objects[_count]._textures = malloc((_objects[_count]._nbTextures = _objects[_count]._scene->mNumMaterials) *
                                         sizeof *_objects[_count]._textures);
    assert(_objects[_count]._textures);

    glGenTextures(_objects[_count]._nbTextures, _objects[_count]._textures);

    for (i = 0; i < _objects[_count]._scene->mNumMaterials; i++)
    {
        const struct aiMaterial *pMaterial = _objects[_count]._scene->mMaterials[i];
        if (aiGetMaterialTextureCount(pMaterial, aiTextureType_DIFFUSE) > 0)
        {
            struct aiString tfname;
            char *dir = pathOf(filename), buf[BUFSIZ];
            if (aiGetMaterialTexture(pMaterial, aiTextureType_DIFFUSE, 0, &tfname, NULL, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS)
            {
                SDL_Surface *t;
                snprintf(buf, sizeof buf, "%s/%s", dir, tfname.data);

                if (!(t = IMG_Load(buf)))
                {
                    fprintf(stderr, "Probleme de chargement de textures %s\n", buf);
                    fprintf(stderr, "\tNouvel essai avec %s\n", tfname.data);
                    if (!(t = IMG_Load(tfname.data)))
                    {
                        fprintf(stderr, "Probleme de chargement de textures %s\n", tfname.data);
                        continue;
                    }
                }
                glBindTexture(GL_TEXTURE_2D, _objects[_count]._textures[i]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT /* GL_CLAMP_TO_EDGE */);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT /* GL_CLAMP_TO_EDGE */);
#ifdef __APPLE__
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, t->w, t->h, 0, t->format->BytesPerPixel == 3 ? GL_BGR : GL_BGRA, GL_UNSIGNED_BYTE, t->pixels);
#else
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, t->w, t->h, 0, t->format->BytesPerPixel == 3 ? GL_RGB : GL_RGBA, GL_UNSIGNED_BYTE, t->pixels);
#endif
                SDL_FreeSurface(t);
            }
        }
    }

    _objects[_count]._nbMeshes = sceneNbMeshes(_objects[_count]._scene, _objects[_count]._scene->mRootNode, 0);
    _objects[_count]._vaos = malloc(_objects[_count]._nbMeshes * sizeof *_objects[_count]._vaos);
    assert(_objects[_count]._vaos);
    glGenVertexArrays(_objects[_count]._nbMeshes, _objects[_count]._vaos);
    _objects[_count]._buffers = malloc(2 * _objects[_count]._nbMeshes * sizeof *_objects[_count]._buffers);
    assert(_objects[_count]._buffers);
    glGenBuffers(2 * _objects[_count]._nbMeshes, _objects[_count]._buffers);
    _objects[_count]._counts = calloc(_objects[_count]._nbMeshes, sizeof *_objects[_count]._counts);
    assert(_objects[_count]._counts);
    sceneMkVAOs(_objects[_count]._scene, _objects[_count]._scene->mRootNode, &ivao, _count);
    ++_count;
    return _count - 1;
}

void assimpDrawScene(int id)
{
    GLfloat tmp;
    GLuint ivao = 0;
    tmp = _objects[id]._scene_max.x - _objects[id]._scene_min.x;
    tmp = aisgl_max(_objects[id]._scene_max.y - _objects[id]._scene_min.y, tmp);
    tmp = aisgl_max(_objects[id]._scene_max.z - _objects[id]._scene_min.z, tmp);
    tmp = 1.0f / tmp;
    gl4duScalef(tmp, tmp, tmp);
    gl4duTranslatef(-_objects[id]._scene_center.x, -_objects[id]._scene_center.y, -_objects[id]._scene_center.z);
    sceneDrawVAOs(_objects[id]._scene, _objects[id]._scene->mRootNode, &ivao, id);
}

void freeObj(int id)
{
    /* cleanup - calling 'aiReleaseImport' is important, as the library 
     keeps internal resources until the scene is freed again. Not 
     doing so can cause severe resource leaking. */
    aiReleaseImport(_objects[id]._scene);
    /* We added a log stream to the library, it's our job to disable it
     again. This will definitely release the last resources allocated
     by Assimp.*/
    aiDetachAllLogStreams();
    if (_objects[id]._counts)
    {
        free(_objects[id]._counts);
        _objects[id]._counts = NULL;
    }
    if (_objects[id]._textures)
    {
        glDeleteTextures(_objects[id]._nbTextures, _objects[id]._textures);
        free(_objects[id]._textures);
        _objects[id]._textures = NULL;
    }
    if (_objects[id]._vaos)
    {
        glDeleteVertexArrays(_objects[id]._nbMeshes, _objects[id]._vaos);
        free(_objects[id]._vaos);
        _objects[id]._vaos = NULL;
    }
    if (_objects[id]._buffers)
    {
        glDeleteBuffers(2 * _objects[id]._nbMeshes, _objects[id]._buffers);
        free(_objects[id]._buffers);
        _objects[id]._buffers = NULL;
    }
}

void assimpQuit(void)
{
    for (int iobj = 1; iobj < _count; ++iobj)
    {
        freeObj(iobj);
    }
    free(_objects);
    _objects = NULL;
}

static void get_bounding_box_for_node(const struct aiNode *nd, struct aiVector3D *min, struct aiVector3D *max, struct aiMatrix4x4 *trafo, int id)
{
    struct aiMatrix4x4 prev;
    unsigned int n = 0, t;
    prev = *trafo;
    aiMultiplyMatrix4(trafo, &nd->mTransformation);
    for (; n < nd->mNumMeshes; ++n)
    {
        const struct aiMesh *mesh = _objects[id]._scene->mMeshes[nd->mMeshes[n]];
        for (t = 0; t < mesh->mNumVertices; ++t)
        {
            struct aiVector3D tmp = mesh->mVertices[t];
            aiTransformVecByMatrix4(&tmp, trafo);
            min->x = aisgl_min(min->x, tmp.x);
            min->y = aisgl_min(min->y, tmp.y);
            min->z = aisgl_min(min->z, tmp.z);
            max->x = aisgl_max(max->x, tmp.x);
            max->y = aisgl_max(max->y, tmp.y);
            max->z = aisgl_max(max->z, tmp.z);
        }
    }
    for (n = 0; n < nd->mNumChildren; ++n)
    {
        get_bounding_box_for_node(nd->mChildren[n], min, max, trafo, id);
    }
    *trafo = prev;
}

static void get_bounding_box(struct aiVector3D *min, struct aiVector3D *max, int id)
{
    struct aiMatrix4x4 trafo;
    aiIdentityMatrix4(&trafo);
    min->x = min->y = min->z = 1e10f;
    max->x = max->y = max->z = -1e10f;
    get_bounding_box_for_node(_objects[id]._scene->mRootNode, min, max, &trafo, id);
}

static void color4_to_float4(const struct aiColor4D *c, float f[4])
{
    f[0] = c->r;
    f[1] = c->g;
    f[2] = c->b;
    f[3] = c->a;
}

static void set_float4(float f[4], float a, float b, float c, float d)
{
    f[0] = a;
    f[1] = b;
    f[2] = c;
    f[3] = d;
}

static void apply_material(const struct aiMaterial *mtl)
{
    float c[4];
    unsigned int max;
    float shininess, strength;
    struct aiColor4D diffuse, specular, ambient, emission;
    GLint id;
    glGetIntegerv(GL_CURRENT_PROGRAM, &id);

    set_float4(c, 0.8f, 0.8f, 0.8f, 1.0f);
    if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &diffuse))
    {
        color4_to_float4(&diffuse, c);
    }
    glUniform4fv(glGetUniformLocation(id, "diffuse_color"), 1, c);

    set_float4(c, 0.0f, 0.0f, 0.0f, 1.0f);
    if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_SPECULAR, &specular))
    {
        color4_to_float4(&specular, c);
    }
    glUniform4fv(glGetUniformLocation(id, "specular_color"), 1, c);

    set_float4(c, 0.2f, 0.2f, 0.2f, 1.0f);
    if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_AMBIENT, &ambient))
    {
        color4_to_float4(&ambient, c);
    }
    glUniform4fv(glGetUniformLocation(id, "ambient_color"), 1, c);

    set_float4(c, 0.0f, 0.0f, 0.0f, 1.0f);
    if (AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_EMISSIVE, &emission))
    {
        color4_to_float4(&emission, c);
    }
    glUniform4fv(glGetUniformLocation(id, "emission_color"), 1, c);

    max = 1;
    if (aiGetMaterialFloatArray(mtl, AI_MATKEY_SHININESS, &shininess, &max) == AI_SUCCESS)
    {
        max = 1;
        if (aiGetMaterialFloatArray(mtl, AI_MATKEY_SHININESS_STRENGTH, &strength, &max) == AI_SUCCESS)
            glUniform1f(glGetUniformLocation(id, "shininess"), shininess * strength);
        else
            glUniform1f(glGetUniformLocation(id, "shininess"), shininess);
    }
    else
    {
        shininess = 0.0;
        glUniform1f(glGetUniformLocation(id, "shininess"), shininess);
    }
}

static void sceneMkVAOs(const struct aiScene *sc, const struct aiNode *nd, GLuint *ivao, int obj_id)
{
    int i, j, comp;
    unsigned int n = 0;
    static int temp = 0;

    temp++;

    for (; n < nd->mNumMeshes; ++n)
    {
        GLfloat *vertices = NULL;
        GLuint *indices = NULL;
        const struct aiMesh *mesh = sc->mMeshes[nd->mMeshes[n]];
        comp = mesh->mVertices ? 3 : 0;
        comp += mesh->mNormals ? 3 : 0;
        comp += mesh->mTextureCoords[0] ? 2 : 0;
        if (!comp)
            continue;

        glBindVertexArray(_objects[obj_id]._vaos[*ivao]);
        glBindBuffer(GL_ARRAY_BUFFER, _objects[obj_id]._buffers[2 * (*ivao)]);

        vertices = malloc(comp * mesh->mNumVertices * sizeof *vertices);
        assert(vertices);
        i = 0;
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
        if (mesh->mVertices)
        {
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const void *)(i * sizeof *vertices));
            for (j = 0; j < mesh->mNumVertices; ++j)
            {
                vertices[i++] = mesh->mVertices[j].x;
                vertices[i++] = mesh->mVertices[j].y;
                vertices[i++] = mesh->mVertices[j].z;
            }
        }
        if (mesh->mNormals)
        {
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (const void *)(i * sizeof *vertices));
            for (j = 0; j < mesh->mNumVertices; ++j)
            {
                vertices[i++] = mesh->mNormals[j].x;
                vertices[i++] = mesh->mNormals[j].y;
                vertices[i++] = mesh->mNormals[j].z;
            }
        }
        if (mesh->mTextureCoords[0])
        {
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (const void *)(i * sizeof *vertices));
            for (j = 0; j < mesh->mNumVertices; ++j)
            {
                vertices[i++] = mesh->mTextureCoords[0][j].x;
                vertices[i++] = mesh->mTextureCoords[0][j].y;
            }
        }
        glBufferData(GL_ARRAY_BUFFER, (i * sizeof *vertices), vertices, GL_STATIC_DRAW);
        free(vertices);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _objects[obj_id]._buffers[2 * (*ivao) + 1]);
        if (mesh->mFaces)
        {
            indices = malloc(3 * mesh->mNumFaces * sizeof *indices);
            assert(indices);
            for (i = 0, j = 0; j < mesh->mNumFaces; ++j)
            {
                assert(mesh->mFaces[j].mNumIndices < 4);
                if (mesh->mFaces[j].mNumIndices != 3)
                    continue;
                indices[i++] = mesh->mFaces[j].mIndices[0];
                indices[i++] = mesh->mFaces[j].mIndices[1];
                indices[i++] = mesh->mFaces[j].mIndices[2];
            }
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, i * sizeof *indices, indices, GL_STATIC_DRAW);
            _objects[obj_id]._counts[*ivao] = i;
            free(indices);
        }
        glBindVertexArray(0);
        (*ivao)++;
    }
    for (n = 0; n < nd->mNumChildren; ++n)
    {
        sceneMkVAOs(sc, nd->mChildren[n], ivao, obj_id);
    }
}

static void sceneDrawVAOs(const struct aiScene *sc, const struct aiNode *nd, GLuint *ivao, int obj_id)
{
    unsigned int n = 0;
    struct aiMatrix4x4 m = nd->mTransformation;
    GLint id;

    glGetIntegerv(GL_CURRENT_PROGRAM, &id);
    /* By VB Inutile de transposer la matrice, gl4dummies fonctionne avec des transpose de GL. */
    /* aiTransposeMatrix4(&m); */
    gl4duPushMatrix();
    gl4duMultMatrixf((GLfloat *)&m);
    gl4duSendMatrices();

    for (; n < nd->mNumMeshes; ++n)
    {
        const struct aiMesh *mesh = sc->mMeshes[nd->mMeshes[n]];
        if (_objects[obj_id]._counts[*ivao])
        {
            glBindVertexArray(_objects[obj_id]._vaos[*ivao]);
            apply_material(sc->mMaterials[mesh->mMaterialIndex]);
            if (aiGetMaterialTextureCount(sc->mMaterials[mesh->mMaterialIndex], aiTextureType_DIFFUSE) > 0)
            {
                glBindTexture(GL_TEXTURE_2D, _objects[obj_id]._textures[mesh->mMaterialIndex]);
                glUniform1i(glGetUniformLocation(id, "hasTexture"), 1);
                glUniform1i(glGetUniformLocation(id, "myTexture"), 0);
            }
            else
            {
                glUniform1i(glGetUniformLocation(id, "hasTexture"), 0);
            }
            glDrawElements(GL_TRIANGLES, _objects[obj_id]._counts[*ivao], GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        (*ivao)++;
    }
    for (n = 0; n < nd->mNumChildren; ++n)
    {
        sceneDrawVAOs(sc, nd->mChildren[n], ivao, obj_id);
    }
    gl4duPopMatrix();
}

static int sceneNbMeshes(const struct aiScene *sc, const struct aiNode *nd, int subtotal)
{
    int n = 0;
    subtotal += nd->mNumMeshes;
    for (n = 0; n < nd->mNumChildren; ++n)
        subtotal += sceneNbMeshes(sc, nd->mChildren[n], 0);
    return subtotal;
}

static int loadasset(const char *path, int obj_id)
{
    /* we are taking one of the postprocessing presets to avoid
     spelling out 20+ single postprocessing flags here. */
    /* struct aiString str; */
    /* aiGetExtensionList(&str); */
    /* fprintf(stderr, "EXT %s\n", str.data); */
    _objects[obj_id]._scene = aiImportFile(path,
                          aiProcessPreset_TargetRealtime_MaxQuality |
                              aiProcess_CalcTangentSpace |
                              aiProcess_Triangulate |
                              aiProcess_JoinIdenticalVertices |
                              aiProcess_SortByPType);
    if (_objects[obj_id]._scene)
    {
        get_bounding_box(&_objects[obj_id]._scene_min, &_objects[obj_id]._scene_max, obj_id);
        _objects[obj_id]._scene_center.x = (_objects[obj_id]._scene_min.x + _objects[obj_id]._scene_max.x) / 2.0f;
        _objects[obj_id]._scene_center.y = (_objects[obj_id]._scene_min.y + _objects[obj_id]._scene_max.y) / 2.0f;
        _objects[obj_id]._scene_center.z = (_objects[obj_id]._scene_min.z + _objects[obj_id]._scene_max.z) / 2.0f;
        return 0;
    }
    return 1;
}
