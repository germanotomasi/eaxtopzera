#include "includes.h"

BoneHandler g_bone_handler{};;
rebuilt_setupbones m_bone_handler{};;

__forceinline void m_AttachmentHelper(Entity* entity, CStudioHdr* hdr) {

    using AttachmentHelperFn = void(__thiscall*)(Entity*, CStudioHdr*);
    g_csgo.m_AttachmentHelper.as< AttachmentHelperFn  >()(entity, hdr);
}

bool BoneHandler::SetupBones( Player* player, BoneArray* pBoneMatrix, float time, bool disable_interp ) {
    player->m_bIsJiggleBonesEnabled() = false;

	const int effects = player->m_fEffects();
	const int lod_flags = player->m_iOcclusionFlags();
	const int anim_occlusion_frame_count = player->m_iOcclusionFramecount();
	const auto ik_ctx = player->m_pIKContext();
	const auto client_effects = player->m_ClientEntEffects();

	if( player != g_cl.m_local || disable_interp )
		player->m_fEffects() |= 8u;
	else
		player->m_fEffects() &= ~8u;

	player->m_iOcclusionFlags() &= ~2u;
	player->m_iOcclusionFramecount() = 0;
	player->m_flLastBoneSetupTime() = 0.f;
	player->RemoveIKContext();
	player->InvalidateBoneCache();
	// not sure of that one 
	// player->m_nCustomBlendingRuleMask() = -1;

	g_bone_handler.m_running = true;
	const bool result = player->SetupBones( pBoneMatrix, 128, 0x7FF00, time );
	g_bone_handler.m_running = false;

	player->m_pIKContext() = ik_ctx;
	player->m_fEffects() = effects;
	player->m_iOcclusionFlags() = lod_flags;
	player->m_iOcclusionFramecount() = anim_occlusion_frame_count;
	player->m_ClientEntEffects() = client_effects;

    return result;
}



bool BoneHandler::SetupBonesOnetap( Player* m_pPlayer, matrix3x4_t* m_pBones, bool m_bInterpolate )
{
	// backup vars.
	const int m_fBackupEffects = m_pPlayer->m_fEffects( );

	// backup globals
	const float m_flCurtime = g_csgo.m_globals->m_curtime;
	const float m_flRealTime = g_csgo.m_globals->m_realtime;
	const float m_flFrametime = g_csgo.m_globals->m_frametime;
	const float m_flAbsFrametime = g_csgo.m_globals->m_abs_frametime;
	const int m_iNextSimTick = m_pPlayer->m_flSimulationTime( ) / g_csgo.m_globals->m_interval + 1;
	const int m_nFrames = g_csgo.m_globals->m_frame;
	const int m_nTicks = g_csgo.m_globals->m_tick_count;

	// get jiggle bone cvar
	static ConVar* r_jiggle_bones = g_csgo.m_cvar->FindVar( HASH( "r_jiggle_bones" ) );

	// if jiggle bone isnt 0, force it to be 0
	if( r_jiggle_bones->GetInt( ) != 0 )
		r_jiggle_bones->SetValue( 0 );

	g_csgo.m_globals->m_curtime = m_pPlayer->m_flSimulationTime( );
	g_csgo.m_globals->m_realtime = m_pPlayer->m_flSimulationTime( );
	g_csgo.m_globals->m_frametime = g_csgo.m_globals->m_interval;
	g_csgo.m_globals->m_abs_frametime = g_csgo.m_globals->m_interval;
	g_csgo.m_globals->m_frame = m_iNextSimTick;
	g_csgo.m_globals->m_tick_count = m_iNextSimTick;

	// remove interpolation flag
	if ( !m_bInterpolate )
		m_pPlayer->m_fEffects( ) |= EF_NOINTERP;
	else
		m_pPlayer->m_fEffects( ) &= ~EF_NOINTERP;

	// setup bones
	const bool m_bRet = m_pPlayer->SetupBones( m_pBones, 128, BONE_USED_BY_ANYTHING, g_csgo.m_globals->m_curtime );
	
	// set back effects to their original state
	m_pPlayer->m_fEffects( ) = m_fBackupEffects;

	// restore globals.
	g_csgo.m_globals->m_curtime = m_flCurtime;
	g_csgo.m_globals->m_realtime = m_flRealTime;
	g_csgo.m_globals->m_frametime = m_flFrametime;
	g_csgo.m_globals->m_abs_frametime = m_flAbsFrametime;
	g_csgo.m_globals->m_frame = m_nFrames;
	g_csgo.m_globals->m_tick_count = m_nTicks;

	// return result
	return m_bRet;
}

