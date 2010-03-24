// Assimp model resource.
// -------------------------------------------------------------------
// Copyright (C) 2007 OpenEngine.dk (See AUTHORS) 
// 
// 
// This program is free software; It is covered by the GNU General 
// Public License version 2 or any later version. 
// See the GNU General Public License for more details (see LICENSE). 
//--------------------------------------------------------------------

#include <Resources/AssimpResource.h>

#include <Scene/SceneNode.h>
#include <Logging/Logger.h>
#include <Resources/Exceptions.h>

#include <Geometry/Mesh.h>
#include <Scene/TransformationNode.h>

#include <Resources/File.h>
#include <Resources/ResourceManager.h>
#include <Resources/ITexture2D.h>


namespace OpenEngine {
namespace Resources {

    using namespace Scene;
    using namespace Geometry;

/**
 * Get the file extension for Assimp files.
 */
AssimpPlugin::AssimpPlugin() {
    this->AddExtension("dae");
    this->AddExtension("obj");
    this->AddExtension("3ds");
    this->AddExtension("ply");
    this->AddExtension("md5mesh");
    this->AddExtension("md5anim");
    this->AddExtension("xml");
    this->AddExtension("q3o");
}

/**
 * Create a Assimp resource.
 */
IModelResourcePtr AssimpPlugin::CreateResource(string file) {
    return IModelResourcePtr(new AssimpResource(file));
}

/**
 * Resource constructor.
 */
    AssimpResource::AssimpResource(string file): file(file), root(NULL) {}

/**
 * Resource destructor.
 */
AssimpResource::~AssimpResource() {
    Unload();
}

void AssimpResource::Load() {

    dir = File::Parent(this->file);

    // Create an instance of the Importer class
    Assimp::Importer importer;
    
    // And have it read the given file with some example postprocessing
    // Usually - if speed is not the most important aspect for you - you'll 
    // propably to request more postprocessing than we do in this example.
    const aiScene* scene = importer.ReadFile( file, 
                                              //aiProcess_CalcTangentSpace       | 
                                              //aiProcess_FlipUVs                |
                                              //aiProcess_MakeLeftHanded         |
                                              aiProcess_Triangulate            |
                                              aiProcess_JoinIdenticalVertices  |
                                              aiProcess_GenSmoothNormals       |
                                              aiProcess_SortByPType);
    
    // If the import failed, report it
    if( !scene)
        {
            Error( importer.GetErrorString() );
            return;
        }
    root = new SceneNode();
    
    // Now we can access the file's contents. 
    ReadMaterials(scene->mMaterials, scene->mNumMaterials);
    ReadMeshes(scene->mMeshes, scene->mNumMeshes);
    ReadScene(scene);
    // We're done. Everything will be cleaned up by the importer destructor
}

void AssimpResource::Unload() {
    if (!root) return;
    root = NULL;
}

ISceneNode* AssimpResource::GetSceneNode() {
    return root;
}

void AssimpResource::Error(string msg) {
    logger.error << "Assimp: " << msg << logger.end;
    throw new ResourceException("Assimp: " + msg);
}

void AssimpResource::Warning(string msg) {
    logger.warning << "Assimp: " <<  msg << logger.end;
}

void AssimpResource::ReadMeshes(aiMesh** ms, unsigned int size) {
    unsigned int i, j;
    logger.info << "meshCount: " << size << logger.end;
    for (i = 0; i < size; ++i) {
        aiMesh* m = ms[i];
        // read vertices
        unsigned int num = m->mNumVertices;
        aiVector3D* src = m->mVertices;
        float* dest = new float[3 * num];
        for (j = 0; j < num; ++j) {
            dest[3*j]   = src[j].x;
            dest[3*j+1] = src[j].y;
            dest[3*j+2] = src[j].z;
        }
        Float3DataBlockPtr pos = Float3DataBlockPtr(new DataBlock<3,float>(num, dest));
        Float3DataBlockPtr norm;
        if (m->HasNormals()) {
            // read normals
            src = m->mNormals;
            dest = new float[3 * num];
            for (j = 0; j < num; ++j) {
                dest[j*3]   = src[j].x;
                dest[j*3+1] = src[j].y;
                dest[j*3+2] = src[j].z;
            }
            norm = Float3DataBlockPtr(new DataBlock<3,float>(num, dest));
        }
        IDataBlockList texc;
        logger.info << "numUV: " << m->GetNumUVChannels() << logger.end;
        for (j = 0; j < m->GetNumUVChannels(); ++j) {
            // read texture coordinates
            unsigned int dim = m->mNumUVComponents[j];
            logger.info << "numUVComponents: " << dim << logger.end;
            src = m->mTextureCoords[j];
            dest = new float[dim * num];
            for (unsigned int k = 0; k < num; ++k) {
                for (unsigned int l = 0; l < dim; ++l) {
                    // dest[k*dim]   = src[k].x;
                    // dest[k*dim+1] = src[k].y;
                    // dest[k*dim+2] = src[k].z;
                     dest[k*dim+l] = src[k][l];
                }
                    //logger.info << "texc: (" << src[k].x << ", " << src[k].y << ")" << logger.end; 
            }
            switch (dim) {
            case 2:
                texc.push_back(Float2DataBlockPtr(new DataBlock<2,float>(num, dest)));
                break;
            case 3:
                texc.push_back(Float3DataBlockPtr(new DataBlock<3,float>(num, dest)));
                break;
            default: 
                delete dest;
                Warning("Unsupported texture coordinate dimension");
            };
        }

        Float3DataBlockPtr col;
        if (m->GetNumColorChannels() > 0) {
            aiColor4D* c = m->mColors[0];
            dest = new float[3 * num];
            for (j = 0; j < num; ++j) {
                dest[j*3]   = c[j].r;
                dest[j*3+1] = c[j].g;
                dest[j*3+2] = c[j].b;
            }
            col = Float3DataBlockPtr(new DataBlock<3,float>(num, dest));
        }
        logger.info << "NumFaces: " << m->mNumFaces << logger.end;

        // assume that we only have triangles (see triangulate option).
        unsigned int* indexArr = new unsigned int[m->mNumFaces * 3];
        for (j = 0; j < m->mNumFaces; ++j) {
            aiFace src = m->mFaces[j];
            indexArr[j*3]   = src.mIndices[0];
            indexArr[j*3+1] = src.mIndices[1];
            indexArr[j*3+2] = src.mIndices[2];
        }
        DataIndicesPtr index = DataIndicesPtr(new DataIndices(m->mNumFaces*3, indexArr));
        GeometrySetPtr gs = GeometrySetPtr(new GeometrySet(pos, norm, texc, col));
        MeshPtr prim = MeshPtr(new Mesh(index, TRIANGLES, gs, materials[m->mMaterialIndex])); 
        meshes.push_back(prim);
    }
}
    
void AssimpResource::ReadMaterials(aiMaterial** ms, unsigned int size) {
    logger.info << "NumMaterials: " << size << logger.end;
    unsigned int i;
    for (i = 0; i < size; ++i) {
        MaterialPtr mat = MaterialPtr(new Material);
        aiMaterial* m = ms[i];
        aiColor3D c;
        float tmp;
        if (AI_SUCCESS == m->Get(AI_MATKEY_COLOR_DIFFUSE, c)) {
            mat->diffuse = Vector<4,float>(c.r, c.g, c.b, 1.0);
            logger.info << "diffuse: " << Vector<4,float>(c.r, c.g, c.b, 1.0) << logger.end;
        }
        if (AI_SUCCESS == m->Get(AI_MATKEY_COLOR_SPECULAR, c)) {
            mat->specular = Vector<4,float>(c.r, c.g, c.b, 1.0);
            logger.info << "specular: " << Vector<4,float>(c.r, c.g, c.b, 1.0) << logger.end;
        }
        if (AI_SUCCESS == m->Get(AI_MATKEY_COLOR_AMBIENT, c)) {
            mat->ambient = Vector<4,float>(c.r, c.g, c.b, 1.0);
            logger.info << "ambient: " << Vector<4,float>(c.r, c.g, c.b, 1.0) << logger.end;
        }
        if (AI_SUCCESS == m->Get(AI_MATKEY_COLOR_EMISSIVE, c)) {
            mat->emission = Vector<4,float>(c.r, c.g, c.b, 1.0);
            logger.info << "emission: " << Vector<4,float>(c.r, c.g, c.b, 1.0) << logger.end;
        }
        if (AI_SUCCESS == m->Get(AI_MATKEY_SHININESS, tmp) && tmp >= 0.0f && tmp <= 128.0f)
            mat->shininess = tmp;
        
        // just read the stack 0 texture if there is any
        aiString path;
        if (AI_SUCCESS == m->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), path)) {
            logger.info << "path: " << dir + string(path.data) << logger.end;
            ITexture2DPtr texr = ResourceManager<ITextureResource>::Create(dir + string(path.data));
            mat->texr = texr;
        
        }
        materials.push_back(mat);
    }
}
    
