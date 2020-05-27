#include "Mesh.h"
#include "CoreLib/LibIO.h"
#include "Skeleton.h"
#include "Engine.h"
#include "ShaderCompiler.h"

using namespace CoreLib::Basic;
using namespace CoreLib::IO;
using namespace VectorMath;

namespace GameEngine
{
	int Mesh::uid = 0;
	void Mesh::LoadFromFile(const CoreLib::Basic::String & pfileName)
	{
		RefPtr<FileStream> stream = new FileStream(pfileName);
		LoadFromStream(stream.Ptr());
		stream->Close();
		this->fileName = pfileName;
	}

	bool CheckMeshIdentifier(char * str)
	{
		MeshHeader header;
		for (int i = 0; i < sizeof(header.MeshFileIdentifier); i++)
			if (header.MeshFileIdentifier[i] != str[i])
				return false;
		return true;
	}

    static PrimitiveType ReadPrimitiveType(int ptype)
    {
        switch (ptype)
        {
        case 0:
            return PrimitiveType::Triangles;
        case 1:
            return PrimitiveType::Lines;
        case 2:
            return PrimitiveType::Points;
        default:
            return PrimitiveType::Triangles;
        }
    }

    static int WritePrimitiveType(PrimitiveType ptype)
    {
        switch (ptype)
        {
        case PrimitiveType::Triangles:
            return 0;
        case PrimitiveType::Lines:
            return 1;
        case PrimitiveType::Points:
            return 2;
        default:
            throw ArgumentException("unsupported mesh primitive type");
        }
    }

	void Mesh::LoadFromStream(Stream * stream)
	{
		auto reader = BinaryReader(stream);
		MeshHeader header;
		reader.Read(header);
        primitiveType = ReadPrimitiveType(header.PrimitiveType);
        surfaceArea = header.SurfaceArea;
        minLightmapResolution = header.MinLightmapResolution;
        if (!CheckMeshIdentifier(header.MeshFileIdentifier))
		{
			stream->Seek(SeekOrigin::Start, 0);
			header = MeshHeader();
		}
		int typeId = reader.ReadInt32();
		vertexFormat = MeshVertexFormat(typeId);
		vertCount = reader.ReadInt32();
	    
		int indexCount = reader.ReadInt32();
		reader.Read(&Bounds, 1);
		AllocVertexBuffer(vertCount);
		Indices.SetSize(indexCount);
		reader.Read((char*)GetVertexBuffer(), vertCount * vertexFormat.GetVertexSize());
		reader.Read(Indices.Buffer(), indexCount);
		ElementRanges.SetSize(header.ElementCount);
		reader.Read(ElementRanges.Buffer(), ElementRanges.Count());
		if (ElementRanges.Count() == 0)
		{
			MeshElementRange range;
			range.StartIndex = 0;
			range.Count = indexCount;
			ElementRanges.Add(range);
		}
        if (header.HasBlendShapes)
        {
            int blendShapeChannelCount = reader.ReadInt32();
            ElementBlendShapeChannels.SetSize(blendShapeChannelCount);
            CORELIB_ASSERT(blendShapeChannelCount == ElementRanges.Count());
            for (auto &blendShapes : ElementBlendShapeChannels)
            {
                blendShapes.SetSize(reader.ReadInt32());
                for (auto &blendShape : blendShapes)
                {
                    reader.Read(blendShape.Name);
                    reader.Read(blendShape.ChannelId);
                    reader.Read(blendShape.BlendShapes);
                    reader.Read(blendShape.Reserved, sizeof(blendShape.Reserved));
                }
            }
            reader.Read(BlendShapeVertices);
        }
		reader.ReleaseStream();
		fileName = String("mesh_") + String(uid++);
	}

	Mesh::Mesh()
	{
		fileName = String("mesh_") + String(uid++);
	}

	CoreLib::String Mesh::GetUID()
	{
		return fileName;
	}

    CoreLib::String Mesh::GetFileName()
    {
        return fileName;
    }

