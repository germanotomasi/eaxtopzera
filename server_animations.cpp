#include "includes.h"
#include "server_animations.hpp"
#include "pattern_scan.hpp"

ServerAnimations g_ServerAnimations;

#define USE_MDLCACHE

void ServerAnimations::IncrementLayerCycle(CCSGOPlayerAnimState* m_pAnimstate, C_AnimationLayer* pLayer, bool bAllowLoop) {
	if (!pLayer || !m_pAnimstate)
		return;

	if (!m_pAnimstate->m_player)
		return;

	if (abs(pLayer->m_playback_rate) <= 0)
		return;

	float flCurrentCycle = pLayer->m_cycle;
	flCurrentCycle += m_pAnimstate->m_last_update_increment * pLayer->m_playback_rate;

	if (!bAllowLoop && flCurrentCycle >= 1) {
		flCurrentCycle = 0.999f;
	}

	if (pLayer->m_cycle != math::ClampCycle(flCurrentCycle)) {
		pLayer->m_cycle = math::ClampCycle(flCurrentCycle);
		m_pAnimstate->m_player->InvalidatePhysicsRecursive(ANIMATION_CHANGED);
	}
}

void ServerAnimations::IncrementLayerWeight(CCSGOPlayerAnimState* m_pAnimstate, C_AnimationLayer* pLayer) {
	if (!pLayer)
		return;

	if (abs(pLayer->m_weight_delta_rate) <= 0.f)
		return;

	float flCurrentWeight = pLayer->m_weight;
	flCurrentWeight += m_pAnimstate->m_last_update_increment * pLayer->m_weight_delta_rate;
	flCurrentWeight = std::clamp(flCurrentWeight, 0.f, 1.f);

	if (pLayer->m_weight != flCurrentWeight) {
		pLayer->m_weight = flCurrentWeight;
	}
}

float ServerAnimations::GetLayerIdealWeightFromSeqCycle(CCSGOPlayerAnimState* m_pAnimstate, C_AnimationLayer* pLayer) {
	if (!pLayer)
		return 0.f;


	float flCycle = pLayer->m_cycle;
	if (flCycle >= 0.999f)
		flCycle = 1;

	float flEaseIn = pLayer->m_fade_out_time; // seqdesc.fadeintime;
	float flEaseOut = pLayer->m_fade_out_time; // seqdesc.fadeouttime;
	float flIdealWeight = 1;

	if (flEaseIn > 0 && flCycle < flEaseIn)
	{
		flIdealWeight = math::SmoothStepBounds(0, flEaseIn, flCycle);
	}
	else if (flEaseOut < 1 && flCycle > flEaseOut)
	{
		flIdealWeight = math::SmoothStepBounds(1.0f, flEaseOut, flCycle);
	}

	if (flIdealWeight < 0.0015f)
		return 0.f;

	return (std::clamp(flIdealWeight, 0.f, 1.f));
}

bool ServerAnimations::IsLayerSequenceCompleted(CCSGOPlayerAnimState* m_pAnimstate, C_AnimationLayer* pLayer) {
	if (pLayer) {
		return ((pLayer->m_cycle + (m_pAnimstate->m_last_update_increment * pLayer->m_playback_rate)) >= 1);
	}

	return false;
}

Activity ServerAnimations::GetLayerActivity(CCSGOPlayerAnimState* m_pAnimstate, C_AnimationLayer* pLayer) {
	if (!m_pAnimstate || !m_pAnimstate->m_player)
		return ACT_INVALID;

	if (pLayer) {
		return (Activity)m_pAnimstate->m_player->GetSequenceActivity(pLayer->m_sequence);
	}

	return ACT_INVALID;
}

void ServerAnimations::IncrementLayerCycleWeightRateGeneric(CCSGOPlayerAnimState* m_pAnimstate, C_AnimationLayer* pLayer) {
	float flWeightPrevious = pLayer->m_weight;
	IncrementLayerCycle(m_pAnimstate, pLayer, false);
	pLayer->m_weight = GetLayerIdealWeightFromSeqCycle(m_pAnimstate, pLayer);
	pLayer->m_weight_delta_rate = flWeightPrevious;
}

