#include "FBXLoader.h"
#include <unordered_set>

FBXLoader::FBXLoader()
{
	mSdkManager = nullptr;
	mScene = nullptr;
	mImporter = nullptr;
}

FBXLoader::~FBXLoader()
{
	if (mImporter)
	{
		mImporter->Destroy();
		mImporter = nullptr;
	}

	if (mScene)
	{
		mScene->Destroy();
		mScene = nullptr;
	}

	if (mSdkManager)
	{
		mSdkManager->Destroy();
		mSdkManager = nullptr;
	}
}

bool FBXLoader::LoadFBX(const char* filename)
{
	if (!filename)
		return FAIL;

	RESULT result = OK;

	InitializeSdkObjects(mSdkManager, mScene);
	if (!mSdkManager)
		return FAIL;

	int lFileFormat = -1;
	mImporter = FbxImporter::Create(mSdkManager, "");

	if (!mSdkManager->GetIOPluginRegistry()->DetectReaderFileFormat(filename, lFileFormat))
	{
		// Unrecognizable file format. Try to fall back to FbxImporter::eFBX_BINARY
		lFileFormat = mSdkManager->GetIOPluginRegistry()->FindReaderIDByDescription("FBX binary (*.fbx)");;
	}

	// Initialize the importer by providing a filename.
	if (!mImporter || mImporter->Initialize(filename, lFileFormat) == false)
		return FAIL;

	if (!mImporter || mImporter->Import(mScene) == false)
		return FAIL;

	FbxAxisSystem OurAxisSystem = FbxAxisSystem::DirectX;

	FbxAxisSystem SceneAxisSystem = mScene->GetGlobalSettings().GetAxisSystem();
	if (SceneAxisSystem != OurAxisSystem)
	{
		FbxAxisSystem::DirectX.ConvertScene(mScene);
	}

	FbxSystemUnit SceneSystemUnit = mScene->GetGlobalSettings().GetSystemUnit();
	if (SceneSystemUnit.GetScaleFactor() != 1.0)
	{
		FbxSystemUnit::cm.ConvertScene(mScene);
	}

	Setup();

	return result;
}

void FBXLoader::InitializeSdkObjects(FbxManager*& pManager, FbxScene*& pScene)
{
	//The first thing to do is to create the FBX Manager which is the object allocator for almost all the classes in the SDK
	pManager = FbxManager::Create();
	if (!pManager)
	{
		FBXSDK_printf("Error: Unable to create FBX Manager!\n");
		exit(1);
	}
	else FBXSDK_printf("Autodesk FBX SDK version %s\n", pManager->GetVersion());

	//Create an IOSettings object. This object holds all import/export settings.
	FbxIOSettings* ios = FbxIOSettings::Create(pManager, IOSROOT);
	pManager->SetIOSettings(ios);

	//Load plugins from the executable directory (optional)
	FbxString lPath = FbxGetApplicationDirectory();
	pManager->LoadPluginsDirectory(lPath.Buffer());

	//Create an FBX scene. This object holds most objects imported/exported from/to files.
	pScene = FbxScene::Create(pManager, "My Scene");
	if (!pScene)
	{
		FBXSDK_printf("Error: Unable to create FBX scene!\n");
		exit(1);
	}
}

void FBXLoader::Setup()
{
	if (mScene->GetRootNode())
	{
		SetupNode(mScene->GetRootNode());
	}
}

void FBXLoader::SetupNode(FbxNode * pNode)
{
	if (!pNode)
		return;

	FbxMesh* mesh = pNode->GetMesh();

	// �޽��� ������
	if (mesh)
	{
		// ���ؽ� ������ ���Ѵ�
		const int vertexCount = mesh->GetControlPointsCount();

		// ���ؽ��� ������ ������
		if (vertexCount > 0)
		{
			CopyVertexData(mesh);
		}
	}

	const int materialCount = pNode->GetMaterialCount();
	for (int i = 0; i < materialCount; i++)
	{
		FbxSurfaceMaterial* mat = pNode->GetMaterial(i);
		if (mat)
		{
			CopyTextureData(mat);
		}		
	}

	const int childCount = pNode->GetChildCount();
	for (int i = 0; i < childCount; i++)
	{
		SetupNode(pNode->GetChild(i));
	}
}