bool BoneHandler::SetupBonesRebuild(Player* entity, BoneArray* pBoneMatrix, int nBoneCount, int boneMask, float time, int flags) {
	if (entity->m_nSequence() == -1)
		return false;

	if (boneMask == -1)
		boneMask = entity->m_iPrevBoneMask();

	boneMask = boneMask | 0x80000;

	int nLOD = 0;
	int nMask = BONE_USED_BY_VERTEX_LOD0;

	for (; nLOD < 8; ++nLOD, nMask <<= 1) {
		if (boneMask & nMask)
			break;
	}

	for (; nLOD < 8; ++nLOD, nMask <<= 1) {
		boneMask |= nMask;
	}

	auto model_bone_counter = g_csgo.InvalidateBoneCache.add(0x000A).to< size_t >();

	CBoneAccessor backup_bone_accessor = entity->m_BoneAccessor();
	CBoneAccessor* bone_accessor = &entity->m_BoneAccessor();

	if (!bone_accessor)
		return false;

	if (entity->m_iMostRecentModelBoneCounter() != model_bone_counter || (flags & BoneSetupFlags::ForceInvalidateBoneCache)) {
		if (FLT_MAX >= entity->m_flLastBoneSetupTime() || time < entity->m_flLastBoneSetupTime()) {
			bone_accessor->m_ReadableBones = 0;
			bone_accessor->m_WritableBones = 0;
			entity->m_flLastBoneSetupTime() = (time);
		}

		entity->m_iPrevBoneMask() = entity->m_iAccumulatedBoneMask();
		entity->m_iAccumulatedBoneMask() = 0;

		auto hdr = entity->GetModelPtr();
		if (hdr) { // profiler stuff
			((CStudioHdrEx*)hdr)->m_nPerfAnimatedBones = 0;
			((CStudioHdrEx*)hdr)->m_nPerfUsedBones = 0;
			((CStudioHdrEx*)hdr)->m_nPerfAnimationLayers = 0;
		}
	}

	entity->m_iAccumulatedBoneMask() |= boneMask;
	entity->m_iOcclusionFramecount() = 0;
	entity->m_iOcclusionFlags() = 0;
	entity->m_iMostRecentModelBoneCounter() = model_bone_counter;

	bool bReturnCustomMatrix = (flags & BoneSetupFlags::UseCustomOutput) && pBoneMatrix;

	CStudioHdr* hdr = entity->GetModelPtr();

	if (!hdr)
		return false;

	vec3_t origin = (flags & BoneSetupFlags::UseInterpolatedOrigin) ? entity->GetAbsOrigin() : entity->m_vecOrigin();
	ang_t angles = entity->GetAbsAngles();

	alignas(16) matrix3x4_t parentTransform;
	math::AngleMatrix(angles, origin, parentTransform);

	boneMask |= entity->m_iPrevBoneMask();

	if (bReturnCustomMatrix)
		bone_accessor->m_pBones = pBoneMatrix;

	int oldReadableBones = bone_accessor->m_ReadableBones;
	int oldWritableBones = bone_accessor->m_WritableBones;

	int newWritableBones = oldReadableBones | boneMask;

	bone_accessor->m_WritableBones = newWritableBones;
	bone_accessor->m_ReadableBones = newWritableBones;

	if (!(hdr->m_pStudioHdr->m_flags & 0x00000010)) {

		entity->m_fEffects() |= EF_NOINTERP;
		entity->m_iEFlags() |= EFL_SETTING_UP_BONES;

		entity->m_pIK() = 0;
		entity->m_ClientEntEffects() |= 2;

		alignas(16) vec3_t pos[128];
		alignas(16) quaternion_t q[128];

		entity->StandardBlendingRules(hdr, pos, q, time, boneMask);

		uint8_t computed[0x100];
		std::memset(computed, 0, 0x100);

		entity->BuildTransformations(hdr, pos, q, parentTransform, boneMask, computed);

		entity->m_iEFlags() &= ~EFL_SETTING_UP_BONES;

		// lol.
		// entity->m_fEffects() &= ~EF_NOINTERP;

	}
	else
		parentTransform = bone_accessor->m_pBones[0];

	if (bReturnCustomMatrix) {
		*bone_accessor = backup_bone_accessor;
		return true;
	}

	return true;
}

