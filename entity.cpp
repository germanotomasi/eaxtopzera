#include "includes.h"
#include "pattern_scan.hpp"

struct mstudioposeparamdesc_t1 {
	int sznameindex;
	inline char* const pszName(void) const { return ((char*)this) + sznameindex; }
	int flags;   // ???? ( volvo, really? )
	float start; // starting value
	float end;   // ending value
	float loop;  // looping range, 0 for no looping, 360 for rotations, etc.
};
mstudioposeparamdesc_t1* pPoseParameter(CStudioHdr* hdr, int index) {
	static auto m_pPoseParameter = memory_scan::search(XOR("client.dll"), XOR("55 8B EC 8B 45 08 57 8B F9 8B 4F 04 85 C9 75 15 8B"));
	using poseParametorFN = mstudioposeparamdesc_t1 * (__thiscall*)(CStudioHdr*, int);
	poseParametorFN p_pose_parameter = m_pPoseParameter.get<poseParametorFN>();
	return p_pose_parameter(hdr, index);
}

void animstate_pose_param_cache_t::SetValue(Player* player, float flValue) {
	auto hdr = player->m_studioHdr();
	if (hdr) {
		auto pose_param = pPoseParameter(hdr, index);
		if (!pose_param)
			return;

		auto PoseParam = *pose_param;

		if (PoseParam.loop) {
			float wrap = (PoseParam.start + PoseParam.end) / 2.0f + PoseParam.loop / 2.0f;
			float shift = PoseParam.loop - wrap;

			flValue = flValue - PoseParam.loop * std::floorf((flValue + shift) / PoseParam.loop);
		}

		auto ctlValue = (flValue - PoseParam.start) / (PoseParam.end - PoseParam.start);
		player->m_flPoseParameter()[index] = ctlValue;
	}
}
