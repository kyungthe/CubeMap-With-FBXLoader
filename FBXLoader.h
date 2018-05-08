#ifndef _FBXLOADER_H_
#define _FBXLOADER_H_

#define OK		1
#define FAIL	0

#include <fbxsdk.h>
#include <vector>
#include <string>

typedef bool RESULT;

struct FBXLoaderVertex
{
	FbxVector4 Pos;
	FbxVector4 Normal;
	FbxVector2 TexC;
};

class FBXLoader
{
private:
	std::vector<FBXLoaderVertex> mVertexs;
	std::vector<std::uint16_t> mIndices;
	std::string mTexturePath;

	void InitializeSdkObjects(FbxManager*& pManager, FbxScene*& pScene);

	void Setup();
	void SetupNode(FbxNode* pNode);
	void CopyVertexData(FbxMesh* pMesh);
	void CopyTextureData(FbxSurfaceMaterial* mat);

public:
	// FBX SDK
	FbxManager* mSdkManager;
	FbxScene*	mScene;
	FbxImporter* mImporter;

	FBXLoader();
	~FBXLoader();

	decltype(mVertexs) GetVertexs() { return mVertexs; }
	decltype(mIndices) GetIndices() { return mIndices; }
	std::string GetTexturePath() { return mTexturePath; }

	RESULT LoadFBX(const char* filename);
};


#endif // !_FBXLOADER_H_
