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
#include <Animations/Animation.h>
#include <Animations/AnimatedTransformation.h>
#include <Animations/AnimatedMesh.h>
#include <Animations/Bone.h>

#include <Scene/AnimationNode.h>
#include <Scene/AnimatedTransformationNode.h>
#include <Scene/AnimatedMeshNode.h>


namespace OpenEngine {
namespace Resources {

    using namespace Scene;
    using namespace Geometry;
    using namespace Animations;

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
AssimpResource::AssimpResource(string file): file(file), root(NULL), animRoot(NULL) {
}

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
                                              aiProcess_MakeLeftHanded         |
                                              aiProcess_Triangulate            |
                                              aiProcess_JoinIdenticalVertices  |
                                              aiProcess_GenSmoothNormals       |
                                              aiProcess_SortByPType);
    
    // If the import failed, report it
    if( !scene){
        Error( importer.GetErrorString() );
        return;
    }
    root = new SceneNode();
    
    // Now we can access the file's contents. 
    ReadMaterials(scene->mMaterials, scene->mNumMaterials);
    ReadMeshes(scene->mMeshes, scene->mNumMeshes);

    ReadScene(scene);
    ReadAnimations(scene->mAnimations, scene->mNumAnimations);
    ReadAnimatedMeshes(scene->mMeshes, scene->mNumMeshes);
    // We're done. Everything will be cleaned up by the importer destructor
}

void AssimpResource::Unload() {
    if (!root) return;
    root = NULL;
}

ISceneNode* AssimpResource::GetSceneNode() {
    return root;
}

ISceneNode* AssimpResource::GetMeshes() {
    return root;
}

AnimationNode* AssimpResource::GetAnimations() {
    if( animRoot )
        cout << "[assimp] animRoot name: " << animRoot->GetNodeName() << ", ptr: " << animRoot << endl;
    return animRoot;
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
        cout << "MeshName:   " << m->mName.data << endl;
        cout << "numBones:   " << m->mNumBones << endl; 
        for(unsigned int b=0; b<m->mNumBones; b++){
            aiBone* bone = m->mBones[b];
            cout << "   bone: " << b << endl;
            cout << "   numWeights: " << bone->mNumWeights << endl;
            cout << "   [";
            for(unsigned int v=0; v<bone->mNumWeights; v++){
                cout << bone->mWeights[v].mVertexId << ", ";
            }
            cout << "]" << endl;
        }

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
        //logger.info << "numUV: " << m->GetNumUVChannels() << logger.end;
        for (j = 0; j < m->GetNumUVChannels(); ++j) {
            // read texture coordinates
            unsigned int dim = m->mNumUVComponents[j];
            //logger.info << "numUVComponents: " << dim << logger.end;
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
        //logger.info << "NumFaces: " << m->mNumFaces << logger.end;

        // assume that we only have triangles (see triangulate option).
        unsigned int* indexArr = new unsigned int[m->mNumFaces * 3];
        for (j = 0; j < m->mNumFaces; ++j) {
            aiFace src = m->mFaces[j];
            indexArr[j*3]   = src.mIndices[0];
            indexArr[j*3+1] = src.mIndices[1];
            indexArr[j*3+2] = src.mIndices[2];
        }
        IndicesPtr index = IndicesPtr(new Indices(m->mNumFaces*3, indexArr));
        GeometrySetPtr gs = GeometrySetPtr(new GeometrySet(pos, norm, texc, col));
        MeshPtr prim = MeshPtr(new Mesh(index, TRIANGLES, gs, materials[m->mMaterialIndex])); 
        meshes.push_back(prim);

        // If the aiMesh has bones, associate it with its MeshPtr
        if( m->HasBones() ){
            meshMap[m] = prim;
        }
    }
}
    
