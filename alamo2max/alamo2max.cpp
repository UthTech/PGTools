#include <max.h>
#include <IParamB2.h> 
#include <IGame/IGame.h>
#include <IDxMaterial.h>
#include <ISkin.h>
#include <pbbitmap.h>
#include <modstack.h>

#include <shobjidl.h>
#include <shlguid.h>
#include <shlwapi.h>
#include <algorithm>
#include <vector>
#include <set>
#include "Model.h"
#include "Exceptions.h"
using namespace std;
using namespace Alamo;

static const Class_ID AlamoImporterID		= Class_ID(0x144A51BB, 0x024D0F1A);
static const Class_ID AlamoExporterID		= Class_ID(0x31F5AE34, 0x7E2D36AA);
static const Class_ID AlamoProxyID			= Class_ID(0x52263841, 0x08194485);
static const Class_ID AlamoUtilityID		= Class_ID(0x70A24090, 0x60C90F03);
static const Class_ID DXMATERIAL_CLASS_ID	= Class_ID(0x0ED995E4, 0x6133DAF2);

// Settings
static const float MAX_COINCIDENT_DIST	  = 0.001f;		// Weld vertices closer than this together
static const float MAX_COINCIDENT_DIST_SQ = MAX_COINCIDENT_DIST * MAX_COINCIDENT_DIST;
static const float MAX_DEVIATION          = 0.00001f;	// If a vector's Y and Z deviate less than this, it's in the line
static const float FPS                    = 30.0f;
static const int   NUM_BONES_PER_VERTEX   = 4;

struct ANIMATION_INFO
{
	char Text[256];
	int  StartFrame;
	int  EndFrame;
	int  Unknown[4];
};

struct Files
{
	vector< pair<Alamo::Animation*, string> > animations;
	Model* model;
	
	Files() : model(NULL) {}
	~Files()
	{
		delete model;
		for (size_t i = 0; i < animations.size(); i++)
		{
			delete animations[i].first;
		}
	}
};

struct FaceInfo : Face
{
	map<size_t, bool>	connections;
	bool				visited;

	void smooth(size_t f)
	{
		map<size_t,bool>::iterator p = connections.find(f);
		if (p != connections.end())
		{
			p->second = true;
		}
	}

	FaceInfo() : visited(false) {}
};

struct FaceGroup
{
	vector<size_t>	faces;
	set<size_t>		connections;
	int				color;

	FaceGroup() : color(0) {}
};

static string StripExtension(const string& filename)
{
	TCHAR* ext = PathFindExtension(filename.c_str());
	return filename.substr(0, ext - filename.c_str());
}

static bool equalsGeometry(const Vertex& v1, const Vertex& v2)
{
	if ((v1.Position - v2.Position).LengthSquared() > MAX_COINCIDENT_DIST_SQ)
	{
		return false;
	}

	for (int i = 0; i < NUM_BONES_PER_VERTEX; i++)
	{
		if ((v1.BoneIndices[i] != v2.BoneIndices[i]) || fabs(v2.BoneWeights[i] - v1.BoneWeights[i]) < 0.01f)
		{
			return false;
		}
	}

	return true;
}

class AlamoImporter : public SceneImport
{
	typedef vector< vector<int> > VertexMaps;

	struct BONEINFO
	{
		Object*  object;
		ImpNode* impnode;
		INode*   inode;

		BONEINFO() : object(NULL), inode(NULL) {}
	};

	vector<BONEINFO>	m_bones;
	string				MapPath;

public:
	int				ExtCount()		     { return 1; }
	const TCHAR *	Ext(int n)		     { return "alo"; }
	const TCHAR *	LongDesc()		     { return "Alamo Object File"; }
	const TCHAR *	ShortDesc()		     { return "Alamo Object"; }
	const TCHAR *	AuthorName()	     { return "Mike Lankamp"; }
	const TCHAR *	CopyrightMessage()   { return "Copyright 2006 Mike Lankamp"; }
	const TCHAR *	OtherMessage1()      { return ""; }
	const TCHAR *	OtherMessage2()      { return ""; }
	unsigned int	Version()            { return 0x0100; }
	void			ShowAbout(HWND hWnd) {};

