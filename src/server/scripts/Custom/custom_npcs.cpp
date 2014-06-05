/*
* Copyright (C) 2008-2014 TrinityCore <http://www.trinitycore.org/>
* Copyright (C) 2006-2009 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program. If not, see <http://www.gnu.org/licenses/>.
*/

/* ScriptData
SDName: custom_standard_of_conquest
SD%Complete: 100
SDComment: This NPC conquers a zone for the guild of the player that spawns it.
SDCategory: Custom
EndScriptData */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "NullSecMgr.h"
#include "GuildMgr.h"

#define MAX_STANDARD_HEALTH 100000
#define TIME_TO_REGEN 60000 // 10 minutes
#define TIME_TO_CLAIM 180000   // 30 minutes
#define TIME_TO_DESPAWN 60000      // 5 seconds

enum States
{
    STANDARD_STATE_NONE     = 1,
    STANDARD_STATE_CLAIMING = 2,
    STANDARD_STATE_CLAIMED  = 3
};

class npc_standard_of_conquest : public CreatureScript
{
public:
    npc_standard_of_conquest() : CreatureScript("npc_standard_of_conquest") { }

    struct npc_standard_of_conquestAI : ScriptedAI
    {
        npc_standard_of_conquestAI(Creature* creature) : ScriptedAI(creature)
        {
            SetCombatMovement(false);
            standardState = STANDARD_STATE_NONE;
            claimTimer = TIME_TO_CLAIM;
            despawnTimer = TIME_TO_DESPAWN;
            regenTimer = TIME_TO_REGEN;
            standardOwner = NULL;
            guildZoneId = sNullSecMgr->GetNullSecGuildZone(me->GetZoneId(), me->GetAreaId());
        }

        uint32 claimTimer;
        uint32 despawnTimer;
        uint32 regenTimer;
        uint32 standardState;
        uint32 guildZoneId;
        Guild* standardOwner;

        void InitializeAI() override
        {
            // This NPC can be only spawned in conquerable areas
            if (!guildZoneId)
                DeleteMe();

            me->SetMaxHealth(MAX_STANDARD_HEALTH / 4);
            me->SetHealth(MAX_STANDARD_HEALTH / 4);
            standardOwner = sNullSecMgr->GetGuildZoneAttacker(guildZoneId);
        }

        void Reset() override
        {
            claimTimer = TIME_TO_CLAIM;
            despawnTimer = TIME_TO_DESPAWN;
            regenTimer = TIME_TO_REGEN;
        }

        void EnterEvadeMode() override
        {
            if (standardState == STANDARD_STATE_CLAIMED)
                sNullSecMgr->SetGuildZoneUnderAttack(guildZoneId, false);
        }
        
