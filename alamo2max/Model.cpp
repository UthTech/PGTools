#include <algorithm>
#include <sstream>
#include "ChunkReader.h"
#include "Model.h"
#include "Log.h"
#include "exceptions.h"
using namespace std;

namespace Alamo
{

static void Verify(bool expr)
{
	if (!expr)
	{
		throw 0;
	}
}

//
// Mesh class
//
void Mesh::ReadMaterialEffect(Material& material, ChunkReader& reader)
{
	// Read effect name
	Verify(reader.next() == 0x10101);
	material.m_effectName = reader.readString();

	// Read parameters
	ChunkType type;
	while ((type = reader.next()) != -1)
	{
		material.m_parameters.push_back( EffectParameter() );
		EffectParameter& param = material.m_parameters.back();
		
		Verify(reader.nextMini() == 1);
		param.m_name = reader.readString();

		Verify(reader.nextMini() == 2);
		switch (type)
		{
		case 0x10102:
			// A single integer
			param.m_type    = PT_INT;
			param.m_integer = reader.readInteger();
			break;

		case 0x10103:
			// A single float
			param.m_type  = PT_FLOAT;
			param.m_float = reader.readFloat();
			break;

		case 0x10104:
			// Vector of 3 floats
			param.m_type = PT_FLOAT3;
			Verify(reader.size() == 3 * sizeof(FLOAT));
			reader.read(&param.m_float4.x, 3 * sizeof(FLOAT));
			break;

		case 0x10105:
			// Texture reference
			param.m_type = PT_TEXTURE;
			param.m_textureName = reader.readString();
			break;

		case 0x10106:
			// Vector of 4 floats
			param.m_type = PT_FLOAT4;
			Verify(reader.size() == 4 * sizeof(FLOAT));
			reader.read(&param.m_float4.x, 4 * sizeof(FLOAT));
			break;

		default:
			// Invalid parameter
			throw 0;
			break;
		}

		Verify(reader.nextMini() == -1);
	}
}

#pragma pack(1)
struct GEOMETRYINFO
{
	uint32_t nVertices;
	uint32_t nPrimitives;
};
#pragma pack(1)

#pragma pack(1)
struct Vertex_Rev1
{
	Point3 Position;
	Point3 Normal;
	Point2 TexCoord[4];
	Point3 Tangent;
	Point3 Binormal;
	Point3 Color;
	float  Alpha;
	Point4 Unknown;
	float  BoneWeights[4];
};
#pragma pack()

void Mesh::ReadMaterialGeometry(Material& material, ChunkReader& reader)
{
	// Read info
	Verify(reader.next() == 0x10001);

	GEOMETRYINFO leInfo;
	reader.read(&leInfo, sizeof(GEOMETRYINFO));
	material.m_vertices.resize(letohl(leInfo.nVertices));
	material.m_indices .resize(letohl(leInfo.nPrimitives) * 3);

	// Read vertex format
	Verify(reader.next() == 0x10002);
	material.m_vertexFormat = reader.readString();

	// Vertex buffer
	ChunkType type = reader.next();
	Verify(type == 0x10007 || type == 0x10005);
	if (type == 0x10005)
	{
		// Vertex format, revision 1
		Verify(reader.size() == sizeof(Vertex_Rev1) * material.m_vertices.size());
		//reader.read(material.m_vertices, reader.size());
	}
	else if (type == 0x10007)
	{
		// Vertex format, revision 2
		Verify(reader.size() == sizeof(Vertex) * material.m_vertices.size());
		reader.read(material.m_vertices, reader.size());
	}

	// Index buffer
	Verify(reader.next() == 0x10004);
	Verify(reader.size() == sizeof(uint16_t) * material.m_indices.size());
	reader.read(material.m_indices, reader.size());

	type = reader.next();
	if (type == 0x10006)
	{
		static const int MAX_BONE_MAPPINGS = 24;

		// Read bone mapping
		unsigned long nBoneMappings  = reader.size() / sizeof(uint32_t);
		Verify((reader.size() % sizeof(uint32_t) == 0) && nBoneMappings <= MAX_BONE_MAPPINGS);
		
		uint32_t boneMappings[MAX_BONE_MAPPINGS];
		reader.read(boneMappings, reader.size());

		material.m_boneMappings.resize(nBoneMappings);
		for (unsigned int i = 0; i < nBoneMappings; i++)
		{
			material.m_boneMappings[i] = letohl(boneMappings[i]);
		}

		type = reader.next();
	}

	if (m_collision)
	{
		// Collision mesh
		Verify(type == 0x1200);
		reader.skip();
		type = reader.next();
	}

	Verify(type == -1);
}

Mesh::Mesh(ChunkReader& reader)
{
#pragma pack()
	struct MESHINFO
	{
		uint32_t    nMaterials;
		BoundingBox boundingBox;
		uint32_t	unknown;
		uint32_t	isHidden;
		uint32_t	isCollisionEnabled;
	};
#pragma pack()

	// Read mesh name
	Verify(reader.next() == 0x401);
	m_name = reader.readString();

	// Read mesh information
	MESHINFO leMeshInfo;
	Verify(reader.next() == 0x402);
	reader.read(&leMeshInfo, sizeof(MESHINFO));

	m_materials.resize( letohl(leMeshInfo.nMaterials) );
	m_boundingBox = leMeshInfo.boundingBox;
	m_hidden      = letohl(leMeshInfo.isHidden) != 0;
	m_collision   = letohl(leMeshInfo.isCollisionEnabled) != 0;

	// Read materials
	for (size_t i = 0; i < m_materials.size(); i++)
	{
		Verify(reader.next() == 0x10100);
		ReadMaterialEffect(m_materials[i], reader);
		
		Verify(reader.next() == 0x10000);
		ReadMaterialGeometry(m_materials[i], reader);
	}

	// Should be the end of the 0x400h chunk
	Verify(reader.next() == -1);
}

//
// Model class
//
Matrix3 Model::GetTransform(int bone) const
{
	return (m_bones[bone].m_parent != -1)
		? m_bones[bone].m_transform * GetTransform(m_bones[bone].m_parent)
		: m_bones[bone].m_transform;
}

void Model::ReadBone(ChunkReader& reader, int bone)
{
#pragma pack(1)
	struct BONEINFO_OLD
	{
		uint32_t parent;
		uint32_t visible;
		Point4   matrix[3];
	};

	struct BONEINFO
	{
		uint32_t parent;
		uint32_t visible;
		uint32_t billboardMode;
		Point4   matrix[3];
	};
#pragma pack()

	Verify(reader.next() == 0x202);

	// Read bone name
	Verify(reader.next() == 0x203);
	m_bones[bone].m_name = reader.readString();

	// Read bone data
	ChunkType type = reader.next();
	Verify( (type == 0x206 && reader.size() == sizeof(BONEINFO    )) ||
			(type == 0x205 && reader.size() == sizeof(BONEINFO_OLD)) );

	BONEINFO leBone;
	if (type == 0x206)
	{
		reader.read(&leBone, sizeof(BONEINFO));
	}
	else if (type == 0x205)
	{
		// Old bone type
		BONEINFO_OLD leBoneOld;
		reader.read(&leBoneOld, sizeof(BONEINFO_OLD));
		leBone.parent        = leBoneOld.parent;
		leBone.visible       = leBoneOld.visible;
		leBone.billboardMode = htolel(BT_DISABLE);
		memcpy(leBone.matrix, leBoneOld.matrix, 3 * sizeof(Point4));
	}

	// The parent should point to an earlier bone
	long parent = letohl(leBone.parent);
	Verify(parent < bone);

	m_bones[bone].m_parent       = parent;
	m_bones[bone].m_visible      = letohl(leBone.visible) != 0;
	m_bones[bone].m_billboarding = (BillboardType)letohl(leBone.billboardMode);

	// Copy transposed matrix
	Matrix3& mat = m_bones[bone].m_transform;
	mat.SetColumn(0, leBone.matrix[0]);
	mat.SetColumn(1, leBone.matrix[1]);
	mat.SetColumn(2, leBone.matrix[2]);

	if (bone == 3)
	{
		Matrix3 tmp = mat * m_bones[parent].m_transform;
		tmp = m_bones[parent].m_transform * mat;
	}

	// Should be the end of the 0x202h chunk
	Verify(reader.next() == -1);
}

void Model::ReadBones(ChunkReader& reader)
{
	Verify(reader.next() == 0x200);
	
	Verify(reader.next() == 0x201 && reader.size() == 128);
	uint32_t leNumBones;
	reader.read(&leNumBones, sizeof leNumBones);

	m_bones.resize( letohl(leNumBones) );
	for (unsigned int i = 0; i < m_bones.size(); i++)
	{
		ReadBone(reader, i);
	}

	// Should be the end of the 0x200h chunk
	Verify(reader.next() == -1);
}

void Model::ReadHierarchy(ChunkReader& reader, const std::vector<int>& objects)
{
	// Read counts
	Verify(reader.next() == 0x601);
	Verify(reader.nextMini() == 1); unsigned long nMeshes  = reader.readInteger();
	Verify(reader.nextMini() == 4); unsigned long nProxies = reader.readInteger();
	Verify(reader.nextMini() == -1);

	// Read mesh and light connections
	for (unsigned long i = 0; i < nMeshes; i++)
	{
		Verify(reader.next() == 0x602);
		Verify(reader.nextMini() == 2); unsigned long obj  = reader.readInteger();
		Verify(reader.nextMini() == 3); unsigned long bone = reader.readInteger();
		Verify(reader.nextMini() == -1);
		
		// Connect object
		Verify(obj < objects.size() && bone < m_bones.size());
		if (objects[obj] != -1)
		{
			m_meshes[objects[obj]].m_bone = bone;
		}
	}

	// Read proxies
	m_proxies.resize(nProxies);
	for (unsigned long i = 0; i < nProxies; i++)
	{
		m_proxies[i].m_hidden			= false;
		m_proxies[i].m_altDecStayHidden = false;

		Verify(reader.next() == 0x603);
		Verify(reader.nextMini() == 5); m_proxies[i].m_name = reader.readString();
		Verify(reader.nextMini() == 6); unsigned long bone  = reader.readInteger();
		
		ChunkType type = reader.nextMini();
		if (type == 7)
		{
			// Read hidden property
			m_proxies[i].m_hidden = (reader.readInteger() != 0);
			type = reader.nextMini();
		}

		if (type == 8)
		{
			// Read Alt Decrease Stay Hidden
			m_proxies[i].m_altDecStayHidden = (reader.readInteger() != 0);
			type = reader.nextMini();
		}
		Verify(type == -1);
		
		// Connect proxy
		Verify(bone < m_bones.size());
		m_proxies[i].m_bone = bone;
	}

	// Should be the end of the 0x600h chunk
	Verify(reader.next() == -1);
}

Model::Model(IFile* file)
{
	ChunkReader reader(file);
	try
	{
		vector<int> objects;

		// Read bones
		ReadBones(reader);

		// Read meshes and lights
		ChunkType type;
		while ((type = reader.next()) != -1)
		{
			if (type == 0x0400)
			{
				objects.push_back((int)m_meshes.size());
				m_meshes.push_back(Mesh(reader));
				continue;
			}

			if (type == 0x1300)
			{
				// We don't care about lights
				reader.skip();
				objects.push_back(-1);
				continue;
			}
			break;
		}

		// Read hierarchy
		Verify(type == 0x0600);
		ReadHierarchy(reader, objects);
	}
	catch (int)
	{
		throw BadFileException(file->name());
	}
}

}