	static string GetLinkTarget(const string& filename)
	{
		IShellLink* psl = NULL;
		if (FAILED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&psl)))
		{
			return "";
		}

		IPersistFile* ppf = NULL; 
        if (FAILED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf)))
		{
			psl->Release();
			return "";
		}


		WCHAR linkpath[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, filename.c_str(), -1, linkpath, MAX_PATH);
		if (FAILED(ppf->Load(linkpath, 0)))
		{
			ppf->Release();
			psl->Release();
			return "";
		}
		ppf->Release();

		TCHAR path[MAX_PATH];
		if (FAILED(psl->GetPath(path, MAX_PATH, NULL, 0)))
		{
			psl->Release();
			return "";
		}
		psl->Release();

		return path;
	}

	static string FindFile(const string& basepath, const string& name, const TCHAR* ext1, const TCHAR* ext2)
	{
		// Check if this path contains it
		string filename, basename = StripExtension(name);
		filename = basepath + name;                  if (PathFileExists(filename.c_str())) return filename;
		filename = basepath + basename + "." + ext1; if (PathFileExists(filename.c_str())) return filename;
		filename = basepath + basename + "." + ext2; if (PathFileExists(filename.c_str())) return filename;

		// No, check subdirectories
		WIN32_FIND_DATA ffd;
		string search = basepath + "*.*";
		HANDLE hFind = FindFirstFile(search.c_str(), &ffd);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (strcmp(ffd.cFileName,".") != 0 && strcmp(ffd.cFileName,"..") != 0)
				{
					if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					{
						// This is a directory
						if ((filename = FindFile(basepath + ffd.cFileName + "\\", name, ext1, ext2)) != "")
						{
							return filename;
						}
					}
					else if ((filename = GetLinkTarget(basepath + ffd.cFileName)) != "")
					{
						// This a shortcut, search there as well
						if ((filename = FindFile(filename + "\\", name, ext1, ext2)) != "")
						{
							return filename;
						}
					}
				}
			} while (FindNextFile(hFind, &ffd));
			FindClose(hFind);
		}
		return "";
	}

	void AssignParameter(IParamBlock2* ipb, ParamID id, const Alamo::EffectParameter& param)
	{
		ParamDef& def = ipb->GetParamDef(id);
		switch (def.type)
		{
			case TYPE_FLOAT:	ipb->SetValue(id, 0, param.m_float); break;
			case TYPE_INT:		ipb->SetValue(id, 0, param.m_integer); break;
			case TYPE_FRGBA:	ipb->SetValue(id, 0, AColor(param.m_float4)); break;
			case TYPE_BITMAP:
			{
				string filename = FindFile(MapPath + "EaW\\", param.m_textureName, "tga", "dds");
				if (filename != "")
				{
					BitmapInfo bi((TCHAR*)filename.c_str());
					PBBitmap* bmp = new PBBitmap(bi);
					bmp->Load();
					ipb->SetValue(id, 0, bmp);
				}
				break;
			}
		}
	}

	Mtl* CreateDX9Material(const Alamo::Material& material)
	{
		string filename = FindFile(MapPath + "EaW\\", material.m_effectName, "fx", "fxo");

		// Create DirectX 9 Shader material
		Mtl* mtl = (Mtl*)CreateInstance(MATERIAL_CLASS_ID, DXMATERIAL_CLASS_ID);
		if (mtl == NULL)
		{
			return NULL;
		}

		// Get the DirectX interface
		IDxMaterial* dxmat = (IDxMaterial*)mtl->GetInterface(IDXMATERIAL_INTERFACE);
		if (dxmat == NULL)
		{
			delete mtl;
			return NULL;
		}

		// Specify the effect file
		dxmat->SetEffectFilename((TCHAR*)filename.c_str());
		dxmat->ReloadDXEffect();

		if (filename != "")
		{
			// Get the parameter list
			IParamBlock2* ipb = mtl->GetParamBlock(0);
			if (ipb == NULL)
			{
				delete mtl;
				return NULL;
			}

			// Assign the parameters
			for (int i = 0; i < ipb->NumParams(); i++)
			{
				ParamID    id = ipb->IndextoID(i);
				ParamDef& def = ipb->GetParamDef(id);
				
				// Find the parameter in the material
				for (size_t p = 0; p < material.m_parameters.size(); p++)
				{
					if (_stricmp(def.int_name, material.m_parameters[p].m_name.c_str()) == 0)
					{
						AssignParameter(ipb, id, material.m_parameters[p]);
						break;
					}
				}
			}
		}

		return mtl;
	}

	Mtl* CreateMaterial(const Alamo::Mesh& mesh)
	{
		// Create multi or single material
		Mtl* mtl = NULL;
		int numMaterials = (int)mesh.m_materials.size();
		if (numMaterials > 0)
		{
			if (numMaterials > 1)
			{
				MultiMtl* multimtl = NewDefaultMultiMtl();
				multimtl->SetNumSubMtls(numMaterials);
				for (int m = 0; m < numMaterials; m++)
				{
					multimtl->SetSubMtl(m, CreateDX9Material(mesh.m_materials[m]));
				}
				mtl = multimtl;
			}
			else
			{
				mtl = CreateDX9Material(mesh.m_materials[0]);
			}
		}
		return mtl;
	}

	void AddFacesToGroup(FaceGroup& group, DWORD smGroup, vector<FaceInfo>& faces, size_t f)
	{
		group.faces.push_back(f);
		FaceInfo& face = faces[f];
		face.visited = true;
		face.smGroup = smGroup;
		for (map<size_t, bool>::iterator i = face.connections.begin(); i != face.connections.end(); i++)
		{
			if (!i->second)
			{
				// Carry the connection over to the group
				group.connections.insert(i->first);
			}
			else if (!faces[i->first].visited)
			{
				// This face is part of the same group, recurse!
				AddFacesToGroup(group, smGroup, faces, i->first);
			}
		}
	}


	//
	// Calculate smoothing groups.
	// Faces that share a vertex (i.e, have no split vertices) are part of the
	// same smoothing group. This way the edge between them does not appear hard.
	//
	void SetSmoothingGroups(::Mesh& maxmesh, const Alamo::Mesh& mesh, vector<FaceInfo>& faces)
	{
		int numFaces = maxmesh.getNumFaces();
		for (int i = 0; i < numFaces; i++)
		{
			const Face& f1 = maxmesh.faces[i];
			for (int j = i + 1; j < numFaces; j++)
			{
				const Face& f2 = maxmesh.faces[j];
				if ((f1.v[0] == f2.v[0] || f1.v[0] == f2.v[1] || f1.v[0] == f2.v[2]) ||
					(f1.v[1] == f2.v[0] || f1.v[1] == f2.v[1] || f1.v[1] == f2.v[2]) ||
					(f1.v[2] == f2.v[0] || f1.v[2] == f2.v[1] || f1.v[2] == f2.v[2]))
				{
					// f1 and f2 share a vertex and are thus connected
					faces[i].connections.insert(make_pair(j, false));
					faces[j].connections.insert(make_pair(i, false));
				}
			}
		}

		int facebase = 0;
		for (int m = 0; m < mesh.m_materials.size(); m++)
		{
			const Alamo::Material& mat = mesh.m_materials[m];
			for (int i = 0; i < mat.m_indices.size(); i += 3)
			{
				const uint16_t* f1 = &mat.m_indices[i];
				for (int j = i + 3; j < mat.m_indices.size(); j += 3)
				{
					const uint16_t* f2 = &mat.m_indices[j];
					if ((f1[0] == f2[0] || f1[0] == f2[1] || f1[0] == f2[2]) ||
						(f1[1] == f2[0] || f1[1] == f2[1] || f1[1] == f2[2]) ||
						(f1[2] == f2[0] || f1[2] == f2[1] || f1[2] == f2[2]))
					{
						// f1 and f2 share a vertex and are thus smoothed
						faces[facebase + i/3].smooth(facebase + j/3);
						faces[facebase + j/3].smooth(facebase + i/3);
					}
				}
			}
			facebase += (int)mat.m_indices.size() / 3;
		}

		// Create groups
		vector<FaceGroup> groups;
		for (int i = 0; i < numFaces; i++)
		{
			if (!faces[i].visited)
			{
				groups.push_back(FaceGroup());
				AddFacesToGroup(groups.back(), (DWORD)groups.size() - 1, faces, i);
			}
		}

		// Simplify group connections (and don't connect to ignored groups)
		for (size_t i = 0; i < groups.size(); i++)
		{
			FaceGroup& group = groups[i];
			set<size_t> connections;
			for (set<size_t>::const_iterator p = group.connections.begin(); p != group.connections.end(); p++)
			{
				size_t smGroup = (size_t)faces[*p].smGroup;
				if (groups[smGroup].color >= 0)
				{
					connections.insert(smGroup);
				}
			}
			group.connections = connections;
		}

		// Create the initial reference template
		set<int> initial;
		for (int i = 1; i <= 32; i++)
		{
			initial.insert(i);
		}

		// Calculate colors
		for (size_t i = 0; i < groups.size(); i++)
		{
			FaceGroup& group = groups[i];
			if (group.color >= 0)
			{
				set<int> available = initial;
				for (set<size_t>::const_iterator p = group.connections.begin(); p != group.connections.end(); p++)
				{
					if (groups[*p].color > 0)
					{
						available.erase(groups[*p].color);
					}
				}
				group.color = *available.begin();
			}
		}

		// Assign smoothing groups
		for (size_t i = 0; i < groups.size(); i++)
		{
			FaceGroup& group = groups[i];
			for (size_t j = 0; j < group.faces.size(); j++)
			{
				DWORD color = (group.color > 0) ? 1 << (group.color - 1) : 0;
				maxmesh.faces[group.faces[j]].setSmGroup(color);
			}
		}
	}

	void SetGeometry(::Mesh& maxmesh, const Alamo::Mesh& mesh, VertexMaps& vertexMaps)
	{
		vertexMaps.clear();
		vertexMaps.resize(mesh.m_materials.size());

		//
		// Get vertex and face counts
		//
		int totalVerts = 0, totalTVerts = 0;
		int totalFaces = 0, totalTFaces = 0;
		for (size_t m = 0; m < mesh.m_materials.size(); m++)
		{
			totalFaces  += (int)mesh.m_materials[m].m_indices.size() / 3;
			totalTFaces += (int)mesh.m_materials[m].m_indices.size() / 3;
			totalVerts  += (int)mesh.m_materials[m].m_vertices.size();
			totalTVerts += (int)mesh.m_materials[m].m_vertices.size();
		}
		
		//
		// Build vertices list (weld coincident vertices)
		//
		maxmesh.setNumVerts(totalVerts);
		totalVerts = 0;
		for (int m = 0; m < mesh.m_materials.size(); m++)
		{
			const Alamo::Material& mat = mesh.m_materials[m];

			vector<int>& vertexMap = vertexMaps[m];
			vertexMap.resize(mat.m_vertices.size());

			for (int i = 0; i < mat.m_vertices.size(); i++)
			{
				const Alamo::Vertex& vertex = mat.m_vertices[i];

				// Find if the vertex already exists
				int j;
				for (j = 0; j < i; j++)
				{
					if (equalsGeometry(vertex, mat.m_vertices[j]))
					{
						// Yes
						vertexMap[i] = vertexMap[j];
						break;
					}
				}

				if (j == i)
				{
					// No, add it
					vertexMap[i] = totalVerts;
					maxmesh.setVert(totalVerts++, vertex.Position);
				}
			}
		}
		maxmesh.setNumVerts(totalVerts, true);

		//
		// Initialize geometry faces
		//
		vector<FaceInfo> faces(totalFaces);
		maxmesh.setNumFaces(totalFaces);
		totalFaces = 0;
		int facebase = 0;
		for (int m = 0; m < mesh.m_materials.size(); m++)
		{
			const Alamo::Material& mat = mesh.m_materials[m];
			vector<int>& vertexMap = vertexMaps[m];
			for (size_t i = 0; i < mat.m_indices.size(); i += 3)
			{
				faces[totalFaces].setVerts(facebase + mat.m_indices[i+0], facebase + mat.m_indices[i+1], facebase + mat.m_indices[i+2]);
				maxmesh.faces[totalFaces].setMatID((MtlID)m);
				maxmesh.faces[totalFaces++].setVerts(
					vertexMap[mat.m_indices[i+0]],
					vertexMap[mat.m_indices[i+1]],
					vertexMap[mat.m_indices[i+2]]
				);
			}
			facebase += (int)mat.m_indices.size() / 3;
		}

		//
		// Set texture vertices and faces for the four channels
		//
		for (int ch = 0; ch < 4; ch++)
		{
			maxmesh.setMapSupport(ch + 1);
			maxmesh.setNumMapVerts(ch + 1, totalTVerts);
			maxmesh.setNumMapFaces(ch + 1, totalTFaces);
			UVVert* verts = maxmesh.mapVerts(ch + 1);
			TVFace* faces = maxmesh.mapFaces(ch + 1);
			if (verts != NULL && faces != NULL)
			{
				totalTVerts = 0;
				totalTFaces = 0;
				for (int m = 0; m < mesh.m_materials.size(); m++)
				{
					const Alamo::Material& mat = mesh.m_materials[m];
					
					for (int i = 0; i < mat.m_indices.size(); i += 3)
					{
						faces[totalTFaces++].setTVerts(
							totalTVerts + mat.m_indices[i+0],
							totalTVerts + mat.m_indices[i+1],
							totalTVerts + mat.m_indices[i+2]
						);
					}

					for (size_t i = 0; i < mat.m_vertices.size(); i++)
					{
						verts[totalTVerts++].Set(mat.m_vertices[i].TexCoord[ch].x, 1.0f - mat.m_vertices[i].TexCoord[ch].y, 0.0f);
					}
				}
			}
		}

		SetSmoothingGroups(maxmesh, mesh, faces);
	}

	ImpNode* CreateMesh(const Model* model, const Alamo::Mesh& mesh, ImpInterface* ii)
	{
		Mtl*        mtl = CreateMaterial(mesh);
		TriObject*  tri = CreateNewTriObject();
		::Mesh& maxmesh = tri->GetMesh();

		//
		// Create the basic mesh geometry
		//
		VertexMaps vertexMaps;
		SetGeometry(maxmesh, mesh, vertexMaps);

		//
		// Set mesh properties
		//
		ImpNode* node = ii->CreateNode();
		INode*  inode = node->GetINode();
		node->SetName(mesh.m_name.c_str());	// Set mesh name
		node->Reference(tri);				// Set mesh geometry
		inode->SetMtl(mtl);					// Set mesh material

        if (mesh.m_bone > 0)
        {
            // Attach the mesh to its bone
            m_bones[mesh.m_bone].inode->AttachChild(inode);
        }

		// Set Alamo properties
		inode->SetUserPropBool("Alamo_Export_Transform", true );
		inode->SetUserPropBool("Alamo_Export_Geometry",  true );
		inode->SetUserPropBool("Alamo_Collision_Enabled", mesh.m_collision);
		inode->SetUserPropBool("Alamo_Geometry_Hidden",   mesh.m_hidden);
		inode->SetUserPropBool("Alamo_Alt_Decrease_Stay_Hidden", false);
		if (mesh.m_bone >= 0)
		{
			// Set the transform
			node->SetTransform(0, model->GetTransform(mesh.m_bone));
			inode->SetUserPropInt("Alamo_Billboard_Mode", model->m_bones[mesh.m_bone].m_billboarding);
		}

		//
		// Initialize skin
		//

		// Find out which bones this mesh uses
		set<int> bones;
		for (int m = 0; m < mesh.m_materials.size(); m++)
		{
			const Alamo::Material& mat = mesh.m_materials[m];
			for (size_t i = 0; i < mat.m_vertices.size(); i++)
			{
				Alamo::Vertex& vertex = mat.m_vertices[i];
				for (int b = 0; b < NUM_BONES_PER_VERTEX; b++)
				{
					if (vertex.BoneWeights[b] != 0.0f)
					{
						int index = vertex.BoneIndices[b];
						index = (index >= mat.m_boneMappings.size()) ? 0 : mat.m_boneMappings[index];
						if (index > 0)
						{
							bones.insert(index);
						}
					}
				}
			}
		}

		if (!bones.empty())
		{
			// Create a derived object to create add skin modifier on
			IDerivedObject* dobj = CreateDerivedObject(tri);
			inode->SetObjectRef(dobj);

			// Create and apply the skin modifier
			OSModifier* skin = (OSModifier*)CreateInstance(OSM_CLASS_ID, SKIN_CLASSID);
			dobj->SetAFlag(A_LOCK_TARGET);
			dobj->AddModifier(skin);
			dobj->ClearAFlag(A_LOCK_TARGET); 

			// Assign the bones to the skin
			ISkinImportData* skinImport = (ISkinImportData*)skin->GetInterface(I_SKINIMPORTDATA);
			size_t boneidx = 0;
			for (set<int>::const_iterator i = bones.begin(); i != bones.end(); i++, boneidx++)
			{
				skinImport->AddBoneEx(m_bones[*i].inode, (boneidx == bones.size() -1) );
			}

			// Do this before calling ISkinImportData::AddWeights
			inode->EvalWorldState(0);

			// Assign weights to the vertices
			for (int m = 0; m < mesh.m_materials.size(); m++)
			{
				const Alamo::Material& mat = mesh.m_materials[m];
				const vector<int>& vertmap = vertexMaps[m];
				for (size_t i = 0; i < mat.m_vertices.size(); i++)
				{
					Alamo::Vertex& vertex = mat.m_vertices[i];
					Tab<INode*> bones;
					Tab<float>  weights;
					bones.ZeroCount();
					weights.ZeroCount();
					for (int b = 0; b < NUM_BONES_PER_VERTEX; b++)
					{
						if (vertex.BoneWeights[b] != 0.0f)
						{
							int index = vertex.BoneIndices[b];
							index = (index >= mat.m_boneMappings.size()) ? 0 : mat.m_boneMappings[index];
						    if (index > 0)
							{
								bones.Append(1, &m_bones[index].inode);
								weights.Append(1, &vertex.BoneWeights[b]);
							}
						}
					}
					skinImport->AddWeights(inode, vertmap[i], bones, weights);
				}
			}
		}

		//
		// Post processing
		//
		maxmesh.InvalidateGeomCache();
		maxmesh.InvalidateTopologyCache();
		maxmesh.buildNormals();

		return node;
	}
	
	static void SetBoneLength(Object* bone, float length)
	{
		Animatable* anim = bone;
		IParamBlock2* params = anim->GetParamBlock(0);

		// Scale width and height along
		params->SetValue( params->IndextoID(0), 0.0f, sqrt(length / 2) );	// 0 is the width parameter
		params->SetValue( params->IndextoID(1), 0.0f, sqrt(length / 2) );	// 1 is the height parameter
		params->SetValue( params->IndextoID(3), 0.0f, length );				// 3 is the length parameter
	}

	void CreateSkeleton(const Model* model, ImpInterface* ii)
	{
		m_bones.resize(model->m_bones.size());

		// Create all the bones.
		// Skip the root bone; it's always identity anyway
		for (int i = 0; i < (int)model->m_bones.size(); i++)
		{
			BONEINFO* parent = (model->m_bones[i].m_parent != -1) ? &m_bones[model->m_bones[i].m_parent] : NULL;
			BONEINFO& bone   = m_bones[i];

			// Create the bone
			bone.object = (Object*)CreateInstance(GEOMOBJECT_CLASS_ID, BONE_OBJ_CLASSID);

			// Add the bone to the scene
			bone.impnode = ii->CreateNode();
			bone.impnode->Reference(bone.object);
			ii->AddNodeToScene(bone.impnode);
			bone.inode = bone.impnode->GetINode();

			// Transform the bone
			bone.impnode->SetTransform(0.0f, model->m_bones[i].m_transform);

			if (parent != NULL && parent->inode != NULL)
			{
				// Attach it to its parent bone
				parent->inode->AttachChild(bone.inode);
			}

			bone.impnode->SetName( (TCHAR*)model->m_bones[i].m_name.c_str() );
		}

		// Set bone lengths
		for (int i = 0; i < (int)model->m_bones.size(); i++)
		{
			INode* bone = m_bones[i].inode;
			float length = 1.0f;
			#if 0
			// We don't do this because bones can have been inverted due
			// to parity-correction. I know of no way to restore them based
			// on the input. So we just keep all bones 1 unit long visually.
			if (bone->NumberOfChildren() != 0)
			{
				// See if the bone touches a child, otherwise averages
				int j;
				length = 0.0f;
				for (j = 0; j < bone->NumberOfChildren(); j++)
				{
					INode*  child = bone->GetChildNode(j);
					
					// Get the end point
					Point3 to;
					Matrix3 transform = child->GetNodeTM(0);
					to = transform.GetTrans();

					// Transform to local space
					transform = bone->GetNodeTM(0);
					Point3 from = transform.GetTrans();
					transform.NoTrans();
					transform.Invert();
					to = transform.VectorTransform(to - from);

					if (fabs(to.y) < MAX_DEVIATION && fabs(to.z) < MAX_DEVIATION)
					{
						// This child lies exactly in the path of the bone, connect them
						length = to.Length();
						break;
					}
					length += to.Length();
				}
				
				if (j == bone->NumberOfChildren())
				{
					length /= bone->NumberOfChildren();
				}
			}
			#endif
			SetBoneLength(m_bones[i].object, length);
		}
	}

	int	DoImport(const TCHAR *_name, ImpInterface *ii, Interface *iface, BOOL suppressPrompts)
	{
		MapPath = string(iface->GetDir(APP_MAXROOT_DIR)) + "maps\\";

		Files files;
		TCHAR name[MAX_PATH];
		strcpy(name, _name);

		// Read the model file
		try
		{
			PhysicalFile file(name);
			files.model = new Model(&file);
		}
		catch (exception& e)
		{
			if (!suppressPrompts)
			{
				MessageBox(NULL, e.what(), NULL, MB_OK | MB_ICONHAND);
			}
			return IMPEXP_FAIL;
		}

		// Get the base name (....\XXXXX.ALO)
		TCHAR* str = PathFindFileName(name);
		PathRemoveExtension(str);
		string basename = string(str) + "_";

		// Get the directory name
		PathRemoveFileSpec(name);

		// Read the animation files
		string search = string(name) + "\\" + basename + "*" + ".ALA";
		WIN32_FIND_DATA wfd;
		HANDLE hFind = FindFirstFile(search.c_str(), &wfd);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				Animation* animation = NULL;
				try
				{
					PhysicalFile file(wfd.cFileName);
					animation = new Alamo::Animation(&file, *files.model);

					// Get the name of the animation
					str = PathFindFileName(wfd.cFileName) + basename.length();
					PathRemoveExtension(str);
					string name = str;
					transform(name.begin(), name.end(), name.begin(), toupper);

					files.animations.push_back(make_pair(animation, name));
				}
				catch (BadFileException&)
				{
					// Invalid file, just ignore it
				}
				catch (exception& e)
				{
					// Something else went wrong
					delete animation;
					if (!suppressPrompts)
					{
						MessageBox(NULL, e.what(), NULL, MB_OK | MB_ICONHAND);
					}
					return IMPEXP_FAIL;
				}
			} while (FindNextFile(hFind, &wfd));
			FindClose(hFind);
		}

		//
		// Do the import
		//

		SuspendAnimate();
		SuspendSetKeyMode();
		AnimateOn();

		// Create the skeleton
		CreateSkeleton(files.model, ii);

		// Create the meshes
		for (size_t i = 0; i < files.model->m_meshes.size(); i++)
		{
			Alamo::Mesh& mesh = files.model->m_meshes[i];
			ImpNode* node = CreateMesh(files.model, mesh, ii);
			if (node != NULL)
			{
				ii->AddNodeToScene(node);
			}
		}

		//
		// Add the animations to the scene
		//

		// Remove all current animations
		ReferenceTarget* scene = iface->GetScenePointer();
		for (int i = 0; scene->RemoveAppDataChunk(AlamoUtilityID, UTILITY_CLASS_ID, i); i++);
	
        // Start animations at 1, frame 0 remains the unanimated mesh
		int curFrame = 1;
		int numAnims = 0;
		for (size_t i = 0; i < files.animations.size(); i++)
		{
			const Alamo::Animation* animation = files.animations[i].first;
			if (animation->GetFPS() == FPS)
			{
				// Apply the animations
				for (unsigned long b = 1; b < animation->GetNumBones(); b++)
				{
					INode* bone = m_bones[b].inode;
					const Alamo::BoneAnimation& anim = animation->GetBone(b);

					for (unsigned long frame = 0; frame < animation->GetNumFrames(); frame++)
					{
						Matrix3 transform;
						anim.GetTransform(frame, transform);

						TimeValue time = SecToTicks((curFrame + frame) / FPS);
						
						// I'm not quite sure why we need to do this, the transformation
						// is already supposed to be absolute.
						INode* parent = bone->GetParentNode();
						if (parent != NULL)
						{
							transform *= parent->GetNodeTM(time);
						}

						bone->SetNodeTM(time, transform);
					}
				}

				// Add the animation to the scene
				ANIMATION_INFO* info = (ANIMATION_INFO*)malloc(sizeof(ANIMATION_INFO));
				strcpy(info->Text, files.animations[i].second.c_str());
				info->StartFrame = curFrame;
				info->EndFrame   = (curFrame += animation->GetNumFrames()) - 1;
				scene->AddAppDataChunk(AlamoUtilityID, UTILITY_CLASS_ID, numAnims++, sizeof(ANIMATION_INFO), info);
			}
		}
		AnimateOff();
		ResumeSetKeyMode();
		ResumeAnimate();

		return IMPEXP_SUCCESS;
	}
};