int ServerAnimations::SelectWeightedSequenceFromModifiers(Player* pEntity, int32_t activity, CUtlVector<uint16_t> modifiers) {
	typedef CStudioHdr::CActivityToSequenceMapping* (__thiscall* fnFindMapping)(void*);
	typedef int32_t(__thiscall* fnSelectWeightedSequenceFromModifiers)(void*, void*, int32_t);



	static auto FindMappingAdr = memory_scan::search(XOR("client.dll"), XOR("55 8B EC 83 E4 F8 81 EC ? ? ? ? 53 56 57 8B F9 8B 17"));
	static auto SelectWeightedSequenceFromModifiersAdr = memory_scan::search(XOR("server.dll"), XOR("55 8B EC 83 EC 2C 53 56 8B 75 08 8B D9"));

	auto pHdr = pEntity->m_studioHdr();
	if (!pHdr) {
		return -1;
	}

	const auto pMapping = (FindMappingAdr.get<fnFindMapping>()(pHdr));
	if (!pHdr->m_pActivityToSequence) {
		pHdr->m_pActivityToSequence = pMapping;
	}

	return (SelectWeightedSequenceFromModifiersAdr.get<fnSelectWeightedSequenceFromModifiers>()(pMapping, pHdr, activity));
}

bool ServerAnimations::IsModifiedLayer(int nLayer) {
	// note - michal;
	// in the future, we should look into rebuilding each layer (or atleast the most curcial ones,
	// such as weapon_action, movement_move, whole_body, etc). i only rebuilt these for the time being
	// as they're the only layers we really need to rebuild, they're responsible for eyepos when landing
	// (also known as "landing comp" kek) etc. plus once they're fixed the animations are eye candy :-)
	return (nLayer == ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB || nLayer == ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL);
}

void ServerAnimations::setup_bones(BoneArray* out, float time, int custom_max) {
	const auto cur_time = g_csgo.m_globals->m_curtime;
	const auto real_time = g_csgo.m_globals->m_realtime;
	const auto frame_time = g_csgo.m_globals->m_frametime;
	const auto abs_frame_time = g_csgo.m_globals->m_abs_frametime;
	const auto tick_count = g_csgo.m_globals->m_tick_count;
	const auto interp_amt = g_csgo.m_globals->m_interp_amt;
	const auto frame_count = g_csgo.m_globals->m_frame;

	g_csgo.m_globals->m_curtime = time;
	g_csgo.m_globals->m_realtime = time;
	g_csgo.m_globals->m_frametime = g_csgo.m_globals->m_interval;
	g_csgo.m_globals->m_abs_frametime = g_csgo.m_globals->m_interval;
	g_csgo.m_globals->m_frame = game::TIME_TO_TICKS(time);
	g_csgo.m_globals->m_tick_count = game::TIME_TO_TICKS(time);
	g_csgo.m_globals->m_interp_amt = 0.f;


	const auto effects = g_cl.m_local->m_fEffects();
	const auto lod_flags = g_cl.m_local->m_nAnimLODflags();
	const auto anim_occlusion_frame_count = g_cl.m_local->m_iOcclusionFramecount();
	const auto ik_ctx = g_cl.m_local->m_pIK();
	const auto client_effects = g_cl.m_local->m_ClientEntEffects();

	g_cl.m_local->m_fEffects() |= 8u;
	g_cl.m_local->m_nAnimLODflags() &= ~2u;
	g_cl.m_local->m_iOcclusionFramecount() = 0;

	g_cl.m_local->m_pIK() = -1; // nullptr
	g_cl.m_local->m_ClientEntEffects() |= 2u;

	g_cl.m_local->m_flLastBoneSetupTime() = 0.f;

	g_cl.m_local->InvalidateBoneCache();

	static auto jiggle_bones = g_csgo.m_cvar->FindVar(HASH("r_jiggle_bones"));

	const auto backup_jiggle_bones = jiggle_bones->GetInt();

	jiggle_bones->SetValue(0);

	const auto should_anim_bypass = g_csgo.m_globals->m_frame;
	g_csgo.m_globals->m_frame = -999;

	g_ServerAnimations.m_uCurrentAnimations.m_allow_setup_bones = true;
	if (custom_max == -1)
		g_cl.m_local->SetupBones(out, 256, 0x7FF00, time); // 256 if local
	else
		g_cl.m_local->SetupBones(out, 256, 0x100, time);

	g_ServerAnimations.m_uCurrentAnimations.m_allow_setup_bones = true;

	g_csgo.m_globals->m_frame = should_anim_bypass;

	g_cl.m_local->m_pIK() = ik_ctx;

	jiggle_bones->SetValue(backup_jiggle_bones);

	g_cl.m_local->m_fEffects() = effects;
	g_cl.m_local->m_nAnimLODflags() = lod_flags;
	g_cl.m_local->m_iOcclusionFramecount() = anim_occlusion_frame_count;

	g_cl.m_local->m_ClientEntEffects() = client_effects;

	g_csgo.m_globals->m_curtime = cur_time;
	g_csgo.m_globals->m_realtime = real_time;
	g_csgo.m_globals->m_abs_frametime = abs_frame_time;
	g_csgo.m_globals->m_frametime = frame_time;
	g_csgo.m_globals->m_tick_count = tick_count;
	g_csgo.m_globals->m_frame = frame_count;
	g_csgo.m_globals->m_interp_amt = interp_amt;
}