	void Mesh::SaveToStream(Stream * stream)
	{
		auto writer = BinaryWriter(stream);
		MeshHeader header;
		header.ElementCount = ElementRanges.Count();
        header.PrimitiveType = WritePrimitiveType(primitiveType);
        header.MinLightmapResolution = minLightmapResolution;
        header.SurfaceArea = surfaceArea;
        header.HasBlendShapes = BlendShapeVertices.Count() != 0;
		writer.Write(header);
		writer.Write(GetVertexTypeId());
		writer.Write(vertCount);
		writer.Write(Indices.Count());
		writer.Write(&Bounds, 1);
		writer.Write((char*)GetVertexBuffer(), vertCount * GetVertexSize());
		writer.Write(Indices.Buffer(), Indices.Count());
		writer.Write(ElementRanges.Buffer(), ElementRanges.Count());
        if (header.HasBlendShapes)
        {
            writer.Write(ElementBlendShapeChannels.Count());
            for (auto &blendShapeChannels : ElementBlendShapeChannels)
            {
                writer.Write(blendShapeChannels.Count());
                for (auto &blendShapeChannel : blendShapeChannels)
                {
                    writer.Write(blendShapeChannel.Name);
                    writer.Write(blendShapeChannel.ChannelId);
                    writer.Write(blendShapeChannel.BlendShapes);
                    writer.Write(blendShapeChannel.Reserved, sizeof(blendShapeChannel.Reserved));
                }
            }
            writer.Write(BlendShapeVertices);
        }
		writer.ReleaseStream();
	}

	void Mesh::SaveToFile(const CoreLib::String & pfileName)
	{
		RefPtr<FileStream> stream = new FileStream(pfileName, FileMode::Create);
		SaveToStream(stream.Ptr());
		stream->Close();
		fileName = pfileName;
	}

	MeshVertexFormat::MeshVertexFormat(int typeId)
	{
		key.typeId = typeId;
		vertSize = CalcVertexSize();
	}

	ShaderTypeSymbol * MeshVertexFormat::GetTypeSymbol()
	{
		StringBuilder sbName;
		sbName << "StandardVertexFormat<";
        sbName << "VertexUVSet" << key.fields.numUVs;
        sbName << ",";
        sbName << (key.fields.hasTangent ? "StandardTangentFrame" : "NoTangentFrame");
        sbName << ",";
        sbName << "VertexColorSet" << key.fields.numColors;
        sbName << ",";
        if (key.fields.hasSkinning)
            sbName << "StandardBoneWeightSet";
        else
            sbName << "NoBoneWeightSet";
        sbName << ">";
        auto name = sbName.ToString();
        return Engine::GetShaderCompiler()->LoadSystemTypeSymbol(name);
	}
	
