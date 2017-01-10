#include "ChunkReader.h"
#include "Model.h"
#include "Log.h"
#include "exceptions.h"
using namespace std;
using namespace Alamo;

static void Verify(bool expr)
{
	if (!expr)
	{
		throw 0;
	}
}

struct BONEANIMATION
{
	string			name;
	unsigned long	bone;
	Point3			translateScale;
	Point3			translateOffset;
	short			idxTranslation;
	short			idxRotation;
	Quat			defaultRotation;
	vector<Point3>	translations;
	vector<Quat>	rotations;
};

static void UnpackQuaternion(Quat& quat, const int16_t data[])
{
	quat.x = (float)data[0] / INT16_MAX;
	quat.y = (float)data[1] / INT16_MAX;
	quat.z = (float)data[2] / INT16_MAX;
	quat.w = (float)data[3] / INT16_MAX;
}

static void ReadPackedQuaternion(ChunkReader& reader, Quat& quat)
{
	int16_t data[4];
	reader.read(data, sizeof data);
	UnpackQuaternion(quat, data);
}

static void UnpackVector(Point3& vec, const uint16_t data[], const Point3& scale, const Point3& offset)
{
	vec.x = (float)data[0] * scale.x + offset.x;
	vec.y = (float)data[1] * scale.y + offset.y;
	vec.z = (float)data[2] * scale.z + offset.z;
}

static void ReadBoneAnimation(ChunkReader& reader, BONEANIMATION& bone, unsigned long numFrames, bool isFOC)
{
	Verify(reader.next() == 0x1002);

	// Read bone animation information
	Verify(reader.next() == 0x1003);
	Verify(reader.nextMini() ==  4); bone.name = reader.readString();
	Verify(reader.nextMini() ==  5); bone.bone = reader.readInteger();

	Alamo::ChunkType type = reader.nextMini();
	if (type == 10)
	{
		// We don't care about this
		type = reader.nextMini();
	}

	Verify(type				 == 6); reader.read(&bone.translateOffset.x, 3 * sizeof(float));
	Verify(reader.nextMini() == 7); reader.read(&bone.translateScale.x,  3 * sizeof(float));
	Verify(reader.nextMini() == 8);
	Verify(reader.nextMini() == 9);
	if (isFOC)
	{
		Verify(reader.nextMini() == 14); bone.idxTranslation = (int16_t)reader.readShort();
		Verify(reader.nextMini() == 15); Verify((int16_t)reader.readShort() == -1);
		Verify(reader.nextMini() == 16); bone.idxRotation    = (int16_t)reader.readShort();
		Verify(reader.nextMini() == 17); ReadPackedQuaternion(reader, bone.defaultRotation);
	}
	Verify(reader.nextMini() == -1);

	if (!isFOC)
	{
		type = reader.next();
		if (type == 0x1004)
		{
			// Read translation data
			Verify(reader.size() == numFrames * 3 * sizeof(uint16_t));
			Buffer<uint16_t> data(numFrames * 3);
			reader.read(&data[0], reader.size());

			bone.translations.resize(numFrames);
			for (unsigned long i = 0; i < numFrames; i++)
			{
				UnpackVector(bone.translations[i], &data[i * 3], bone.translateScale, bone.translateOffset);
			}
			type = reader.next();
		}

		// Read rotations
		Verify(type == 0x1006);
		if (reader.size() == 4 * sizeof(uint16_t))
		{
			ReadPackedQuaternion(reader, bone.defaultRotation);
		}
		else
		{
			Verify(reader.size() == numFrames * 4 * sizeof(uint16_t));
			Buffer<int16_t> data(numFrames * 4);
			reader.read(&data[0], reader.size());
			bone.rotations.resize(numFrames);
			for (unsigned long i = 0; i < numFrames; i++)
			{
				UnpackQuaternion(bone.rotations[i], &data[i*4]);
			}
		}
	}

	type = reader.next();
	if (type == 0x1007)
	{
		// Read visibility track
		type = reader.next();
	}

	if (type == 0x1008)
	{
		// We don't care about the key step track
		type = reader.next();
	}

	// End of bone animation
	Verify(type == -1);
}