void ServerAnimations::RebuildLand(C_AnimationLayer* layers) {
	auto state = g_cl.m_local->m_PlayerAnimState();
	if (!state)
		return;

	// setup layers that we want to use/fix
	C_AnimationLayer* ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB = &layers[animstate_layer_t::ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];
	C_AnimationLayer* ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL = &layers[animstate_layer_t::ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];

	// setup ground and flag stuff
	int fFlags = g_cl.m_local->m_fFlags();
	static bool bOnGround = false;
	bool bWasOnGround = bOnGround;
	bOnGround = (fFlags & 1);

	static float m_flLadderWeight = 0.f, m_flLadderSpeed = 0.f;

	// not sure about these two, they don't seem wrong though
	bool bLeftTheGroundThisFrame = bWasOnGround && !bOnGround;
	bool bLandedOnGroundThisFrame = !bWasOnGround && bOnGround;
	static bool m_bAdjustStarted = false;

	// ladder stuff
	static bool bOnLadder = false;
	bool bPreviouslyOnLadder = bOnLadder;
	bOnLadder = !bOnGround && g_cl.m_local->m_MoveType() == MOVETYPE_LADDER;
	bool bStartedLadderingThisFrame = (!bPreviouslyOnLadder && bOnLadder);
	bool bStoppedLadderingThisFrame = (bPreviouslyOnLadder && !bOnLadder);

	// TODO: fix the rest of the layers, for now I'm only fixing the land and jump layer
	// because until I get this working without any fuckery, there is no point to continue
	// trying to fix every other layer.
	// see: CSGOPlayerAnimState::SetUpMovement
	int nSequence = 0;

	// fix ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB
	if (bOnLadder) {
		if (bStartedLadderingThisFrame) {
			state->set_layer_seq(ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, ACT_CSGO_CLIMB_LADDER);
		}

		if (abs(state->m_velocity_length_z) > 100) {
			m_flLadderSpeed = math::ApproachAngle(1, m_flLadderSpeed, state->m_last_update_increment * 10.0f);
		}
		else {
			m_flLadderSpeed = math::ApproachAngle(0, m_flLadderSpeed, state->m_last_update_increment * 10.0f);
		}
		m_flLadderSpeed = std::clamp(m_flLadderSpeed, 0.f, 1.f);

		if (bOnLadder) {
			m_flLadderWeight = math::ApproachAngle(1, m_flLadderWeight, state->m_last_update_increment * 5.0f);
		}
		else {
			m_flLadderWeight = math::ApproachAngle(0, m_flLadderWeight, state->m_last_update_increment * 10.0f);
		}
		m_flLadderWeight = std::clamp(m_flLadderWeight, 0.f, 1.f);

		vec3_t vecLadderNormal = g_cl.m_local->m_vecLadderNormal();
		ang_t angLadder;

		math::VectorAngles(vecLadderNormal, angLadder);
		float flLadderYaw = math::AngleDiff(angLadder.y, state->m_abs_yaw);
		state->m_pose_param_mappings[LADDER_YAW].SetValue(g_cl.m_local, flLadderYaw);

		float flLadderClimbCycle = ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB->m_cycle;
		flLadderClimbCycle += (state->m_position_current.z - state->m_position_last.z) * math::Lerp(m_flLadderSpeed, 0.010f, 0.004f);

		state->m_pose_param_mappings[LADDER_SPEED].SetValue(g_cl.m_local, m_flLadderSpeed);

		if (GetLayerActivity(state, ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB) == ACT_CSGO_CLIMB_LADDER) {
			ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB->m_weight = m_flLadderWeight;
		}

		ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB->m_cycle = flLadderClimbCycle;

		// fade out jump if we're climbing
		if (bOnLadder) {
			float flIdealJumpWeight = 1.0f - m_flLadderWeight;
			if (ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL->m_weight > flIdealJumpWeight)
			{
				ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL->m_weight = flIdealJumpWeight;
			}
		}
	}
	// fix ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL
	else {
		//?????
		if (state->m_on_ground) {
			state->m_duration_in_air = 0;
		}

		m_flLadderSpeed = m_flLadderWeight = 0.f;

		// TODO: bStoppedLadderingThisFrame
		if (bLandedOnGroundThisFrame) {
			// setup the sequence responsible for landing
			nSequence = 20;
			if (state->m_speed_as_portion_of_walk_top_speed > .25f)
				nSequence = 22;

			IncrementLayerCycle(state, ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, false);
			IncrementLayerCycle(state, ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, false);

			state->m_pose_param_mappings[JUMP_FALL].SetValue(g_cl.m_local, 0);

			if (IsLayerSequenceCompleted(state, ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB)) {
				state->m_landing = false;
				ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB->m_weight = 0.f;
				ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL->m_weight = 0.f;
				state->m_land_anim_multiplier = 1.0f;
			}
			else {
				float flLandWeight = GetLayerIdealWeightFromSeqCycle(state, ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB) * state->m_land_anim_multiplier;

				// if we hit the ground crouched, reduce the land animation as a function of crouch, since the land animations move the head up a bit ( and this is undesirable )
				flLandWeight *= std::clamp((1.0f - state->m_anim_duck_amount), 0.2f, 1.0f);

				ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB->m_weight = flLandWeight;

				// fade out jump because land is taking over
				float flCurrentJumpFallWeight = ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL->m_weight;
				if (flCurrentJumpFallWeight > 0) {
					flCurrentJumpFallWeight = math::ApproachAngle(0, flCurrentJumpFallWeight, state->m_last_update_increment * 10.0f);
					ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL->m_weight = flCurrentJumpFallWeight;
				}
			}

			state->set_layer_seq(ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, -1, nSequence);
		}
		else if (bLeftTheGroundThisFrame) {
			// setup the sequence responsible for jumping
			if (state->m_speed_as_portion_of_walk_top_speed > .25f) {
				nSequence = state->m_anim_duck_amount > .55f ? 18 : 16;
			}
			else {
				nSequence = state->m_anim_duck_amount > .55f ? 17 : 15;
			}

			state->set_layer_seq(ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, -1, nSequence);
		}

		// blend jump into fall. This is a no-op if we're playing a fall anim.
		state->m_pose_param_mappings[JUMP_FALL].SetValue(g_cl.m_local, std::clamp(math::SmoothStepBounds(0.72f, 1.52f, state->m_duration_in_air), 0.f, 1.f));
	}

	// apply our changes
	layers[animstate_layer_t::ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB] = *ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB;
	layers[animstate_layer_t::ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL] = *ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL;
}

