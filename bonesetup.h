#pragma once

class BoneHandler {
public:
    bool m_running;
    bool m_updating_anims;


public:

    bool SetupBones( Player* entity, BoneArray* pBoneMatrix, float time, bool disable_interp = false );
    bool SetupBonesOnetap( Player* m_pPlayer, matrix3x4_t* m_pBones, bool m_bInterpolate );
    bool SetupBonesRebuild(Player* entity, BoneArray* pBoneMatrix, int nBoneCount, int boneMask, float time, int flags);
};

extern BoneHandler g_bone_handler;

enum BoneSetupFlags {
    None = 0,
    UseInterpolatedOrigin = (1 << 0),
    UseCustomOutput = (1 << 1),
    ForceInvalidateBoneCache = (1 << 2),
    AttachmentHelper = (1 << 3),
};


class rebuilt_setupbones {
public:
    bool ishltv;
public:

    bool setup_entity_bones(Player* player, matrix3x4_t* bone, int max_bones, int mask);

};

extern rebuilt_setupbones m_bone_handler;