	struct SkeletonMeshVertex
	{
		Vec3 pos;
		Quaternion tangentFrame;
		int boneId;
	};
    void Mesh::UpdateBounds()
    {
        Bounds.Init();
        for (int i = 0; i < vertCount; i++)
        {
            Bounds.Union(GetVertexPosition(i));
        }
    }
	void Mesh::FromSkeleton(Skeleton * skeleton, float width)
	{
		Bounds.Init();
		this->Indices.Clear();
		this->vertexData.Clear();
		SetVertexFormat(MeshVertexFormat(0, 0, true, true));
		List<SkeletonMeshVertex> vertices;
		List<Matrix4> forwardTransforms;
		List<Vec3> positions;
		positions.SetSize(skeleton->Bones.Count());
		forwardTransforms.SetSize(skeleton->Bones.Count());
		for (int i = 0; i < skeleton->Bones.Count(); i++)
		{
			forwardTransforms[i] = skeleton->Bones[i].BindPose.ToMatrix();
			if (skeleton->Bones[i].ParentId != -1)
				Matrix4::Multiply(forwardTransforms[i], forwardTransforms[skeleton->Bones[i].ParentId], forwardTransforms[i]);
			positions[i] = Vec3::Create(forwardTransforms[i].values[12], forwardTransforms[i].values[13], forwardTransforms[i].values[14]);
		}
		for (int i = 0; i < skeleton->Bones.Count(); i++)
		{
			float boneWidth = width;
			int parent = skeleton->Bones[i].ParentId;
			Vec3 bonePos = positions[i];
			Vec3 parentPos = parent == -1 ? bonePos : positions[parent];
			if (parent == -1)
			{
				bonePos.y -= width;
				parentPos.y += width;
			}
			else
			{
				float length = (bonePos - parentPos).Length();
				if (length < width * 2.0f)
					boneWidth = length * 0.5f;
			}
			Bounds.Union(bonePos);
			Bounds.Union(parentPos);

			
			auto addBoneStruct = [&](Vec3 pos, Vec3 pos1, int bid)
			{
				Vec3 dir = (pos1 - pos).Normalize();
				Vec3 xAxis, yAxis;
				GetOrthoVec(xAxis, dir);
				Vec3::Cross(yAxis, dir, xAxis);
				int vCoords[] = { 0, 1, 3, 2 };
				for (int j = 0; j < 4; j++)
				{
					int vCoord = vCoords[j];
					int vCoord1 = vCoords[(j + 1) & 3];
					Vec3 v0 = pos + dir * boneWidth + xAxis * (boneWidth * ((float)(vCoord & 1) - 0.5f))
						+ yAxis * (boneWidth * ((float)((vCoord >> 1) & 1) - 0.5f));
					Vec3 v1 = pos + dir * boneWidth + xAxis * (boneWidth * ((float)(vCoord1 & 1) - 0.5f))
						+ yAxis * (boneWidth * ((float)((vCoord1 >> 1) & 1) - 0.5f));
					Bounds.Union(v0);

					// triangle1: v1->v0->parent
					{
						Vec3 normal1 = Vec3::Cross(v0 - v1, pos - v1).Normalize();
						Vec3 tangent1 = (v1 - v0).Normalize();
						Vec3 binormal1 = Vec3::Cross(tangent1, normal1).Normalize();
						Quaternion q = Quaternion::FromCoordinates(tangent1, normal1, binormal1);
						SkeletonMeshVertex vert;
						vert.pos = v1;
						vert.tangentFrame = q;
						vert.boneId = bid;
						Indices.Add(vertices.Count());
						vertices.Add(vert);
						vert.pos = v0;
						Indices.Add(vertices.Count());
						vertices.Add(vert);
						vert.pos = pos;
						Indices.Add(vertices.Count());
						vertices.Add(vert);
					}
					// triangle2: v0->v1->bone
					{
						Vec3 normal1 = Vec3::Cross(v1 - v0, pos1 - v0).Normalize();
						Vec3 tangent1 = (v1 - v0).Normalize();
						Vec3 binormal1 = Vec3::Cross(tangent1, normal1).Normalize();
						Quaternion q = Quaternion::FromCoordinates(tangent1, normal1, binormal1);
						SkeletonMeshVertex vert;
						vert.pos = v0;
						vert.tangentFrame = q;
						vert.boneId = bid;
						Indices.Add(vertices.Count());
						vertices.Add(vert);
						vert.pos = v1;
						Indices.Add(vertices.Count());
						vertices.Add(vert);
						vert.pos = pos1;
						Indices.Add(vertices.Count());
						vertices.Add(vert);
					}
				}
			};
			addBoneStruct(parentPos, bonePos, (parent == -1 ? i : parent));
			if(i != 0)
				addBoneStruct(bonePos-Vec3::Create(boneWidth, 0.0f, 0.0f), bonePos + Vec3::Create(boneWidth, 0.0f, 0.0f), i);
			bonePos = positions[i];

		}

		vertexData.SetSize(vertices.Count() * vertexFormat.GetVertexSize());
		for (int i = 0; i<vertices.Count(); i++)
		{
			SetVertexPosition(i, vertices[i].pos);
			SetVertexTangentFrame(i, vertices[i].tangentFrame);
			SetVertexSkinningBinding(i, MakeArrayView(vertices[i].boneId), MakeArrayView(1.0f));
		}
		vertCount = vertices.Count();
		MeshElementRange range;
		range.StartIndex = 0;
		range.Count = Indices.Count();
		this->ElementRanges.Clear();
		this->ElementRanges.Add(range);
	}