// fix this
void ServerAnimations::SetEvents() {
	C_AnimationLayer* ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB = &g_cl.m_local->m_AnimOverlay()[animstate_layer_t::ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];
	C_AnimationLayer* ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL = &g_cl.m_local->m_AnimOverlay()[animstate_layer_t::ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];

	int nCurrentFlags = m_uCurrentAnimations.m_local_data.m_anim_event.m_flags;
	int nCurrentMoveType = m_uCurrentAnimations.m_local_data.m_anim_event.m_move_type;

	// apply correct sequences
	if (nCurrentMoveType != MOVETYPE_LADDER && g_cl.m_local->m_MoveType() == MOVETYPE_LADDER)
		g_cl.m_local->m_PlayerAnimState()->set_layer_seq(ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB, ACT_CSGO_CLIMB_LADDER);
	else if (nCurrentMoveType == MOVETYPE_LADDER && g_cl.m_local->m_MoveType() != MOVETYPE_LADDER)
		g_cl.m_local->m_PlayerAnimState()->set_layer_seq(ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, ACT_CSGO_FALL);
	else
	{
		if (g_cl.m_local->m_fFlags() & FL_ONGROUND)
		{
			if (!(nCurrentFlags & FL_ONGROUND))
				g_cl.m_local->m_PlayerAnimState()->set_layer_seq(
					ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB,
					g_cl.m_local->m_PlayerAnimState()->m_duration_in_air > 1.0f ? ACT_CSGO_LAND_HEAVY : ACT_CSGO_LAND_LIGHT
				);
		}
		else if (nCurrentFlags & FL_ONGROUND)
		{
			if (g_cl.m_local->m_vecVelocity().z > 0.0f)
				g_cl.m_local->m_PlayerAnimState()->set_layer_seq(ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, ACT_CSGO_JUMP);
			else
				g_cl.m_local->m_PlayerAnimState()->set_layer_seq(ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL, ACT_CSGO_FALL);
		}
	}

	m_uCurrentAnimations.m_local_data.m_anim_event.m_move_type = g_cl.m_local->m_MoveType();
	m_uCurrentAnimations.m_local_data.m_anim_event.m_flags = g_cl.m_local->m_fFlags();
}