bool rebuilt_setupbones::setup_entity_bones(Player* player, matrix3x4_t* bone, int max_bones, int mask) {
	const int backup_ik = player->m_pIK();
	const int backup_client_effects = player->m_ClientEntEffects();
	const int backup_effects = player->m_fEffects(); // ( "CBaseEntity", "m_fEffects" )
	const int backup_last_non_skipped_frame_cnt = player->m_nLastNonSkippedFrame();
	const int backup_anim_lod_flags = *(int*)(uintptr_t(player) + 0xa28);
	const vec3_t backup_abs_origin = player->GetAbsOrigin(); // 10 index from c_base_ent virtual table..

	// get jiggle bones cvar
	static ConVar* jiggle_bones = g_csgo.m_cvar->FindVar(HASH("r_jiggle_bones"));

	// if jigglebones cvar is invalid, something is wrong return here
	if (!jiggle_bones)
		return false;

	player->m_pIK() = 0;
	player->InvalidateBoneCache();
	player->m_nLastNonSkippedFrame() = 0;
	player->m_iOcclusionFlags() &= ~2;
	player->m_nCustomBlendingRuleMask() = -1;
	player->m_ClientEntEffects() |= 2;
	player->m_fEffects() |= 8;

	// fix model sway...
	int v105 = -1;
	if (player->m_AnimOverlayCount() >= 12) {
		v105 = player->m_AnimOverlay()[12].m_weight;
		player->m_AnimOverlay()[12].m_weight = 0;
	}

	// set un-interpolated origin.
	if (player != g_cl.m_local)
		player->SetAbsOrigin(player->m_vecOrigin());

	// set time too.
	float time = 0;
	if (player != g_cl.m_local)
		time = player->m_flSimulationTime();
	else
		time = game::TICKS_TO_TIME(g_cl.m_local->m_nTickBase()); // server tick

	// es0 shizo.
	const int backup_frame_cnt = g_csgo.m_globals->m_frame;
	g_csgo.m_globals->m_frame = -999; // bypass should_skip_anim_frame.

	auto v107 = (float*)(uintptr_t(this) + (0x3AD0 + 4));
	auto v108 = (float*)(uintptr_t(player) + (0x6f20 + 0x4));

	float backup_unk = *v107;
	float backup_unk2 = *v108;

	*v107 = 0.0;
	*v108 = 0.0;

	// get jiggle bones
	const bool backup_jiggle_bones = jiggle_bones->GetInt();

	// null its value
	jiggle_bones->SetValue(0);

	// idc so mb use that for pvs fix or something else.
	// build bones.
	ishltv = true;
	bool premium_geng = player->SetupBones(bone, max_bones, mask, time);
	ishltv = false;

	// restore our jiggle bones
	jiggle_bones->SetValue(backup_jiggle_bones);

	// restore frame cnt.
	g_csgo.m_globals->m_frame = backup_frame_cnt;

	// restore unk funcs.
	*v107 = backup_unk;
	*v108 = backup_unk2;

	// restore prev.
	if (player->m_AnimOverlayCount() >= 12 && v105 >= 0)
		player->m_AnimOverlay()[12].m_weight = v105;

	// ..........
	player->m_pIK() = backup_ik;
	player->m_fEffects() = backup_effects;
	player->m_ClientEntEffects() = backup_client_effects;
	player->m_nAnimLODflags() = backup_anim_lod_flags;

	// restore interpolated origin.
	if (player != g_cl.m_local)
		player->SetAbsOrigin(backup_abs_origin);

	return premium_geng;
}