void AssimpResource::ReadMaterials(aiMaterial** ms, unsigned int size) {
    // logger.info << "NumMaterials: " << size << logger.end;
    unsigned int i;
    for (i = 0; i < size; ++i) {
        MaterialPtr mat = MaterialPtr(new Material);
        aiMaterial* m = ms[i];
        aiColor3D c;
        float tmp;
        int shade;
        if (AI_SUCCESS == m->Get(AI_MATKEY_SHADING_MODEL, shade)) { 
            switch (shade) {
            case aiShadingMode_Gouraud:
                logger.info << "use gouraud shader" << logger.end;
                break;
            case aiShadingMode_Phong:
                mat->shading = Material::PHONG;
                logger.info << "use phong shader" << logger.end;
                break;
            default:
                mat->shading = Material::NONE;
            }
        }
        if (AI_SUCCESS == m->Get(AI_MATKEY_COLOR_DIFFUSE, c)) 
            mat->diffuse = Vector<4,float>(c.r, c.g, c.b, 1.0);
        if (AI_SUCCESS == m->Get(AI_MATKEY_COLOR_SPECULAR, c)) 
            mat->specular = Vector<4,float>(c.r, c.g, c.b, 1.0);
            if (AI_SUCCESS == m->Get(AI_MATKEY_COLOR_AMBIENT, c)) 
            mat->ambient = Vector<4,float>(c.r, c.g, c.b, 1.0);
        if (AI_SUCCESS == m->Get(AI_MATKEY_COLOR_EMISSIVE, c)) 
            mat->emission = Vector<4,float>(c.r, c.g, c.b, 1.0);
        if (AI_SUCCESS == m->Get(AI_MATKEY_SHININESS, tmp) && tmp >= 0.0f && tmp <= 128.0f)
            mat->shininess = tmp;
        
        // just read the stack 0 texture if there is any
        aiString path;
        if (AI_SUCCESS == m->Get(AI_MATKEY_TEXTURE(aiTextureType_AMBIENT, 0), path)) {
            logger.info << "ambient map path: " << dir + string(path.data) << logger.end;
            ITexture2DPtr texr = ResourceManager<ITextureResource>::Create(dir + string(path.data));
            mat->AddTexture(texr, "ambient");
        }
        if (AI_SUCCESS == m->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), path)) {
            logger.info << "diffuse map path: " << dir + string(path.data) << logger.end;
            ITexture2DPtr texr = ResourceManager<ITextureResource>::Create(dir + string(path.data));
            mat->AddTexture(texr, "diffuse");
        }
        if (AI_SUCCESS == m->Get(AI_MATKEY_TEXTURE(aiTextureType_SPECULAR, 0), path)) {
            logger.info << "specular map path: " << dir + string(path.data) << logger.end;
            ITexture2DPtr texr = ResourceManager<ITextureResource>::Create(dir + string(path.data));
            mat->AddTexture(texr, "specular");
        }
        if (AI_SUCCESS == m->Get(AI_MATKEY_TEXTURE(aiTextureType_EMISSIVE, 0), path)) {
            logger.info << "emissive map path: " << dir + string(path.data) << logger.end;
            ITexture2DPtr texr = ResourceManager<ITextureResource>::Create(dir + string(path.data));
            mat->AddTexture(texr, "emissive");
        }
        if (AI_SUCCESS == m->Get(AI_MATKEY_TEXTURE(aiTextureType_NORMALS, 0), path)) {
            logger.info << "normal map path: " << dir + string(path.data) << logger.end;
            ITexture2DPtr texr = ResourceManager<ITextureResource>::Create(dir + string(path.data));
            mat->AddTexture(texr, "normals");
        }
        if (AI_SUCCESS == m->Get(AI_MATKEY_TEXTURE(aiTextureType_HEIGHT, 0), path)) {
            logger.info << "height map path: " << dir + string(path.data) << logger.end;
            ITexture2DPtr texr = ResourceManager<ITextureResource>::Create(dir + string(path.data));
            mat->AddTexture(texr, "height");
        }
        materials.push_back(mat);
    }
}
    
void AssimpResource::ReadScene(const aiScene* scene) {
    aiNode* mRoot = scene->mRootNode;

    ReadNode(mRoot, root);

    // debug
    map<std::string, TransformationNode*>::iterator itr;
    for( itr=transMap.begin(); itr!=transMap.end(); itr++ ){
        logger.info << itr->first << " -> " << itr->second << logger.end;
    }
}