void ServerAnimations::SetLayerInactive(C_AnimationLayer* layers, int idx) {
	if (!layers)
		return;

	layers[idx].m_cycle = 0.f;
	layers[idx].m_sequence = 0.f;
	layers[idx].m_weight = 0.f;
	layers[idx].m_playback_rate = 0.f;
}

void ServerAnimations::HandleServerAnimation(CUserCmd* pCmd) {
	auto pLocal = g_cl.m_local;
	if (!pLocal)
		return;

	// perform basic sanity checks
	if (!pLocal->alive())
		return;

	auto pState = pLocal->m_PlayerAnimState();
	if (!pState)
		return;

	float            backup_poses[24];
	C_AnimationLayer backup_layers[13];

	// store layers and poses.
	pLocal->GetPoseParameters(backup_poses);
	pLocal->GetAnimLayers(backup_layers);

	// apply animation layers.
	pLocal->SetPoseParameters(g_cl.m_poses);
	pLocal->SetAnimLayers(g_cl.m_layers);

	// set absolute yaw.
	pLocal->SetAbsAngles(ang_t(0.0f, g_cl.m_abs_yaw, 0.0f));

	// build bones
	g_cl.m_setupped = m_bone_handler.setup_entity_bones(pLocal, nullptr, 128, BONE_USED_BY_ANYTHING);

	// restore.
	pLocal->SetPoseParameters(backup_poses);
	pLocal->SetAnimLayers(backup_layers);
}