void AssimpResource::ReadScene(const aiScene* scene) {
    aiNode* mRoot = scene->mRootNode;
    ReadNode(mRoot, root);
}

void AssimpResource::ReadNode(aiNode* node, ISceneNode* parent) {
    unsigned int i;
    ISceneNode* current = parent;
    aiMatrix4x4 t = node->mTransformation;
    if (!t.IsIdentity()) {
        aiVector3D pos, scl;
        aiQuaternion rot;
        t.Decompose(scl, rot, pos);
        TransformationNode* tn = new TransformationNode();
        tn->SetPosition(Vector<3,float>(pos.x, pos.y, pos.z));
        tn->SetScale(Vector<3,float>(scl.x, scl.y, scl.z));
        tn->SetRotation(Quaternion<float>(rot.w, Vector<3,float>(rot.x, rot.y, rot.z)));
        current->AddNode(tn);
        current = tn;
    }
    if (node->mNumMeshes > 0) {
        ISceneNode* scene = new SceneNode();
        for (i = 0; i < node->mNumMeshes; ++i) {
            scene->AddNode(new MeshNode(meshes[node->mMeshes[i]]));
        } 
        current->AddNode(scene);
        current = scene;
    }
    for (i = 0; i < node->mNumChildren; ++i) {
        ReadNode(node->mChildren[i], current);
    }
}

} // NS Resources
} // NS OpenEngine
