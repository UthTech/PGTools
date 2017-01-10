#ifndef MODELTEMPLATE_H
#define MODELTEMPLATE_H

#include "ChunkReader.h"
#include <max.h>
#include <vector>
#include <map>

namespace Alamo
{

class  Animation;

enum ParameterType
{
	PT_INT,
	PT_FLOAT,
	PT_FLOAT3,
	PT_FLOAT4,
	PT_TEXTURE,
};

enum BillboardType
{
	BT_DISABLE,
	BT_PARALLEL,
	BT_FACE,
	BT_ZAXIS_VIEW,
	BT_ZAXIS_LIGHT,
	BT_ZAXIS_WIND,
	BT_SUNLIGHT_GLOW,
	BT_SUN,
};

#pragma pack(1)
struct Vertex
{
	Point3 Position;
	Point3 Normal;
	Point2 TexCoord[4];
	Point3 Tangent;
	Point3 Binormal;
	Point3 Color;
	float  Alpha;
	Point4 Unknown;
	DWORD  BoneIndices[4];
	float  BoneWeights[4];
};
#pragma pack()

struct EffectParameter
{
	std::string     m_name;
	ParameterType   m_type;
	long			m_integer;
	FLOAT           m_float;
	Point4			m_float4;
	std::string		m_textureName;
};

struct BoundingBox
{
	Point3 min;
	Point3 max;
};

struct Bone
{
	int		      m_parent;
	bool          m_visible;
	BillboardType m_billboarding;
	std::string   m_name;
	Matrix3		  m_transform;
};

struct Proxy
{
	std::string  m_name;
	int			 m_bone;
	bool		 m_hidden;
	bool		 m_altDecStayHidden;
};

struct Material
{
	std::string 					m_effectName;
	std::vector<EffectParameter>	m_parameters;
	std::string						m_vertexFormat;
	Buffer<unsigned long>			m_boneMappings;
	Buffer<Vertex>					m_vertices;
	Buffer<uint16_t>				m_indices;
};

class Mesh
{
	void ReadMaterialEffect  (Material& material, ChunkReader& reader);
	void ReadMaterialGeometry(Material& material, ChunkReader& reader);

public:
	std::string           m_name;
	int					  m_bone;
	BoundingBox           m_boundingBox;
	bool				  m_hidden;
	bool				  m_collision;
	std::vector<Material> m_materials;

	Mesh(ChunkReader &reader);
};

class Model
{
	void ReadBones    (ChunkReader& reader);
	void ReadBone     (ChunkReader& reader, int bone);
	void ReadHierarchy(ChunkReader& reader, const std::vector<int>& objects);

public:
	Matrix3 GetTransform(int bone) const;

	std::vector<Bone>				m_bones;
	std::vector<Mesh>				m_meshes;
	std::vector<Proxy>				m_proxies;

	Model(IFile* file);
};

class BoneAnimation
{
	friend class Animation;
public:
	void GetTransform(unsigned long frame, Matrix3& transform) const
	{
		transform = m_frames[frame];
	}

private:
	std::vector<Matrix3>	m_frames;
};

class Animation
{
	float						m_fps;
	unsigned long				m_numFrames;
	std::vector<BoneAnimation>	m_bones;

public:
	Animation(IFile* file, const Model& model);

	float         GetFPS()       const { return m_fps; }
	unsigned long GetNumFrames() const { return m_numFrames; }

	const BoneAnimation& GetBone(size_t i) const { return m_bones[i];     }
	size_t GetNumBones() const                   { return m_bones.size(); }
};

}
#endif