void AssimpResource::ReadNode(aiNode* node, ISceneNode* parent) {

    unsigned int i;
    ISceneNode* current = parent;
    aiMatrix4x4 t = node->mTransformation;

    // If the node holds any mesh we create a parent transformation node for the mesh.
    if ( node->mNumMeshes > 0 ) {
        aiVector3D pos, scl;
        aiQuaternion rot;
        // NOTE: decompose seems buggy when it comes to rotations
        t.Decompose(scl, rot, pos);
        // Use rotation matrix to construct rotation quaternion instead.
        aiMatrix3x3 m3 = rot.GetMatrix();
        // Try creating quaternion from aiMatrix which seems correct.
        Quaternion<float> q = Quaternion<float>(Matrix<3,3,float>(m3.a1, m3.a2, m3.a3, 
                                                                  m3.b1, m3.b2, m3.b3,  
                                                                  m3.c1, m3.c2, m3.c3));

        // Create parent transformation node.
        TransformationNode* tn = new TransformationNode();
        tn->SetPosition(Vector<3,float>(pos.x, pos.y, pos.z));
        tn->SetScale(Vector<3,float>(scl.x, scl.y, scl.z));
        tn->SetRotation(q);
        //tn->SetRotation(Quaternion<float>(rot.w, Vector<3,float>(rot.x, rot.y, rot.z)));
        //        tn->SetRotation(rot);

        current->AddNode(tn);
        current = tn;

        // Create scene node and add all mesh nodes to it.
        ISceneNode* scene = new SceneNode();
        for (i = 0; i < node->mNumMeshes; ++i) {
            MeshNode* meshNode = new MeshNode(meshes[node->mMeshes[i]]);
            char buf[16];
            sprintf(buf, "\n faces: %i", meshNode->GetMesh()->GetGeometrySet()->GetSize());
            string name = meshNode->GetNodeName();
            meshNode->SetNodeName(name.append(buf));
            scene->AddNode(meshNode);
        } 
        scene->SetNodeName(node->mName.data);
        cout << "Adding scenenode with name: " << node->mName.data << " with " << node->mNumMeshes << " number of meshes" << endl;
        current->AddNode(scene);
        current = scene;

        // Associate node name with the transformation node we just created.
        if( transMap.find(node->mName.data) == transMap.end() ){
            transMap[node->mName.data] = tn;
        }else{
            logger.warning << "Duplicate MeshNode with name " << node->mName.data << " exists." << logger.end;
        }
    }

    // Go on and read nodes recursively.
    for (i = 0; i < node->mNumChildren; ++i) {
        ReadNode(node->mChildren[i], current);
    }
}


void AssimpResource::ReadAnimations(aiAnimation** ani, unsigned int size) {

    if( size > 0 ){
        animRoot = new AnimationNode();
        cout << "AnimationRoot: " << animRoot << endl;
        //root->AddNode(animationRoot);
        animRoot->SetNodeName("Animation Root"); 
        
        for( unsigned int animIdx = 0; animIdx < size; animIdx++ ){
            aiAnimation* anim = ani[animIdx];

            Animation* animation = new Animation();
            animation->SetName(anim->mName.data);
            animation->SetDuration(anim->mDuration*1000000);
            animation->SetTicksPerSecond(anim->mTicksPerSecond);

            AnimationNode* animNode = new AnimationNode(animation);
            animRoot->AddNode(animNode);

            cout << "Animation " << animIdx << ":" << endl;
            cout << "Name: " << anim->mName.data << endl;
            cout << "Num bone channels: " << anim->mNumChannels << endl;
            cout << "Num mesh channels: " << anim->mNumMeshChannels << endl;
            cout << "Duration: " << anim->mDuration << endl;
            cout << "Ticks per sec:" << anim->mTicksPerSecond << endl;
        
            aiNodeAnim** boneList = anim->mChannels;
            for( unsigned int boneIdx = 0; boneIdx < anim->mNumChannels; boneIdx++ ){
                aiNodeAnim* bone = boneList[boneIdx];
            
                // Check that the transformation node being animated exists.
                map<std::string, TransformationNode*>::iterator itr;
                if( (itr=transMap.find(bone->mNodeName.data)) != transMap.end() ) {
                    // Create animated transformation node.
                    AnimatedTransformation* animTrans = new AnimatedTransformation(itr->second);
                    animTrans->SetName(bone->mNodeName.data);

                    AnimatedTransformationNode* animTransNode = new AnimatedTransformationNode(animTrans);
                    animTransNode->SetNodeName(animTrans->GetName().append("\n[AnimTransNode]"));
                    animNode->AddNode(animTransNode);

                    cout << "Bone " << boneIdx << ":" << endl;
                    cout << "    Name of affected node: " << bone->mNodeName.data << endl; 
                    cout << "    Addr of affected node: " << itr->second << endl;
                    cout << "    Num position keys: " << bone->mNumPositionKeys << endl;
                    cout << "    Num rotation keys: " << bone->mNumRotationKeys << endl;
                    cout << "    Num scaling  keys: " << bone->mNumScalingKeys << endl;
            

                    // Add all rotation key/value pairs the animated transformation node.
                    aiQuatKey* rotKeyList = bone->mRotationKeys;
                    for( unsigned int rotKeyIdx = 0; rotKeyIdx < bone->mNumRotationKeys; rotKeyIdx++ ){
                        aiQuatKey rot = rotKeyList[rotKeyIdx];
                        
                        // Assuming time is in seconds, convert to micro sec - TODO: calc ticks.
                        unsigned int usec = rot.mTime * 1000000.0;

                        animTrans->AddRotationKey(usec, Quaternion<float>(rot.mValue.w, 
                                                                          rot.mValue.x, 
                                                                          rot.mValue.y, 
                                                                          rot.mValue.z) );

                        //cout << "Rotation Key " << rotKeyIdx << " time: " << usec << " quat(x,y,z,w): " << rot.mValue.x << ", " << rot.mValue.y << ", " << rot.mValue.z << ", " << rot.mValue.w << endl;
                    }

                    // Add all position key/value pairs to the animated transformation node.
                    aiVectorKey* posKeyList = bone->mPositionKeys;
                    for( unsigned int posKeyIdx = 0; posKeyIdx < bone->mNumPositionKeys; posKeyIdx++ ){
                        aiVectorKey pos = posKeyList[posKeyIdx];
                        aiVector3D  vec = pos.mValue;

                        // Assuming time is in seconds, convert to micro sec - TODO: calc ticks.
                        unsigned int usec = pos.mTime * 1000000.0;

                        animTrans->AddPositionKey(usec, Vector<3,float>(vec.x, vec.y, vec.z));

                        cout << "Position Key" << posKeyIdx << " time: " << usec << " pos (x,y,z): " << vec.x << ", " << vec.y << ", " << vec.z << endl;
                    }

                    // Add the animated transformation (aka channel).
                    animation->AddAnimatedTransformation(animTrans);
                }
                else {
                    cout << "Warning: could not find transformation with name: " << bone->mNodeName.data << endl;
                }
            }
        }
    }
}