        void DamageTaken(Unit* doneBy, uint32& damage) override
        {
            regenTimer = TIME_TO_REGEN;

            if (doneBy->GetTypeId() == TYPEID_PLAYER)
            {
                Guild* attacker = doneBy->ToPlayer()->GetGuild();
                if (attacker == standardOwner || !attacker)
                    damage = 0;
                else
                    sNullSecMgr->SetGuildZoneUnderAttack(guildZoneId, true, attacker);
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (standardState == STANDARD_STATE_NONE)
            {
                // NPC has just been summoned by a player to conquer a zone
                if (standardOwner)
                    standardState = STANDARD_STATE_CLAIMING;
                else
                {
                    // NPC has spawned in an already conquered zone, the owner is the zone controller.
                    standardOwner = sNullSecMgr->GetNullSecZoneOwner(guildZoneId);
                    if (standardOwner)
                    {
                        me->SetMaxHealth(MAX_STANDARD_HEALTH);
                        me->SetHealth(MAX_STANDARD_HEALTH);
                        standardState = STANDARD_STATE_CLAIMED;
                    }
                    // This case should not be possible anyway...
                    else
                        DeleteMe();
                }
            }

            if (standardState == STANDARD_STATE_CLAIMING)
            {
                if (claimTimer <= diff)
                {
                    me->SetMaxHealth(MAX_STANDARD_HEALTH);
                    me->SetHealth(MAX_STANDARD_HEALTH);
                    standardState = STANDARD_STATE_CLAIMED;
                    sNullSecMgr->SetNullSecZoneOwner(guildZoneId, standardOwner);
                }
                else
                    claimTimer -= diff;
            }

            if (UpdateVictim())
            {
                if (regenTimer <= diff)
                {
                    EnterEvadeMode();
                    regenTimer = TIME_TO_REGEN;
                }
                else
                    regenTimer -= diff;
            }

            if (!me->IsAlive())
                DeleteMe();
        }

        void DeleteMe()
        {
            if (standardState == STANDARD_STATE_CLAIMED && standardOwner)
                sNullSecMgr->RemoveGuildZoneOwner(guildZoneId);

            sNullSecMgr->SetGuildZoneUnderAttack(guildZoneId, false);
            me->CombatStop();
            me->DeleteFromDB();
            me->AddObjectToRemoveList();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_standard_of_conquestAI(creature);
    }
};

/*######
## npc_tour_guide
######*/

#define QUEST_ID_HORDE_TOUR    50001
#define QUEST_ID_ALLIANCE_TOUR 50002
#define NPC_THUUL_IMAGE        50001
#define NPC_BLA_IMAGE          0
#define SAY_FOLLOW_THE_IMAGE   "Sigue a mi imagen, colega!"

class npc_tour_guide : public CreatureScript
{
public:
    npc_tour_guide() : CreatureScript("npc_tour_guide") { }

    bool OnQuestAccept(Player* player, Creature* creature, Quest const* quest) override
    {
        uint32 npcId = 0;
        if (quest->GetQuestId() == QUEST_ID_HORDE_TOUR)
            npcId = NPC_THUUL_IMAGE;
        else if (quest->GetQuestId() == QUEST_ID_ALLIANCE_TOUR)
            npcId = NPC_BLA_IMAGE;

        if (!npcId)
            return true;

        // Spawn the NPC
        Map* map = creature->GetMap();
        Creature* npcImage = new Creature();
        uint32 tempGuid = sObjectMgr->GenerateLowGuid(HIGHGUID_UNIT);
        if (!npcImage->Create(tempGuid, map, player->GetPhaseMaskForSpawn(), npcId, creature->GetPositionX(), creature->GetPositionY(), creature->GetPositionZ(), creature->GetOrientation()))
        {
            delete npcImage;
            return false;
        }
        // Add to map
        map->AddToMap(npcImage);

        creature->MonsterSay(SAY_FOLLOW_THE_IMAGE, 0, player);
        uint32 ficticialDamage = 0;
        npcImage->GetAI()->DamageTaken(player, ficticialDamage);
        return true;
    }
};

/*######
## npc_thuul_image
######*/

enum ThuulTexts
{
    SAY_MORE_JOUNG            = 0,
    SAY_PVP_GENERAL           = 1,
    SAY_PVP_HIGH_SEC          = 2,
    SAY_PVP_LOW_SEC           = 3,
    SAY_PVP_NULL_SEC          = 4,
    SAY_PVP_CONQUERABLE_ZONES = 5,
    SAY_FOLLOW_1              = 6,
    SAY_BANKER                = 7,
    SAY_FOLLOW_2              = 8,
    SAY_PROFESSIONS_1         = 9,
    SAY_PROFESSIONS_2         = 10,
    SAY_PROFESSIONS_3         = 11,
    SAY_NO_SOULBOUNDS         = 12,
    SAY_FINISHING             = 13,
    SAY_GOOD_LUCK             = 14,
    SAY_TOO_FAR               = 15
};

enum ThuulPhases
{
    PHASE_MOVING_PVP_1       = 0,
    PHASE_MOVING_PVP_2       = 1,
    PHASE_PVP                = 2,
    PHASE_MOVING_BANK_1      = 3,
    PHASE_MOVING_BANK_2      = 4,
    PHASE_BANK               = 5,
    PHASE_MOVING_PROFESSIONS = 6,
    PHASE_PROFESSIONS        = 7,
    PHASE_MOVING_END_1       = 8,
    PHASE_MOVING_END_2       = 9,
    PHASE_END                = 10
};

enum ThuulImageWP
{
    WP_HALL_OF_LEGENDS_1 = 0,
    WP_HALL_OF_LEGENDS_2 = 1,
    WP_BANK_1            = 2,
    WP_BANK_2            = 3,
    WP_PROFESSIONS       = 4,
    WP_END_1             = 5,
    WP_END_2             = 6
};

const Position ThuulWP[7] = {
    { 1548.24f, -4209.17f, 43.21f, 0.10059f },
    { 1670.26f, -4225.17f, 56.39f, 3.72601f },
    { 1696.17f, -4276.21f, 33.58f, 4.71088f },
    { 1625.45f, -4374.25f, 12.00f, 3.87680f },
    { 1729.80f, -4480.47f, 31.90f, 5.08867f },
    { 1774.20f, -4535.73f, 24.79f, 6.25558f },
    { 1860.29f, -4513.38f, 23.87f, 3.64982f }
};

const float ThuulTeleportPos[4] = { -610.07f, -4253.52f, 39.04f, 3.28122f };

class npc_thuul_image : public CreatureScript
{
public:
    npc_thuul_image() : CreatureScript("npc_thuul_image") { }