class AlamoImporterClassDesc : public ClassDesc2
{
public:
	void *Create(BOOL loading)
	{
		return new AlamoImporter();
	}

	int 		 IsPublic()		{ return TRUE; }
	const TCHAR* ClassName()	{ return "Alamo2max"; }
	SClass_ID    SuperClassID()	{ return SCENE_IMPORT_CLASS_ID; }
	Class_ID     ClassID()		{ return AlamoImporterID; }
	const TCHAR* Category()		{ return "Alamo Plugins"; }
	const TCHAR* InternalName()	{ return "Max2alamo"; }
	HINSTANCE    HInstance()	{ return GetModuleHandle(NULL); }

	void SetUserDlgProc(ParamBlockDesc2* pbd, ParamMap2UserDlgProc* proc)
	{
		ClassDesc2::SetUserDlgProc(pbd, 0, proc);
	}
};

AlamoImporterClassDesc alamoImportClassDesc;

extern "C"
{
	DllExport const char* LibDescription()
	{
		return "Alamo import module";
	}

	DllExport int LibNumberClasses()
	{
		return 1;
	}

	DllExport ULONG LibVersion()
	{
		return VERSION_3DSMAX;
	}

	DllExport ClassDesc *LibClassDesc(int i)
	{
		switch (i)
		{
			case 0: return &alamoImportClassDesc;
		}
		return NULL;
	}
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		CoInitialize(NULL);
		AllocConsole();
		freopen("conout$", "w", stdout);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		FreeConsole();
	}
	return TRUE;
}