void AssimpResource::ReadAnimatedMeshes(aiMesh** ms, unsigned int size) {
    for(unsigned int i=0; i<size; i++){
        aiMesh* mesh = ms[i];

        if(mesh->HasBones()){
            // Find the MeshPtr representing the aiMesh.
            map<aiMesh*, OpenEngine::Geometry::MeshPtr>::iterator res;
            if( (res=meshMap.find(mesh))!=meshMap.end() ){
                cout << "MeshPtr found" << endl;
                
                // Get MeshPtr
                OpenEngine::Geometry::MeshPtr meshPtr = res->second;
                // Create animated mesh.
                AnimatedMesh* animMesh = new AnimatedMesh(meshPtr);
                AnimatedMeshNode* animMeshNode = new AnimatedMeshNode(animMesh);
            

                cout << "numBones: " << mesh->mNumBones << endl;
                // Iterate through all bones
                for(unsigned int b=0; b<mesh->mNumBones; b++){
                    const aiBone* aib = mesh->mBones[b];
                    
                    // Find transformation node associated with this bone.
                    map<std::string, TransformationNode*>::iterator itr;
                    if( (itr=transMap.find(aib->mName.data))==transMap.end() ){
                        logger.warning << "Could not find transformation node associated with bone" << logger.end;
                        continue;
                    }
                    // We found the associated transformation node, now create bone.
                    TransformationNode* boneTrans = itr->second;
                    Bone* bone = new Bone(boneTrans);

                    // Add weights to bone, this defines how much influence the
                    // bone has on each affected vertex.
                    for(unsigned int w=0; w<aib->mNumWeights; w++){
                        aiVertexWeight weight = aib->mWeights[w];
                        bone->AddWeight(weight.mVertexId, weight.mWeight);
                    }
                    // Set offset matrix on bone. Matrix that transforms 
                    // from mesh space to bone space in bind pose. 
                    aiMatrix4x4 aiom = aib->mOffsetMatrix;
                    Matrix<4,4,float> offset(aiom.a1, aiom.a2, aiom.a3, aiom.a4,
                                             aiom.b1, aiom.b2, aiom.b3, aiom.b4,
                                             aiom.c1, aiom.c2, aiom.c3, aiom.c4,
                                             aiom.d1, aiom.d2, aiom.d3, aiom.d4); 
                    cout << b << " bone offset matrix: " << offset << endl;

                    bone->SetOffsetMatrix(offset);

                    // Add bone as mesh deformer to animated mesh.
                    animMesh->AddMeshDeformer(bone);
                }

                // Add animated mesh node to animation root.
                animRoot->AddNode(animMeshNode); 

            }
        }
    }
}

} // NS Resources
} // NS OpenEngine