void CCSGOPlayerAnimState::set_layer_seq(C_AnimationLayer* layer, int act, int overrideseq)
{
	int32_t sequence = select_sequence_from_acitivty_modifier(act);

	if (overrideseq != -1)
		sequence = overrideseq;

	if (sequence < 2)
		return;

	layer->m_cycle = 0.0f;
	layer->m_weight = 0.0f;
	layer->m_sequence = sequence;
	layer->m_playback_rate = g_cl.m_local->get_layer_seq_cycle_rate(layer, sequence);
}

void ServerAnimations::HandleAnimations(CUserCmd* cmd) {
	auto pLocal = g_cl.m_local;
	if (!pLocal)
		return;

	// perform basic sanity checks
	if (!pLocal->alive())
		return;

	auto pState = pLocal->m_PlayerAnimState();
	if (!pState)
		return;

	if (g_cl.m_lag > 0)
		return;

	// null out incorrect data
	g_cl.m_local->some_ptr() = nullptr;

	// get tickbase in time and modify it.
	const float tickbase = game::TICKS_TO_TIME(g_cl.m_local->m_nTickBase());
	const float tickbase_interval = (tickbase / g_csgo.m_globals->m_interval) + .5f;

	// update time.
	g_cl.m_anim_frame = g_csgo.m_globals->m_curtime - g_cl.m_anim_time;
	g_cl.m_anim_time = g_csgo.m_globals->m_curtime;

	// current angle will be animated.
	g_cl.m_angle = cmd->m_view_angles;

	// fix landing anim.
	if (pState->m_landing && (g_inputpred.m_predicted_flags & FL_ONGROUND) && (pLocal->m_fFlags() & FL_ONGROUND)) {
		g_cl.m_angle.x = -10.f;
	}

	g_cl.m_angle.x = std::clamp(g_cl.m_angle.x, -90.f, 90.f);
	g_cl.m_angle.normalize();

	// write angles to model.
	g_csgo.m_prediction->SetLocalViewAngles(g_cl.m_angle);

	// set lby to predicted value.
	pLocal->m_flLowerBodyYawTarget() = g_cl.m_body;

	// CCSGOPlayerAnimState::Update, bypass already animated checks.
	if (pState->m_last_update_frame >= tickbase_interval)
		pState->m_last_update_frame = tickbase_interval - 1;

	// update anim update delta like the server does.
	pState->m_last_update_increment = std::max(0.0f, g_csgo.m_globals->m_curtime - pState->m_last_update_time);

	// prevent C_BaseEntity::CalcAbsoluteVelocity being called
	pLocal->m_iEFlags() &= ~EFL_DIRTY_ABSVELOCITY;

	// snap to footyaw, instead of approaching
	pState->m_move_weight = 0.f;

	// is it time to update?
	if (g_csgo.m_globals->m_curtime != pState->m_last_update_time) {

		// update our animation state.
		game::UpdateAnimationState(pState, ang_t(g_cl.m_angle.x, g_cl.m_angle.y, g_cl.m_angle.z));

		// update animations.
		const bool backup = pLocal->m_bClientSideAnimation();

		// call original, bypass hook.
		pLocal->m_bClientSideAnimation() = g_cl.m_update = true;
		pLocal->UpdateClientSideAnimation();
		g_cl.m_update = false;

		// restore client side animation.
		pLocal->m_bClientSideAnimation() = backup;

		// store updated abs yaw.
		g_cl.m_abs_yaw = pState->m_abs_yaw;

		// grab updated layers & poses.
		pLocal->GetAnimLayers(g_cl.m_layers);
		pLocal->GetPoseParameters(g_cl.m_poses);
	}

	// handle animation events on client
	//HandleAnimationEvents(pLocal, pState, pLocal->m_AnimOverlay(), cmd);
	// nigger nigger sync nigger nigger
	RebuildLand(g_cl.m_local->m_AnimOverlay()); // updated layers

	// note - michal;
	// might want to make some storage for constant anim variables
	const float CSGO_ANIM_LOWER_REALIGN_DELAY{ 1.1f };

	if (pState->m_on_ground)
	{
		// rebuild server CCSGOPlayerAnimState::SetUpVelocity
		// predict m_flLowerBodyYawTarget
		if (pState->m_velocity_length_xy > 0.1f) {
			m_uServerAnimations.m_pLBYUpdated = true;
			g_cl.m_body_pred = g_csgo.m_globals->m_curtime + (CSGO_ANIM_LOWER_REALIGN_DELAY * 0.2f);
			g_cl.m_body = pState->m_eye_yaw;
		}
		// note - michal;
		// hello ledinchik men so if you've noticed our fakewalk breaks/stops for a while if we don't use "random" 
		// fake yaw option, coz random swaps flick side making this footyaw and eyeyaw delta pretty much always > 35.f
		// and the other options only flick to one side and due to something something footyaw being weird when flicking 
		// the delta jumps below 35, which causes the fakewalk to freak out and stop
		// so if we remove the delta check the fakewalk will work perfectly on every lby breaker option (but sometimes fail cos obv it failed on server)
		// only way i can think of fixing this without removing the delta check (coz ur comment below is right) is to force flick further or smth or 
		// somehow make sure that the footyaw is always at a bigger than 35deg delta from eyeyaw, whether that'd be by recalculating it ourselves
		// or maybe doing some other fuckery shit IDK!!
		// TLDR: fakewalk stops for 2 hours coz of the delta check (vague asf)
		else if (g_csgo.m_globals->m_curtime > g_cl.m_body_pred && abs(math::AngleDiff(pState->m_abs_yaw, pState->m_eye_yaw)) > 35.0f) {
			//
			// actually angle diff check is needed cause else lby breaker will see update when it didn't happen on server which is going to desync the timer
			// -- L3D415R7
			//

			m_uServerAnimations.m_pLBYUpdated = true;
			g_cl.m_body_pred = g_csgo.m_globals->m_curtime + CSGO_ANIM_LOWER_REALIGN_DELAY;
			g_cl.m_body = pState->m_eye_yaw;
		}
	}

	// remove model sway
	pLocal->m_AnimOverlay()[animstate_layer_t::ANIMATION_LAYER_LEAN].m_weight = 0.f;

	// save updated data.
	g_cl.m_speed = pState->m_velocity_length_xy;
	g_cl.m_ground = pState->m_on_ground;
}

