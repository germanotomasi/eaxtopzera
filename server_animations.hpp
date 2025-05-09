#pragma once
#include "includes.h"
#include <optional>

class ServerAnimations {
	struct AnimationInfo_t {
		std::array<C_AnimationLayer, 13> m_pAnimOverlays;
		std::array<float, 20> m_pPoseParameters;

		float m_flFootYaw;
		float m_flLowerBodyYawTarget;
		float m_flLowerBodyRealignTimer;
		float m_flSpawnTime;

		CCSGOPlayerAnimState m_pAnimState;
		bool m_bInitializedAnimState;
		bool m_bBreakingTeleportDst;

		vec3_t m_vecBonePos[256];
		quaternion_t m_quatBoneRot[256];

		ang_t m_angUpdateAngles;

		bool m_bSetupBones;
		alignas(16) matrix3x4_t m_pMatrix[128];

		vec3_t m_vecLastOrigin;

		bool m_pLBYUpdated;
		bool m_allow_setup_bones;

		struct local_data_t {
			float m_spawn_time;

			struct anim_event_t {
				int m_move_type = 0;
				int m_flags = 0;
			} m_anim_event;

			__forceinline void reset() {
				m_spawn_time = 0.f;
				m_anim_event.m_move_type = 0;
				m_anim_event.m_flags = 0;
			}
		} m_local_data {};
	};


	bool IsModifiedLayer(int nLayer);

	void Simulate();

	float GetLayerIdealWeightFromSeqCycle(CCSGOPlayerAnimState* m_pAnimstate, C_AnimationLayer* pLayer);
	bool IsLayerSequenceCompleted(CCSGOPlayerAnimState* m_pAnimstate, C_AnimationLayer* pLayer);
	Activity GetLayerActivity(CCSGOPlayerAnimState* m_pAnimstate, C_AnimationLayer* pLayer);
	void SetEvents();
	void SetLayerInactive(C_AnimationLayer* pLayer, int idx);
void RebuildLand(C_AnimationLayer* layers); public:
private:
	void IncrementLayerCycle(CCSGOPlayerAnimState* m_pAnimstate, C_AnimationLayer* pLayer, bool bAllowLoop);
	void IncrementLayerWeight(CCSGOPlayerAnimState* m_pAnimstate, C_AnimationLayer* pLayer);
	void IncrementLayerCycleWeightRateGeneric(CCSGOPlayerAnimState* m_pAnimstate, C_AnimationLayer* pLayer);
	int  SelectWeightedSequenceFromModifiers(Player* pEntity, int32_t activity, CUtlVector<uint16_t> modifiers);

public:
	AnimationInfo_t m_uCurrentAnimations;
	AnimationInfo_t m_uServerAnimations;
	ang_t m_angChokedShotAngle;

	void HandleAnimations(CUserCmd* cmd);
	void HandleServerAnimation(CUserCmd* pCmd);
	void ShootPosition();
	// this shit actually gets called like 50 times in different spots coz eso genius so i will do the same
	void UpdateLandLayer(CUserCmd* pCmd);
	void setup_bones(BoneArray* out, float time, int custom_max);

};

extern ServerAnimations g_ServerAnimations;