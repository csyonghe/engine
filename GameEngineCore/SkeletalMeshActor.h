#ifndef GAME_ENGINE_SKELETAL_ANIMATED_ACTOR_H
#define GAME_ENGINE_SKELETAL_ANIMATED_ACTOR_H

#include "Actor.h"
#include "RendererService.h"
#include "Model.h"

namespace GameEngine
{
	class RetargetFile;

	class SkeletalMeshActor : public Actor
	{
	private:
		Pose nextPose;
        CoreLib::List<BlendShapeWeightInfo> blendShapeWeights;
		CoreLib::RefPtr<ModelPhysicsInstance> physInstance, errorPhysInstance;
		ModelDrawableInstance modelInstance, errorModelInstance;
		bool disableRetargetFile = false;
		Model * model = nullptr;
		RetargetFile * retargetFile = nullptr;
	protected:
		void UpdateBounds();
		void UpdateStates();
		void LocalTransform_Changing(VectorMath::Matrix4 & newTransform);
		void ModelFileName_Changing(CoreLib::String & newFileName);
		void RetargetFileName_Changing(CoreLib::String & newFileName);
	public:
		PROPERTY_ATTRIB(CoreLib::String, ModelFile, "resource(Mesh, model)");
		PROPERTY_ATTRIB(CoreLib::String, RetargetFileName, "resource(Animation, retarget);altname(RetargetFile)");
	public:
        VectorMath::Vec3 GetRootPosition();
        VectorMath::Vec3 GetRootOrientation();
        VectorMath::Matrix4 GetRootTransform();
		virtual void Tick() override;
		Model * GetModel()
		{
			return model;
		}
        void SetPose(const Pose & p);
		Pose GetPose();
		virtual void GetDrawables(const GetDrawablesParameter & params) override;
		virtual EngineActorType GetEngineType() override
		{
			return EngineActorType::Drawable;
		}
		virtual CoreLib::String GetTypeName() override
		{
			return "SkeletalMesh";
		}
		virtual void OnLoad() override;
		virtual void OnUnload() override;
	};
}

#endif