// not used either remove or fix this to work with the matrix
// we dont need this but it would be good to have it
void ServerAnimations::ShootPosition()
{
	const auto abs_origin = g_cl.m_local->m_vecOrigin(); // restore bones to the ones that will be used by the server
	auto* bone_cache = &g_cl.m_local->m_BoneCache();
	BoneArray* backup_cache = nullptr;
	if (bone_cache && g_cl.m_setupped)
	{
		for (auto i = 0; i < 128; i++)
		{
			g_cl.m_real_bones[i].m_flMatVal[0][3] += abs_origin.x; // adjust bones positions to player origin
			g_cl.m_real_bones[i].m_flMatVal[1][3] += abs_origin.y;
			g_cl.m_real_bones[i].m_flMatVal[2][3] += abs_origin.z;
		}
		backup_cache = bone_cache->m_pCachedBones;
		bone_cache->m_pCachedBones = g_cl.m_real_bones;
	}
	g_cl.m_local->GetEyePos(
		&g_cl.m_shoot_pos); // get proper shoot position, we can regenerate a more accurate one where its needed
	if (g_cl.m_setupped && bone_cache)
	{
		g_cl.ModifyEyePosition(g_cl.m_local->m_PlayerAnimState(), g_cl.m_real_bones, &g_cl.m_shoot_pos);
		for (auto i = 0; i < 128; i++)
		{
			g_cl.m_real_bones[i].m_flMatVal[0][3] -= abs_origin.x; // adjust bones positions back to 3d origin
			g_cl.m_real_bones[i].m_flMatVal[1][3] -= abs_origin.y;
			g_cl.m_real_bones[i].m_flMatVal[2][3] -= abs_origin.z;
		}
		bone_cache->m_pCachedBones = backup_cache;
	}
}