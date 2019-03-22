#include "SkeletalMeshActor.h"
#include "Engine.h"
#include "Skeleton.h"

namespace GameEngine
{
	void SkeletalMeshActor::UpdateBounds()
	{
		if (physInstance)
		{
			Bounds.Init();
			for (auto & obj : physInstance->objects)
				Bounds.Union(obj->GetBounds());
		}
	}

	void SkeletalMeshActor::UpdateStates()
	{
		if (model)
			physInstance = model->CreatePhysicsInstance(level->GetPhysicsScene(), this, nullptr);
		Tick();
	}

	void SkeletalMeshActor::LocalTransform_Changing(VectorMath::Matrix4 & newTransform)
	{
		if (physInstance)
			physInstance->SetTransform(newTransform, nextPose, retargetFile);
		if (errorPhysInstance)
			errorPhysInstance->SetTransform(newTransform);
	}

	void SkeletalMeshActor::ModelFileName_Changing(CoreLib::String & newFileName)
	{
		model = level->LoadModel(newFileName);
		if (!model)
			newFileName = "";
		modelInstance.Drawables.Clear();
		nextPose.Transforms.Clear();
		UpdateStates();
	}

	void SkeletalMeshActor::RetargetFileName_Changing(CoreLib::String & newFileName)
	{
		retargetFile = level->LoadRetargetFile(newFileName);
		if (!retargetFile)
			newFileName = "";
		UpdateStates();
	}

	void GameEngine::SkeletalMeshActor::Tick()
	{
		auto isPoseCompatible = [this]()
		{
			if (!model)
				return false;
			if (!model->GetSkeleton())
				return false;
			if (retargetFile)
			{
				if (retargetFile->RetargetedInversePose.Count() != model->GetSkeleton()->Bones.Count())
					return false;
				if (nextPose.Transforms.Count() < retargetFile->MaxAnimationBoneId + 1)
					return false;
				return true;
			}
			else if (nextPose.Transforms.Count() != model->GetSkeleton()->Bones.Count())
				return false;
			return true;
		};
		disableRetargetFile = false;
		if (!isPoseCompatible())
		{
			nextPose.Transforms.Clear();
			if (model)
			{
				auto srcSkeleton = model->GetSkeleton();
				nextPose.Transforms.SetSize(srcSkeleton->Bones.Count());
				for (int i = 0; i < nextPose.Transforms.Count(); i++)
					nextPose.Transforms[i] = srcSkeleton->Bones[i].BindPose;
			}
			disableRetargetFile = true;
		}
		if (physInstance)
		{
			physInstance->SetTransform(*LocalTransform, nextPose, disableRetargetFile ? nullptr : retargetFile);
		}
		if ((!model || nextPose.Transforms.Count() == 0) && !errorPhysInstance)
		{
			errorPhysInstance = level->LoadErrorModel()->CreatePhysicsInstance(level->GetPhysicsScene(), this, nullptr);
		}
		else
		{
			errorPhysInstance = nullptr;
		}
        if (errorPhysInstance)
            errorPhysInstance->SetTransform(*LocalTransform);
		UpdateBounds();
	}

	Pose SkeletalMeshActor::GetPose()
	{
		return nextPose;
	}

    void SkeletalMeshActor::SetPose(const Pose & p)
    {
        nextPose = p;
    }

    VectorMath::Vec3 SkeletalMeshActor::GetRootPosition()
    {
        VectorMath::Matrix4 rs = GetRootTransform();
        if (model && model->GetSkeleton())
        {
            auto pos = rs.TransformHomogeneous(model->GetSkeleton()->Bones[0].BindPose.Translation);
            return pos;
        }
        else
        {
            return rs.GetTranslation();
        }
    }

    VectorMath::Matrix4 SkeletalMeshActor::GetRootTransform()
    {
        VectorMath::Matrix4 rs;
        if (retargetFile)
        {
            VectorMath::Matrix4::Multiply(rs, nextPose.Transforms[0].ToMatrix(), retargetFile->RetargetedInversePose[0]);
        }
        else
        {
            if (model && model->GetSkeleton())
                VectorMath::Matrix4::Multiply(rs, nextPose.Transforms[0].ToMatrix(), model->GetSkeleton()->InversePose[0]);
            else
                rs = nextPose.Transforms[0].ToMatrix();
        }
        VectorMath::Matrix4::Multiply(rs, LocalTransform.GetValue(), rs);
        return rs;
    }

    VectorMath::Vec3 SkeletalMeshActor::GetRootOrientation()
    {
        VectorMath::Matrix4 rootTransform = GetRootTransform();
        VectorMath::Vec3 result;
        MatrixToEulerAngle(rootTransform.GetMatrix3(), result.x, result.y, result.z, VectorMath::EulerAngleOrder::ZXY);
        return result;
    }

    void SkeletalMeshActor::GetDrawables(const GetDrawablesParameter & params)
	{
        if (params.IsBaking)
            return;
		if (model && modelInstance.IsEmpty())
			modelInstance = model->GetDrawableInstance(params);
		if (!model || nextPose.Transforms.Count() == 0)
		{
			if (errorModelInstance.IsEmpty())
				errorModelInstance = level->LoadErrorModel()->GetDrawableInstance(params);
			errorModelInstance.UpdateTransformUniform(*LocalTransform);

			for (auto & drawable : errorModelInstance.Drawables)
			{
				drawable->CastShadow = CastShadow.GetValue();
				TransformBBox(drawable->Bounds, *LocalTransform, level->LoadErrorModel()->GetBounds());
				AddDrawable(params, drawable.Ptr(), drawable->Bounds);
			}
			return;
		}
		
		modelInstance.UpdateTransformUniform(*LocalTransform, nextPose, disableRetargetFile ? nullptr : retargetFile);
        AddDrawable(params, &modelInstance);
	}

	void SkeletalMeshActor::OnLoad()
	{
		model = level->LoadModel(*ModelFile);
		if (RetargetFile.GetValue().Length())
			retargetFile = level->LoadRetargetFile(*RetargetFile);
		
		LocalTransform.OnChanging.Bind(this, &SkeletalMeshActor::LocalTransform_Changing);
		UpdateStates();

        ModelFile.OnChanging.Bind(this, &SkeletalMeshActor::ModelFileName_Changing);
        RetargetFile.OnChanging.Bind(this, &SkeletalMeshActor::RetargetFileName_Changing);
	}

	void SkeletalMeshActor::OnUnload()
	{
		if (physInstance)
			physInstance->RemoveFromScene();
	}

}