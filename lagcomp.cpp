#include "includes.h"

Extrapolation g_extrapolation{};;

bool Extrapolation::HandleLagCompensation(AimPlayer* data) {

	// reset delayed state
	data->m_delayed = false;


	// we have no data to work with.
	// this should never happen if we call this
	if (data->m_records.empty() || data->m_records.size() <= 2)
		return false;

	// meme.
	if (data->m_player->dormant())
		return false;

	// get first record.
	LagRecord* record = data->m_records[0].get();



	// reset all prediction related variables.
	// this has been a recurring problem in all my hacks lmfao.
	// causes the prediction to stack on eachother.
	record->predict();
	record->m_broke_lc = record->broke_lc();

	// we are not breaking lagcomp at this point.
	// return false so it can aim at all the records at once since server-sided lagcomp is still active and we can abuse that.
	if (!record->m_broke_lc)
		return false;


	if (g_menu.main.aimbot.fakelag_correction.get() == 0)
		return true;

	const int sv_ticks_since_upd = g_cl.m_server_tick - record->m_tick;

	// hardcoded for now
	if (sv_ticks_since_upd <= 1)
		return true;

	if (sv_ticks_since_upd >= g_cl.m_latency_ticks) {
		data->m_delayed = true;
		return true;
	}

	if (record->m_lag > 16) {
		data->m_delayed = true;
		return true;
	}

	const float outgoing = g_csgo.m_engine->GetNetChannelInfo()->GetLatency(INetChannel::FLOW_OUTGOING);
	const int receive_tick = std::abs((g_csgo.m_cl->m_server_tick + (game::TIME_TO_TICKS(outgoing))) - game::TIME_TO_TICKS(record->m_sim_time));

	const float delta = static_cast<float>(receive_tick) / static_cast<float>(record->m_lag);
	const int adjusted_arrive_tick = std::clamp(game::TIME_TO_TICKS(((outgoing)+g_csgo.m_globals->m_realtime) - record->m_realtime), 0, 100);

	if (g_menu.main.aimbot.fakelag_correction.get() == 1) {
		data->m_delayed = true;
		return true;
	}

	float total_latency = g_csgo.m_net->GetLatency(0) + g_csgo.m_net->GetLatency(1);
	const float time_delta = record->get_time_delta();

	// if there will be no delay if we reached max lag
	// however it will be delayed by 1tick if there is remaining lag to choke
	const int delay = g_cl.m_lag >= g_cl.m_max_lag ? 0 : 1;
	const int latency_ticks = game::TIME_TO_TICKS(g_csgo.m_net->GetLatency(0) + g_csgo.m_net->GetLatency(1));
	const float delta2 = static_cast<float>(delay + g_csgo.m_cl->m_server_tick + latency_ticks - record->m_tick) / static_cast<float>(record->m_lag);
	const float max = static_cast<float>(game::TIME_TO_TICKS(time_delta - 0.2f)) / static_cast<float>(record->m_lag);
	const auto clamped_delta = std::min(delta2, max);

	extrapolation_data_t pred_data{ data->m_player, record };

	for (int i{}; i < record->m_lag; ++i) {
		pred_data.m_sim_time += g_csgo.m_globals->m_interval;
		SimulateMovement(pred_data);
	}

	record->m_extrapolated = true;
	record->m_pred_time = pred_data.m_sim_time;
	record->m_pred_flags = pred_data.m_flags;
	record->m_pred_origin = pred_data.m_origin;
	record->m_pred_velocity = pred_data.m_velocity;

	const vec3_t origin_delta = pred_data.m_origin - record->m_origin;


	for (int i{ 0 }; i <= 128; i++) {

		if (i < 0 || i > 128)
			break;

		record->m_extrap_bones[i][0][3] += origin_delta.x;
		record->m_extrap_bones[i][1][3] += origin_delta.y;
		record->m_extrap_bones[i][2][3] += origin_delta.z;
	}

	return true;
}

void Extrapolation::SimulateMovement(extrapolation_data_t& data) {

	if (!(data.m_flags & FL_ONGROUND)) {
		if (!g_csgo.sv_enablebunnyhopping->GetInt()) {
			const auto speed = data.m_velocity.length();

			const auto max_speed = data.m_player->m_flMaxspeed() * 1.1f;

			if (max_speed > 0.f && speed > max_speed)
				data.m_velocity *= (max_speed / speed);
		}

		if (data.m_was_in_air)
			data.m_velocity.z = g_csgo.sv_jump_impulse->GetFloat();
	}
	else
		data.m_velocity.z -= g_csgo.sv_gravity->GetFloat() * g_csgo.m_globals->m_interval;

	CGameTrace trace{};
	CTraceFilterWorldOnly trace_filter{};

	g_csgo.m_engine_trace->TraceRay(
		{
			data.m_origin,
			data.m_origin + data.m_velocity * g_csgo.m_globals->m_interval,
			data.m_obb_min, data.m_obb_max
		},
		CONTENTS_SOLID, &trace_filter, &trace
	);

	if (trace.m_fraction != 1.f) {
		for (int i{}; i < 2; ++i) {
			data.m_velocity -= trace.m_plane.m_normal * data.m_velocity.dot(trace.m_plane.m_normal);

			const auto adjust = data.m_velocity.dot(trace.m_plane.m_normal);
			if (adjust < 0.f)
				data.m_velocity -= trace.m_plane.m_normal * adjust;

			g_csgo.m_engine_trace->TraceRay(
				{
					trace.m_endpos,
					trace.m_endpos + (data.m_velocity * (g_csgo.m_globals->m_interval * (1.f - trace.m_fraction))),
					data.m_obb_min, data.m_obb_max
				},
				CONTENTS_SOLID, &trace_filter, &trace
			);

			if (trace.m_fraction == 1.f)
				break;
		}
	}

	data.m_origin = trace.m_endpos;

	g_csgo.m_engine_trace->TraceRay(
		{
			trace.m_endpos,
			{ trace.m_endpos.x, trace.m_endpos.y, trace.m_endpos.z - 2.f },
			data.m_obb_min, data.m_obb_max
		},
		CONTENTS_SOLID, &trace_filter, &trace
	);

	data.m_flags &= ~FL_ONGROUND;

	if (trace.m_fraction != 1.f && trace.m_plane.m_normal.z > 0.7f)
		data.m_flags |= FL_ONGROUND;
}