    struct npc_thuul_imageAI : ScriptedAI
    {
        npc_thuul_imageAI(Creature* creature) : ScriptedAI(creature)
        {
            phase = PHASE_MOVING_PVP_1;
            nextSay = SAY_MORE_JOUNG;
            teleportTimer = 0;
            sayTimer = 600000;
            forceDespawnTimer = 900000; // 15 minutes
            player = NULL;
        }

        uint8 phase;
        uint8 nextSay;
        Player* player;
        uint32 sayTimer;  // Controls time between talks
        uint32 teleportTimer;
        uint32 forceDespawnTimer;

        void EnterEvadeMode() override
        {
            DeleteMe();
        }

        void DamageTaken(Unit* doneBy, uint32& damage) override
        {
            if (doneBy->GetTypeId() != TYPEID_PLAYER)
            {
                damage = 0;
                return;
            }

            if (player)
            {
                damage = 0;
                return;
            }

            player = doneBy->ToPlayer();
            // Start first movement
            me->GetMotionMaster()->MovePoint(1, ThuulWP[WP_HALL_OF_LEGENDS_1], true);
            nextSay = SAY_MORE_JOUNG;
            sayTimer = 3000;
        }

        void UpdateAI(uint32 diff) override
        {
            // Emergency check, for example if player disconnects,
            // the NPC will despawn in several minutes.
            if (forceDespawnTimer <= diff)
                DeleteMe();
            else
                forceDespawnTimer -= diff;

            // Player has disconnected / is not set yet?
            if (!player)
                return;

            // If the player is too far from the NPC, set the mission as failed.
            if (player->GetDistance(me->GetPosition()) > 50.0f)
            {
                Talk(SAY_TOO_FAR, player);
                player->SetQuestStatus(QUEST_ID_HORDE_TOUR, QUEST_STATUS_FAILED);
                DeleteMe();
            }

            // Control phases
            switch (phase)
            {
                case PHASE_MOVING_PVP_1:
                    if (me->GetDistance(ThuulWP[WP_HALL_OF_LEGENDS_1]) < 1.0f)
                    {
                        me->GetMotionMaster()->MovePoint(1, ThuulWP[WP_HALL_OF_LEGENDS_2]);
                        phase = PHASE_MOVING_PVP_2;
                    }
                    break;
                case PHASE_MOVING_PVP_2:
                    if (me->GetDistance(ThuulWP[WP_HALL_OF_LEGENDS_2]) < 1.0f)
                    {
                        sayTimer = 2000;
                        phase = PHASE_PVP;
                    }
                    break;
                case PHASE_MOVING_BANK_1:
                    if (me->GetDistance(ThuulWP[WP_BANK_1]) < 1.0f)
                    {
                        me->GetMotionMaster()->MovePoint(1, ThuulWP[WP_BANK_2]);
                        phase = PHASE_MOVING_BANK_2;
                    }
                    break;
                case PHASE_MOVING_BANK_2:
                    if (me->GetDistance(ThuulWP[WP_BANK_2]) < 1.0f)
                    {
                        sayTimer = 2000;
                        phase = PHASE_BANK;
                    }
                    break;
                case PHASE_MOVING_PROFESSIONS:
                    if (me->GetDistance(ThuulWP[WP_PROFESSIONS]) < 1.0f)
                    {
                        sayTimer = 2000;
                        me->GetMotionMaster()->Clear();
                        me->SetWalk(true);
                        me->GetMotionMaster()->MovePoint(1, ThuulWP[WP_END_1], true);
                        phase = PHASE_MOVING_END_1;
                    }
                    break;
                case PHASE_MOVING_END_1:
                    if (me->GetDistance(ThuulWP[WP_END_1]) < 1.0f)
                    {
                        me->GetMotionMaster()->MovePoint(1, ThuulWP[WP_END_2], true);
                        phase = PHASE_MOVING_END_2;
                    }
                    break;
                case PHASE_MOVING_END_2:
                    if (me->GetDistance(ThuulWP[WP_END_2]) < 1.0f)
                    {
                        sayTimer = 2000;
                        teleportTimer = 60000;
                        phase = PHASE_END;
                    }
                    break;
                default:
                    break;
            }

            // Control say timer (also phases when the chat ends)
            if (sayTimer <= diff && nextSay <= SAY_GOOD_LUCK)
            {
                Talk(nextSay, player);
                switch (nextSay)
                {
                    case SAY_MORE_JOUNG:
                        sayTimer = 600000;
                        break;
                    case SAY_PVP_GENERAL:
                        me->SetFacingToObject(player);
                        sayTimer = 7000;
                        break;
                    case SAY_PVP_HIGH_SEC:
                        sayTimer = 10000;
                        break;
                    case SAY_PVP_LOW_SEC:
                        sayTimer = 13000;
                        break;
                    case SAY_PVP_NULL_SEC:
                        sayTimer = 11000;
                        break;
                    case SAY_PVP_CONQUERABLE_ZONES:
                        sayTimer = 10000;
                        break;
                    case SAY_FOLLOW_1:
                        sayTimer = 600000;
                        me->GetMotionMaster()->MovePoint(1, ThuulWP[WP_BANK_1], true);
                        phase = PHASE_MOVING_BANK_1;
                        break;
                    case SAY_BANKER:
                        me->SetFacingToObject(player);
                        sayTimer = 10000;
                        break;
                    case SAY_FOLLOW_2:
                        sayTimer = 600000;
                        me->GetMotionMaster()->MovePoint(1, ThuulWP[WP_PROFESSIONS], true);
                        phase = PHASE_MOVING_PROFESSIONS;
                        break;
                    case SAY_PROFESSIONS_1:
                        sayTimer = 12000;
                        break;
                    case SAY_PROFESSIONS_2:
                        sayTimer = 12000;
                        break;
                    case SAY_PROFESSIONS_3:
                        sayTimer = 10000;
                        break;
                    case SAY_NO_SOULBOUNDS:
                        sayTimer = 600000;
                        break;
                    case SAY_FINISHING:
                        me->SetFacingToObject(player);
                        sayTimer = 9000;
                        break;
                    case SAY_GOOD_LUCK:
                        teleportTimer = 3000;
                        break;
                    default:
                        break;
                }
                ++nextSay;
            }
            else
                sayTimer -= diff;

            // Control teleport
            if (phase == PHASE_END)
            {
                if (teleportTimer <= diff)
                {
                    player->TeleportTo(1, ThuulTeleportPos[0], ThuulTeleportPos[1], ThuulTeleportPos[2], ThuulTeleportPos[3]);
                    DeleteMe();
                }
                else
                    teleportTimer -= diff;
            }
        }

        void DeleteMe()
        {
            me->CombatStop();
            me->CleanupsBeforeDelete();
            me->AddObjectToRemoveList();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_thuul_imageAI(creature);
    }
};

void AddSC_custom_npcs()
{
    new npc_standard_of_conquest();
    new npc_tour_guide();
    new npc_thuul_image();
}
