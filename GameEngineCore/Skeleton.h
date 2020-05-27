#ifndef GAME_ENGINE_RIG_H
#define GAME_ENGINE_RIG_H

#include "CoreLib/Basic.h"
#include "CoreLib/VectorMath.h"
#include "CoreLib/Stream.h"

namespace GameEngine
{
	class BoneTransformation
	{
	public:
		VectorMath::Quaternion Rotation;
		VectorMath::Vec3 Translation, Scale;
		BoneTransformation()
		{
			Rotation = VectorMath::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
			Translation.SetZero();
			Scale.x = Scale.y = Scale.z = 1.0f;
		}
		void FromMatrix(VectorMath::Matrix4 m)
		{
			Scale.x = VectorMath::Vec3::Create(m.values[0], m.values[1], m.values[2]).Length();
			Scale.y = VectorMath::Vec3::Create(m.values[4], m.values[5], m.values[6]).Length();
			Scale.z = VectorMath::Vec3::Create(m.values[8], m.values[9], m.values[10]).Length();
			m.values[0] *= (1.0f / Scale.x);
			m.values[1] *= (1.0f / Scale.x);
			m.values[2] *= (1.0f / Scale.x);
			m.values[4] *= (1.0f / Scale.y);
			m.values[5] *= (1.0f / Scale.y);
			m.values[6] *= (1.0f / Scale.y);
			m.values[8] *= (1.0f / Scale.z);
			m.values[9] *= (1.0f / Scale.z);
			m.values[10] *= (1.0f / Scale.z);
			Rotation = VectorMath::Quaternion::FromMatrix(m.GetMatrix3());
			Translation = VectorMath::Vec3::Create(m.values[12], m.values[13], m.values[14]);
		}
        void SetYawAngle(float yaw)
        {
            VectorMath::Matrix4 roty;
            VectorMath::Matrix4::RotationY(roty, yaw);
            VectorMath::Matrix4 original = Rotation.ToMatrix4();
            VectorMath::Matrix4::Multiply(original, roty, original);
            Rotation = VectorMath::Quaternion::FromMatrix(original.GetMatrix3());
        }
		VectorMath::Matrix4 ToMatrix() const
		{
			auto rs = Rotation.ToMatrix4();
			rs.values[12] = Translation.x;
			rs.values[13] = Translation.y;
			rs.values[14] = Translation.z;
			rs.values[0] *= Scale.x;
			rs.values[1] *= Scale.x;
			rs.values[2] *= Scale.x;
			rs.values[4] *= Scale.y;
			rs.values[5] *= Scale.y;
			rs.values[6] *= Scale.y;
			rs.values[8] *= Scale.z;
			rs.values[9] *= Scale.z;
			rs.values[10] *= Scale.z;
			return rs;
		}
		static inline BoneTransformation Lerp(const BoneTransformation & t0, const BoneTransformation & t1, float t)
		{
			BoneTransformation result;
			auto rot1 = t1.Rotation;
			if (VectorMath::Quaternion::Dot(t0.Rotation, t1.Rotation) < 0.0f)
			{
				rot1 = -rot1;
			}
			result.Rotation = VectorMath::Quaternion::Slerp(t0.Rotation, rot1, t);
			result.Translation = VectorMath::Vec3::Lerp(t0.Translation, t1.Translation, t);
			result.Scale = VectorMath::Vec3::Lerp(t0.Scale, t1.Scale, t);
			return result;
		}
	};

	class Bone
	{
	public:
		int ParentId;
		CoreLib::String Name;
		BoneTransformation BindPose;
	};

	class RetargetFile
	{
	public:
		CoreLib::String SourceSkeletonName, TargetSkeletonName;
		/*
		final model pos = AnimationPose * AnimationRetargetTransform * SourceRetargetTransform * InverseBindPose * modelVertPos
		*/
		CoreLib::List<VectorMath::Quaternion> SourceRetargetTransforms; // this is user input from retargeting tool
		CoreLib::List<int> ModelBoneIdToAnimationBoneId; // this is user specified source model bone to animation bone mapping (which animation bone to use for driving each source bone)
		CoreLib::List<VectorMath::Matrix4> RetargetedInversePose; // equals to InverseAnimationSkeletonBindPose * SourceRetargetTransform * InverseBindPose
		CoreLib::List<VectorMath::Vec3> RetargetedBoneOffsets;
		VectorMath::Vec3 RootTranslationScale = VectorMath::Vec3::Create(1.0f, 1.0f, 1.0f);
		int MaxAnimationBoneId = 0;
		void SaveToStream(CoreLib::IO::Stream * stream);
		void LoadFromStream(CoreLib::IO::Stream * stream);
		void SaveToFile(const CoreLib::String & filename);
		void LoadFromFile(const CoreLib::String & filename);

		void SetBoneCount(int count);
	};

	class Skeleton
	{
	public:
		CoreLib::String Name;
		CoreLib::List<Bone> Bones;
		CoreLib::List<VectorMath::Matrix4> InversePose;
		CoreLib::EnumerableDictionary<CoreLib::String, int> BoneMapping;
        Skeleton TopologySort();
		void SaveToStream(CoreLib::IO::Stream * stream);
		void LoadFromStream(CoreLib::IO::Stream * stream);
		void SaveToFile(const CoreLib::String & filename);
		void LoadFromFile(const CoreLib::String & filename);
	};

	class Pose
	{
	public:
		CoreLib::List<BoneTransformation> Transforms;
        CoreLib::Dictionary<CoreLib::String, float> BlendShapeWeights;
		void GetMatrices(const Skeleton * skeleton, CoreLib::List<VectorMath::Matrix4> & matrices, bool multiplyInversePose = true, RetargetFile * retarget = nullptr) const;
	};

	class AnimationKeyFrame
	{
	public:
		float Time;
		BoneTransformation Transform;
	};

	template <typename TKeyFrame> int BinarySearchForKeyFrame(CoreLib::List<TKeyFrame> &KeyFrames, float time)
    {
        int begin = 0;
        int end = KeyFrames.Count();
        while (begin < end)
        {
            int mid = (begin + end) >> 1;
            if (KeyFrames[mid].Time > time)
                end = mid;
            else if (KeyFrames[mid].Time == time)
                return mid;
            else
                begin = mid + 1;
        }
        if (begin >= KeyFrames.Count())
            begin = KeyFrames.Count() - 1;
        if (KeyFrames[begin].Time > time)
            begin--;
        if (begin < 0)
            begin = 0;
        return begin;
    }

	class AnimationChannel
	{
	public:
		CoreLib::String BoneName;
		int BoneId = -1;
		CoreLib::List<AnimationKeyFrame> KeyFrames;
		BoneTransformation Sample(float time);
	};

	struct BlendShapeAnimationKeyFrame
    {
		float Time;
        float Weight;
    };

	struct BlendShapeAnimationChannel
    {
        CoreLib::String Name;
        CoreLib::List<BlendShapeAnimationKeyFrame> KeyFrames;
        float Sample(float time);
    };

	class SkeletalAnimation
	{
	public:
		CoreLib::String Name;
		float Speed;
		float Duration;
		float FPS;
        int BlendShapeChannelCount = 0;
        int Reserved[14] = {};
		CoreLib::List<AnimationChannel> Channels;
        CoreLib::List<BlendShapeAnimationChannel> BlendShapeChannels;
		void SaveToStream(CoreLib::IO::Stream * stream);
		void LoadFromStream(CoreLib::IO::Stream * stream);
		void SaveToFile(const CoreLib::String & filename);
		void LoadFromFile(const CoreLib::String & filename);
	};
}

#endif