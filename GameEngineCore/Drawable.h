#ifndef GAME_ENGINE_DRAWABLE_H
#define GAME_ENGINE_DRAWABLE_H

#include "HardwareRenderer.h"
#include "Mesh.h"
#include "EngineLimits.h"

namespace GameEngine
{
	class RendererSharedResource;
	class Material;
	class Skeleton;
	class ModuleInstance;
	class PipelineClass;
	class PipelineContext;
	class SceneResource;
	class Pose;
	class RetargetFile;

	class DrawableMesh : public CoreLib::RefObject
	{
	private:
		RendererSharedResource * renderRes;
	public:
		VertexFormat vertexFormat;
        MeshVertexFormat meshVertexFormat;
		int vertexBufferOffset;
		int indexBufferOffset;
        int blendShapeBufferOffset = 0;
		int vertexCount = 0;
		int indexCount = 0;
        int blendShapeVertexCount = 0;
		Buffer *GetVertexBuffer();
		Buffer *GetIndexBuffer();
        Buffer *GetBlendShapeBuffer();
		DrawableMesh(RendererSharedResource * pRenderRes)
		{
			renderRes = pRenderRes;
		}
        void Free();
        void MoveFrom(DrawableMesh & other)
        {
            Free();
            vertexFormat = other.vertexFormat;
            meshVertexFormat = other.meshVertexFormat;
            renderRes = other.renderRes;
            vertexBufferOffset = other.vertexBufferOffset;
            indexBufferOffset = other.indexBufferOffset;
            vertexCount = other.vertexCount;
            indexCount = other.indexCount;
            other.vertexCount = 0;
            other.indexCount = 0;
        }
		~DrawableMesh();
	};

	enum class DrawableType
	{
		Static, Skeletal
	};

	struct BlendShapeWeight
    {
        int BlendShapeStartVertexIndex;
        float Weight;
    };

    struct BlendShapeWeightInfo
    {
        CoreLib::List<BlendShapeWeight> Weights;
    };

	class Drawable : public CoreLib::RefObject
	{
		friend class RendererImpl;
		friend class SceneResource;
		friend class RendererServiceImpl;
	private:
		DrawableType type = DrawableType::Static;
        PrimitiveType primType = PrimitiveType::Triangles;
		CoreLib::RefPtr<DrawableMesh> mesh = nullptr;
		MeshElementRange elementRange;
		Material * material = nullptr;
		ModuleInstance * transformModule = nullptr;
		Skeleton * skeleton = nullptr;
		CoreLib::Array<PipelineClass*, MaxWorldRenderPasses> pipelineCache;
		SceneResource * scene = nullptr;
	public:
        uint32_t lightmapId = 0xFFFFFFFF;
		CoreLib::Graphics::BBox Bounds;
		bool CastShadow = true;
        bool RenderCustomDepth = false;
		unsigned int ReorderKey = 0;
		Drawable(SceneResource * sceneRes);
		~Drawable();
		PipelineClass * GetPipeline(int passId, PipelineContext & pipelineManager);
		inline ModuleInstance * GetTransformModule()
		{
			return transformModule;
		}
		bool IsTransparent();
		inline DrawableMesh * GetMesh()
		{
			return mesh.Ptr();
		}
		inline Material* GetMaterial()
		{
			return material;
		}
		MeshVertexFormat & GetVertexFormat()
		{
			return mesh->meshVertexFormat;
		}
        PrimitiveType GetPrimitiveType()
        {
            return primType;
        }
		MeshElementRange GetElementRange()
		{
			return elementRange;
		}
		void UpdateMaterialUniform();
        void UpdateLightmapIndex(uint32_t lightmapIndex);
		void UpdateTransformUniform(const VectorMath::Matrix4 & localTransform);
		void UpdateTransformUniform(const VectorMath::Matrix4 & localTransform, const Pose & pose, RetargetFile * retarget = nullptr, 
			BlendShapeWeightInfo *blendShapeInfo = nullptr);
	};

	class DrawableSink
	{
	private:
		CoreLib::List<Drawable*> opaqueDrawables;
		CoreLib::List<Drawable*> transparentDrawables;

	public:
		void AddDrawable(Drawable * drawable)
		{
			if (drawable->IsTransparent())
				transparentDrawables.Add(drawable);
			else
				opaqueDrawables.Add(drawable);
			drawable->UpdateMaterialUniform();
		}
		void Clear()
		{
			opaqueDrawables.Clear();
			transparentDrawables.Clear();
		}
		CoreLib::ArrayView<Drawable*> GetDrawables(bool transparent)
		{
			return transparent ? transparentDrawables.GetArrayView() : opaqueDrawables.GetArrayView();
		}
	};
}

#endif