/*
	�޽��� ������ �ؽ�ó�� 1���� �����ϰ� ����(?)
*/
void FBXLoader::CopyVertexData(FbxMesh* pMesh)
{
	// �޽��� ������ ������ ���Ѵ�
	int polygonCount = pMesh->GetPolygonCount();
	std::unordered_set<int> indexSet;
	FbxStringList uvSetNameList;
	pMesh->GetUVSetNames(uvSetNameList);
	bool unmapped = false;

	for (int pol = 0; pol < polygonCount; pol++)
	{
		// ������ �ȿ� �ִ� ���ؽ��� ������ ���Ѵ�
		int vertexCount = pMesh->GetPolygonSize(pol);

		for (int vtx = 0; vtx < vertexCount; vtx++)
		{
			// ���ؽ��� �ε����� ���Ѵ�
			int index = pMesh->GetPolygonVertex(pol, vtx);

			// �ε����¿� ���� �ε����� ������(���ؽ��� ������)
			if (indexSet.find(index) == indexSet.end())
			{
				// �ε����¿� �ε����� �����Ѵ�
				indexSet.insert(index);

				// ���ؽ� ��ǥ�� ���Ѵ�
				FBXLoaderVertex vertex;
				vertex.Pos = pMesh->GetControlPointAt(index);

				// �븻 ���� ���Ѵ�
				FbxVector4 normal;
				pMesh->GetPolygonVertexNormal(pol, vtx, normal);
				vertex.Normal = normal;

				// �ؽ�ó ��ǥ�� ���Ѵ�
				FbxString uvSetName = uvSetNameList.GetStringAt(0);
				pMesh->GetPolygonVertexUV(pol, vtx, uvSetName, vertex.TexC, unmapped);

				mVertexs.push_back(vertex);
			}

			mIndices.push_back(index);
		}
	}
}

/*
	Diffuse�� ����
	�ؽ�ó 1���� ����
	���̾ƿ� �ؽ�ó(http://help.autodesk.com/view/FBX/2018/ENU/?guid=FBX_Developer_Help_meshes_materials_and_textures_textures_layered_textures_html) ���� �� ��
*/
void FBXLoader::CopyTextureData(FbxSurfaceMaterial * mat)
{
	const FbxProperty property = mat->FindProperty(FbxSurfaceMaterial::sDiffuse);

	if (property.IsValid())
	{
		const int textureCount = property.GetSrcObjectCount<FbxFileTexture>();

		for (int i = 0; i < textureCount; i++)
		{
			FbxFileTexture* fileTexture = property.GetSrcObject<FbxFileTexture>(i);
			
			if (fileTexture)
			{
				mTexturePath = fileTexture->GetFileName();
			}
		}
	}
}

/*
	���ؽ� ���ϴ� ����ȭ�� ���� �Լ�
*/

//void FBXLoader::CopyVertexData(FbxMesh* pMesh)
//{
//	// �޽��� ������ ������ ���Ѵ�
//	int polygonCount = pMesh->GetPolygonCount();
//	unsigned int indx = 0;
//
//	for (int pol = 0; pol < polygonCount; pol++)
//	{
//		// ������ �ȿ� �ִ� ���ؽ��� ������ ���Ѵ�
//		int vertexCount = pMesh->GetPolygonSize(pol);
//
//		for (int vtx = 0; vtx < vertexCount; vtx++)
//		{
//			int index = pMesh->GetPolygonVertex(pol, vtx);
//			mIndices.push_back(indx);
//
//			FBXLoaderVertex vertex;
//			vertex.Pos = pMesh->GetControlPointAt(index);
//
//			FbxVector4 normal;
//			pMesh->GetPolygonVertexNormal(pol, vtx, normal);
//			vertex.Normal = normal;
//
//			mVertexs.push_back(vertex);
//
//			++indx;
//		}
//	}
//
//	FbxStringList uvSetNameList;
//	pMesh->GetUVSetNames(uvSetNameList);
//	bool unmapped = false;
//
//	int k = 0;
//	for (int pol = 0; pol < polygonCount; pol++)
//	{
//		int vertexCount = pMesh->GetPolygonSize(pol);
//
//		for (int vtx = 0; vtx < vertexCount; vtx++, k++)
//		{
//			FbxString uvSetName = uvSetNameList.GetStringAt(0);
//			pMesh->GetPolygonVertexUV(pol, vtx, uvSetName, mVertexs[k].TexC, unmapped);
//		}
//	}
//}