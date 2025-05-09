#include "includes.h"

Resolver g_resolver{};

#pragma optimize("", off)

// Estrutura para ângulos adaptativos (usada no Anti-Freestand)
struct AdaptiveAngle {
    float m_yaw;
    float m_dist;

    AdaptiveAngle(float yaw, float dist = 0.f) :
        m_yaw(yaw),
        m_dist(dist) {
    }
};

// Função principal para Anti-Freestand
float Resolver::AntiFreestand(Player* player, LagRecord* record, vec3_t start, vec3_t end, bool include_base, float base_yaw, float delta) {
    std::vector<AdaptiveAngle> angles{};
    angles.emplace_back(base_yaw + delta);
    angles.emplace_back(base_yaw - delta);

    if (include_base)
        angles.emplace_back(base_yaw);

    vec3_t shoot_pos = end;
    bool valid = false;

    for (auto& angle : angles) {
        vec3_t end_pos{
            shoot_pos.x + std::cos(math::deg_to_rad(angle.m_yaw)) * 32.f,
            shoot_pos.y + std::sin(math::deg_to_rad(angle.m_yaw)) * 32.f,
            shoot_pos.z
        };

        vec3_t dir = end_pos - start;
        float len = dir.normalize();

        if (len <= 0.f)
            continue;

        for (float i = 0.f; i < len; i += 4.f) {
            vec3_t point = start + (dir * i);
            int contents = g_csgo.m_engine_trace->GetPointContents(point, MASK_SHOT_HULL);

            if (!(contents & MASK_SHOT_HULL))
                continue;

            float mult = 1.f;

            if (i > (len * 0.5f))
                mult = 1.25f;

            if (i > (len * 0.75f))
                mult = 1.25f;

            if (i > (len * 0.9f))
                mult = 2.f;

            angle.m_dist += (4.f * mult);
            valid = true;
        }
    }

    if (!valid)
        return base_yaw;

    std::sort(angles.begin(), angles.end(),
        [](const AdaptiveAngle& a, const AdaptiveAngle& b) {
            return a.m_dist > b.m_dist;
        });

    return angles.front().m_yaw;
}

// Obtém o ângulo oposto ao jogador
float Resolver::GetAwayAngle(LagRecord* record) {
    vec3_t enemy_pos = record->m_pred_origin;
    vec3_t local_pos = g_cl.m_shoot_pos;

    ang_t away;
    math::VectorAngles(local_pos - enemy_pos, away);
    return away.y;
}

// Verifica se o LBY é confiável
bool Resolver::IsLBYReliable(LagRecord* record) {
    float delta = std::abs(math::AngleDiff(record->m_eye_angles.y, record->m_body));
    return (delta < 35.f || delta > 145.f);
}

// Resolve ângulos principais
void Resolver::ResolveAngles(Player* player, LagRecord* record) {
    if (!player || !record)
        return;

    // Casos especiais (escadas, granadas, etc.)
    if (player->m_MoveType() == MOVETYPE_LADDER || player->m_MoveType() == MOVETYPE_NOCLIP) {
        record->m_resolver_mode = "SPECIAL";
        return;
    }

    AimPlayer* data = &g_aimbot.m_players[player->index() - 1];
    float speed = record->m_velocity.length_2d();

    // Modo andando (LBY confiável)
    if (speed > 0.1f && speed < 130.f && (record->m_flags & FL_ONGROUND)) {
        record->m_eye_angles.y = record->m_body;
        record->m_resolver_mode = "WALK";
        return;
    }

    // Modo parado (Anti-Freestand + LBY)
    if (speed <= 0.1f && (record->m_flags & FL_ONGROUND)) {
        float away = GetAwayAngle(record);
        vec3_t enemy_head = player->GetHitboxPos(HITBOX_HEAD);
        vec3_t local_eye = g_cl.m_local->GetShootPosition();

        float left = away + 90.f;
        float right = away - 90.f;

        // Anti-freestand avançado
        float final_yaw = AntiFreestand(player, record, local_eye, enemy_head, true, away, 45.f);
        record->m_eye_angles.y = final_yaw;
        record->m_resolver_mode = "STAND";

        // Se LBY for confiável, mistura com o resultado
        if (IsLBYReliable(record)) {
            record->m_eye_angles.y = (final_yaw + record->m_body) * 0.5f;
            record->m_resolver_mode = "STAND-LBY";
        }
        return;
    }

    // Modo aéreo (simples)
    if (!(record->m_flags & FL_ONGROUND)) {
        float away = GetAwayAngle(record);
        record->m_eye_angles.y = away + 180.f;
        record->m_resolver_mode = "AIR";
        return;
    }

    // Fallback
    record->m_eye_angles.y = GetAwayAngle(record) + 180.f;
    record->m_resolver_mode = "FALLBACK";
}

// Resolver override (manual)
void Resolver::ResolveOverride(AimPlayer* data, LagRecord* record, Player* player) {
    ang_t viewangles;
    g_csgo.m_engine->GetViewAngles(viewangles);

    const float at_target_yaw = math::CalcAngle(g_cl.m_local->m_vecOrigin(), player->m_vecOrigin()).y;
    const float dist = math::NormalizedAngle(viewangles.y - at_target_yaw);

    if (std::abs(dist) <= 1.f) {
        record->m_eye_angles.y = at_target_yaw;
        record->m_resolver_mode = "OVERRIDE-BACK";
    }
    else if (dist > 0) {
        record->m_eye_angles.y = at_target_yaw + 90.f;
        record->m_resolver_mode = "OVERRIDE-RIGHT";
    }
    else {
        record->m_eye_angles.y = at_target_yaw - 90.f;
        record->m_resolver_mode = "OVERRIDE-LEFT";
    }
}

#pragma optimize("", on)