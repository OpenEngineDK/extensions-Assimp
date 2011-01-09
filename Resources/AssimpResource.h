// OpenCollada Model resource.
// -------------------------------------------------------------------
// Copyright (C) 2007 OpenEngine.dk (See AUTHORS) 
// Modified by Anders Bach Nielsen <abachn@daimi.au.dk> - 21. Nov 2007
// 
// This program is free software; It is covered by the GNU General 
// Public License version 2 or any later version. 
// See the GNU General Public License for more details (see LICENSE). 
//--------------------------------------------------------------------

#ifndef _OE_ASSIMP_RESOURCE_H_
#define _OE_ASSIMP_RESOURCE_H_

#include <Resources/IModelResource.h>
#include <Resources/IResourcePlugin.h>
#include <Geometry/Mesh.h>
#include <Scene/MeshNode.h>
#include <Scene/TransformationNode.h>
#include <Geometry/Material.h>
#include <Geometry/GeometrySet.h>
#include <Resources/DataBlock.h>

#include <Math/Vector.h>

#include <string>
#include <vector>
#include <map>

// assimp
#include <assimp.hpp>      // C++ importer interface
#include <aiScene.h>       // Output data structure
#include <aiPostProcess.h> // Post processing flags

//forward declarations
namespace OpenEngine {
    namespace Scene {
        class ISceneNode;
        class TransformationNode;
        class AnimationNode;
    }
namespace Resources {
    class ITexture2D;
    typedef boost::shared_ptr<ITexture2D> ITexture2DPtr;

    using namespace Geometry;
    using std::string;
    using std::vector;
    

/**
 * Assimp model resource.
 *
 * @class AssimpResource AssimpResource.h "AssimpResource.h"
 */
class AssimpResource : public IModelResource {
private: 
    string file, dir;
    ISceneNode* root;
    AnimationNode* animRoot;

    vector<MeshPtr> meshes;
    vector<MaterialPtr> materials;

    map<std::string, OpenEngine::Scene::TransformationNode*> transMap;
    map<aiMesh*, OpenEngine::Geometry::MeshPtr> meshMap;

    void Error(string msg);
    void Warning(string msg);

    void ReadMeshes(aiMesh** ms, unsigned int size);
    void ReadMaterials(aiMaterial** ms, unsigned int size);
    void ReadScene(const aiScene* scene);
    void ReadNode(aiNode* node, ISceneNode* parent);

    void ReadAnimations(aiAnimation** ani, unsigned int size);
    void ReadAnimatedMeshes(aiMesh** ms, unsigned int size);

public:
    AssimpResource(string file);
    ~AssimpResource();
    void Load();
    void Unload();
    ISceneNode* GetSceneNode();

    ISceneNode* GetMeshes();
    AnimationNode* GetAnimations();
};

/**
 * Assimp resource plug-in.
 *
 * @class AssimpPlugin AssimpResource.h "AssimpResource.h"
 */
class AssimpPlugin : public IResourcePlugin<IModelResource> {
public:
	AssimpPlugin();
    IModelResourcePtr CreateResource(string file);
};

} // NS Resources
} // NS OpenEngine

#endif // _OE_ASSIMP_RESOURCE_H_