	struct ByteStreamView
	{
		unsigned char * bytes;
		int length;
		ByteStreamView() = default;
		ByteStreamView(unsigned char * stream, int offset, int len)
		{
			bytes = stream + offset;
			length = len;
		}
		int GetHashCode()
		{
			int hash = 0;
			for (int i = 0; i < length; i++)
			{
				int c = bytes[i];
				hash = c + (hash << 6) + (hash << 16) - hash;
			}
			return hash;
		}
		bool operator == (ByteStreamView v)
		{
			if (length != v.length)
				return false;
			for (int i = 0; i < length; i++)
				if (bytes[i] != v.bytes[i])
					return false;
			return true;
		}
	};
	Mesh Mesh::DeduplicateVertices()
	{
		Mesh result;
		result.ElementRanges = ElementRanges;
		result.Bounds = Bounds;
		result.SetVertexFormat(vertexFormat);
		MemoryStream ms;
		BinaryWriter bw(&ms);
		Dictionary<ByteStreamView, int> vertSet;
		List<int> vertIds;
		vertIds.SetSize(vertCount);
		int vertId = 0;
		for (int i = 0; i < vertCount; i++)
		{
			ByteStreamView vert = ByteStreamView(vertexData.Buffer(), GetVertexSize() * i, GetVertexSize());
			int id = -1;
			if (!vertSet.TryGetValue(vert, id))
			{
				vertSet[vert] = vertId;
				bw.Write(vert.bytes, vert.length);
				id = vertId;
				vertId++;
			}
			vertIds[i] = id;
		}
		result.vertCount = vertId;
		result.vertexData.AddRange((unsigned char*)ms.GetBuffer(), vertId * GetVertexSize());
		result.Indices.SetSize(Indices.Count());
		for (int i = 0; i < Indices.Count(); i++)
		{
			result.Indices[i] = vertIds[Indices[i]];
		}
		bw.ReleaseStream();
		return result;
	}
	Mesh Mesh::CreateBox(VectorMath::Vec3 vmin, VectorMath::Vec3 vmax)
	{
		Mesh rs;
		rs.SetVertexFormat(MeshVertexFormat(0, 1, true, false));
		rs.AllocVertexBuffer(24);
		
		Quaternion tangentFrame;
		// top
		tangentFrame = Quaternion::FromCoordinates(Vec3::Create(1.0f, 0.0f, 0.0f), Vec3::Create(0.0f, 1.0f, 0.0f), Vec3::Create(0.0f, 0.0f, 1.0f));
		rs.SetVertexPosition(0, Vec3::Create(vmax.x, vmax.y, vmin.z)); rs.SetVertexUV(0, 0, Vec2::Create(0.0f, 0.0f)); rs.SetVertexTangentFrame(0, tangentFrame);
		rs.SetVertexPosition(1, Vec3::Create(vmin.x, vmax.y, vmin.z)); rs.SetVertexUV(1, 0, Vec2::Create(1.0f, 0.0f)); rs.SetVertexTangentFrame(1, tangentFrame);
		rs.SetVertexPosition(2, Vec3::Create(vmin.x, vmax.y, vmax.z)); rs.SetVertexUV(2, 0, Vec2::Create(1.0f, 1.0f)); rs.SetVertexTangentFrame(2, tangentFrame);
		rs.SetVertexPosition(3, Vec3::Create(vmax.x, vmax.y, vmax.z)); rs.SetVertexUV(3, 0, Vec2::Create(0.0f, 1.0f)); rs.SetVertexTangentFrame(3, tangentFrame);
		rs.Indices.Add(0);	rs.Indices.Add(1);	rs.Indices.Add(2);
		rs.Indices.Add(0);	rs.Indices.Add(2);	rs.Indices.Add(3);

		// bottom
		tangentFrame = Quaternion::FromCoordinates(Vec3::Create(-1.0f, 0.0f, 0.0f), Vec3::Create(0.0f, -1.0f, 0.0f), Vec3::Create(0.0f, 0.0f, 1.0f));
		rs.SetVertexPosition(4, Vec3::Create(vmin.x, vmin.y, vmax.z)); rs.SetVertexUV(4, 0, Vec2::Create(0.0f, 0.0f)); rs.SetVertexTangentFrame(4, tangentFrame);
		rs.SetVertexPosition(5, Vec3::Create(vmin.x, vmin.y, vmin.z)); rs.SetVertexUV(5, 0, Vec2::Create(0.0f, 1.0f)); rs.SetVertexTangentFrame(5, tangentFrame);
		rs.SetVertexPosition(6, Vec3::Create(vmax.x, vmin.y, vmin.z)); rs.SetVertexUV(6, 0, Vec2::Create(1.0f, 1.0f)); rs.SetVertexTangentFrame(6, tangentFrame);
		rs.SetVertexPosition(7, Vec3::Create(vmax.x, vmin.y, vmax.z)); rs.SetVertexUV(7, 0, Vec2::Create(1.0f, 0.0f)); rs.SetVertexTangentFrame(7, tangentFrame);
		rs.Indices.Add(4);	rs.Indices.Add(5);	rs.Indices.Add(6);
		rs.Indices.Add(4);	rs.Indices.Add(6);	rs.Indices.Add(7);

		// front
		tangentFrame = Quaternion::FromCoordinates(Vec3::Create(1.0f, 0.0f, 0.0f), Vec3::Create(0.0f, 0.0f, 1.0f), Vec3::Create(0.0f, -1.0f, 0.0f));
		rs.SetVertexPosition(8,  Vec3::Create(vmin.x, vmin.y, vmax.z)); rs.SetVertexUV(8,  0, Vec2::Create(0.0f, 0.0f)); rs.SetVertexTangentFrame(8,  tangentFrame);
		rs.SetVertexPosition(9,  Vec3::Create(vmax.x, vmin.y, vmax.z)); rs.SetVertexUV(9,  0, Vec2::Create(1.0f, 0.0f)); rs.SetVertexTangentFrame(9,  tangentFrame);
		rs.SetVertexPosition(10, Vec3::Create(vmax.x, vmax.y, vmax.z)); rs.SetVertexUV(10, 0, Vec2::Create(1.0f, 1.0f)); rs.SetVertexTangentFrame(10, tangentFrame);
		rs.SetVertexPosition(11, Vec3::Create(vmin.x, vmax.y, vmax.z)); rs.SetVertexUV(11, 0, Vec2::Create(0.0f, 1.0f)); rs.SetVertexTangentFrame(11, tangentFrame);
		rs.Indices.Add(8);	rs.Indices.Add(9);	rs.Indices.Add(10);
		rs.Indices.Add(8);	rs.Indices.Add(10);	rs.Indices.Add(11);

		// back
		tangentFrame = Quaternion::FromCoordinates(Vec3::Create(-1.0f, 0.0f, 0.0f), Vec3::Create(0.0f, 0.0f, -1.0f), Vec3::Create(0.0f, -1.0f, 0.0f));
		rs.SetVertexPosition(12, Vec3::Create(vmin.x, vmin.y, vmin.z)); rs.SetVertexUV(12, 0, Vec2::Create(0.0f, 0.0f)); rs.SetVertexTangentFrame(12, tangentFrame);
		rs.SetVertexPosition(13, Vec3::Create(vmin.x, vmax.y, vmin.z)); rs.SetVertexUV(13, 0, Vec2::Create(0.0f, 1.0f)); rs.SetVertexTangentFrame(13, tangentFrame);
		rs.SetVertexPosition(14, Vec3::Create(vmax.x, vmax.y, vmin.z)); rs.SetVertexUV(14, 0, Vec2::Create(1.0f, 1.0f)); rs.SetVertexTangentFrame(14, tangentFrame);
		rs.SetVertexPosition(15, Vec3::Create(vmax.x, vmin.y, vmin.z)); rs.SetVertexUV(15, 0, Vec2::Create(1.0f, 0.0f)); rs.SetVertexTangentFrame(15, tangentFrame);
		rs.Indices.Add(12);	rs.Indices.Add(13);	rs.Indices.Add(14);
		rs.Indices.Add(12);	rs.Indices.Add(14);	rs.Indices.Add(15);

		// left
		tangentFrame = Quaternion::FromCoordinates(Vec3::Create(0.0f, 1.0f, 0.0f), Vec3::Create(-1.0f, 0.0f, 0.0f), Vec3::Create(0.0f, 0.0f, 1.0f));
		rs.SetVertexPosition(16, Vec3::Create(vmin.x, vmin.y, vmin.z)); rs.SetVertexUV(16, 0, Vec2::Create(0.0f, 0.0f)); rs.SetVertexTangentFrame(16, tangentFrame);
		rs.SetVertexPosition(17, Vec3::Create(vmin.x, vmin.y, vmax.z)); rs.SetVertexUV(17, 0, Vec2::Create(0.0f, 1.0f)); rs.SetVertexTangentFrame(17, tangentFrame);
		rs.SetVertexPosition(18, Vec3::Create(vmin.x, vmax.y, vmax.z)); rs.SetVertexUV(18, 0, Vec2::Create(1.0f, 1.0f)); rs.SetVertexTangentFrame(18, tangentFrame);
		rs.SetVertexPosition(19, Vec3::Create(vmin.x, vmax.y, vmin.z)); rs.SetVertexUV(19, 0, Vec2::Create(1.0f, 0.0f)); rs.SetVertexTangentFrame(19, tangentFrame);
		rs.Indices.Add(16);	rs.Indices.Add(17);	rs.Indices.Add(18);
		rs.Indices.Add(16);	rs.Indices.Add(18);	rs.Indices.Add(19);

		// right
		tangentFrame = Quaternion::FromCoordinates(Vec3::Create(0.0f, 1.0f, 0.0f), Vec3::Create(1.0f, 0.0f, 0.0f), Vec3::Create(0.0f, 0.0f, -1.0f));
		rs.SetVertexPosition(20, Vec3::Create(vmax.x, vmin.y, vmax.z)); rs.SetVertexUV(20, 0, Vec2::Create(0.0f, 0.0f)); rs.SetVertexTangentFrame(20, tangentFrame);
		rs.SetVertexPosition(21, Vec3::Create(vmax.x, vmin.y, vmin.z)); rs.SetVertexUV(21, 0, Vec2::Create(0.0f, 1.0f)); rs.SetVertexTangentFrame(21, tangentFrame);
		rs.SetVertexPosition(22, Vec3::Create(vmax.x, vmax.y, vmin.z)); rs.SetVertexUV(22, 0, Vec2::Create(1.0f, 1.0f)); rs.SetVertexTangentFrame(22, tangentFrame);
		rs.SetVertexPosition(23, Vec3::Create(vmax.x, vmax.y, vmax.z)); rs.SetVertexUV(23, 0, Vec2::Create(1.0f, 0.0f)); rs.SetVertexTangentFrame(23, tangentFrame);
		rs.Indices.Add(20);	rs.Indices.Add(21);	rs.Indices.Add(22);
		rs.Indices.Add(20);	rs.Indices.Add(22);	rs.Indices.Add(23);
		MeshElementRange range;
		range.StartIndex = 0;
		range.Count = rs.Indices.Count();
		rs.ElementRanges.Add(range);
		return rs;
	}
}