Animation::Animation(IFile* file, const Model& model)
{
	ChunkReader reader(file);
	try
	{
		bool isFOC = false;
		unsigned long numTranslationWords = 0;
		unsigned long numRotationWords    = 0;

		Verify(reader.next() == 0x1000);

		vector<BONEANIMATION> bones;
	
		// Read animation information
		Verify(reader.next() == 0x1001);
		Verify(reader.nextMini() == 1); m_numFrames = reader.readInteger();
		Verify(reader.nextMini() == 2); m_fps       = reader.readFloat();
		Verify(reader.nextMini() == 3); bones.resize( reader.readInteger() );
		ChunkType type = reader.nextMini();
		if (type == 11)
		{
			isFOC = true;
			numRotationWords = reader.readInteger();
			Verify(reader.nextMini() == 12); numTranslationWords = reader.readInteger();
			Verify(reader.nextMini() == 13); Verify(reader.readInteger() == 0);
			type = reader.nextMini();
		}
		Verify(type == -1);

		// Read bone animations
		for (size_t i = 0; i < bones.size(); i++)
		{
			ReadBoneAnimation(reader, bones[i], m_numFrames, isFOC);
			if (isFOC)
			{
				Verify((bones[i].idxTranslation == -1) || (bones[i].idxTranslation < (short)numTranslationWords));
				Verify((bones[i].idxRotation    == -1) || (bones[i].idxRotation    < (short)numRotationWords));
			}
		}

		if (isFOC)
		{
			if (numTranslationWords > 0)
			{
				// Read translation data
				Verify(reader.next() == 0x100a);
				Verify(reader.size() == m_numFrames * numTranslationWords * sizeof(uint16_t));
				
				Buffer<uint16_t> data(m_numFrames * numTranslationWords);
				reader.read(&data[0], reader.size());

				for (size_t i = 0; i < bones.size(); i++)
				{
					if (bones[i].idxTranslation != -1)
					{
						bones[i].translations.resize(m_numFrames);
						for (unsigned long j = 0; j < m_numFrames; j++)
						{
							int index = j * numTranslationWords + bones[i].idxTranslation;
							UnpackVector(bones[i].translations[j], &data[index], bones[i].translateScale, bones[i].translateOffset);
						}
					}
				}
			}

			if (numRotationWords > 0)
			{
				// Read rotation data
				Verify(reader.next() == 0x1009);
				Verify(reader.size() == m_numFrames * numRotationWords * sizeof(uint16_t));

				Buffer<int16_t> data(m_numFrames * numRotationWords);
				reader.read(&data[0], reader.size());

				for (size_t i = 0; i < bones.size(); i++)
				{
					if (bones[i].idxRotation != -1)
					{
						bones[i].rotations.resize(m_numFrames);
						for (unsigned long j = 0; j < m_numFrames; j++)
						{
							int index = j * numRotationWords + bones[i].idxRotation;
							UnpackQuaternion(bones[i].rotations[j], &data[index]);
						}
					}
				}
			}
		}

		// End of file
		Verify(reader.next() == -1);

		//
		// Verify data
		//
		for (size_t i = 0; i < bones.size(); i++)
		{
			Verify(bones[i].bone < model.m_bones.size());
			Verify(bones[i].name == model.m_bones[bones[i].bone].m_name);
		}

		//
		// Convert data
		//
		m_bones.resize( model.m_bones.size() );
		for (size_t i = 0; i < bones.size(); i++)
		{
			// Calculate per-frame rotation and translation
			BoneAnimation& bone = m_bones[bones[i].bone];
			bone.m_frames.resize(m_numFrames);
			for (unsigned long frame = 0; frame < m_numFrames; frame++)
			{
				Quat& rotation = (bones[i].rotations.empty())    ? bones[i].defaultRotation : bones[i].rotations[frame];
				Point3& trans  = (bones[i].translations.empty()) ? bones[i].translateOffset : bones[i].translations[frame];

				Matrix3& transform = bone.m_frames[frame];
				rotation.MakeMatrix(transform, true);
				transform.SetTrans(trans);
			}
		}
	}
	catch (int)
	{
		throw BadFileException(file->name());
	}
}
