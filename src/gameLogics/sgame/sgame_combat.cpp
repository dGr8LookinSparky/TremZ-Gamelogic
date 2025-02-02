////////////////////////////////////////////////////////////////////////////////////////
// Copyright(C) 1999 - 2005 Id Software, Inc.
// Copyright(C) 2000 - 2006 Tim Angus
// Copyright(C) 2011 - 2021 Dusan Jocic <dusanjocic@msn.com>
//
// This file is part of OpenWolf.
//
// OpenWolf is free software; you can redistribute it
// and / or modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 3 of the License,
// or (at your option) any later version.
//
// OpenWolf is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenWolf; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110 - 1301  USA
//
// -------------------------------------------------------------------------------------
// File name:   sgame_combat.cpp
// Created:
// Compilers:   Microsoft (R) C/C++ Optimizing Compiler Version 19.26.28806 for x64,
//              gcc (Ubuntu 9.3.0-10ubuntu2) 9.3.0
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#include <sgame/sgame_precompiled.hpp>

/*
===============
idSGameMem::idSGameMem
===============
*/
idSGameCombat::idSGameCombat(void) {
}

/*
===============
idSGameCombat::~idSGameCombat
===============
*/
idSGameCombat::~idSGameCombat(void) {
}

/*
============
idSGameCombat::AddScore

Adds score to the client
============
*/
void idSGameCombat::AddScore(gentity_t *ent, sint score) {
    if(!ent->client) {
        return;
    }

    // no scoring during pre-match warmup
    if(level.warmupTime) {
        return;
    }

    // make alien and human scores equivalent
    if(ent->client->pers.teamSelection == TEAM_ALIENS) {
        score = rint((float64) score / 2.0);
    }

    // scale values down to fit the scoreboard better
    score = rint((float64) score / 10.0);

    ent->client->ps.persistant[ PERS_SCORE ] += score;
    idSGameMain::CalculateRanks();
}

/*
==================
idSGameCombat::LookAtKiller
==================
*/
void idSGameCombat::LookAtKiller(gentity_t *self, gentity_t *inflictor,
                                 gentity_t *attacker) {

    if(attacker && attacker != self) {
        self->client->ps.stats[ STAT_VIEWLOCK ] = attacker - g_entities;
    } else if(inflictor && inflictor != self) {
        self->client->ps.stats[ STAT_VIEWLOCK ] = inflictor - g_entities;
    } else {
        self->client->ps.stats[ STAT_VIEWLOCK ] = self - g_entities;
    }
}

// these are just for logging, the client prints its own messages
valueType *modNames[ ] = {
    "MOD_UNKNOWN",
    "MOD_SHOTGUN",
    "MOD_BLASTER",
    "MOD_PAINSAW",
    "MOD_MACHINEGUN",
    "MOD_CHAINGUN",
    "MOD_PRIFLE",
    "MOD_MDRIVER",
    "MOD_LASGUN",
    "MOD_LCANNON",
    "MOD_LCANNON_SPLASH",
    "MOD_FLAMER",
    "MOD_FLAMER_SPLASH",
    "MOD_GRENADE",
    "MOD_WATER",
    "MOD_SLIME",
    "MOD_LAVA",
    "MOD_CRUSH",
    "MOD_TELEFRAG",
    "MOD_FALLING",
    "MOD_SUICIDE",
    "MOD_DECONSTRUCT",
    "MOD_NOCREEP",
    "MOD_TARGET_LASER",
    "MOD_TRIGGER_HURT",

    "MOD_ABUILDER_CLAW",
    "MOD_LEVEL0_BITE",
    "MOD_LEVEL1_CLAW",
    "MOD_LEVEL1_PCLOUD",
    "MOD_LEVEL3_CLAW",
    "MOD_LEVEL3_POUNCE",
    "MOD_LEVEL3_BOUNCEBALL",
    "MOD_LEVEL2_CLAW",
    "MOD_LEVEL2_ZAP",
    "MOD_LEVEL4_CLAW",
    "MOD_LEVEL4_TRAMPLE",
    "MOD_LEVEL4_CRUSH",

    "MOD_SLOWBLOB",
    "MOD_POISON",
    "MOD_SWARM",

    "MOD_HSPAWN",
    "MOD_TESLAGEN",
    "MOD_MGTURRET",
    "MOD_REACTOR",

    "MOD_ASPAWN",
    "MOD_ATUBE",
    "MOD_OVERMIND"
};

/*
==================
idSGameCombat::RewardAttackers

Function to distribute rewards to entities that killed this one.
Returns the total damage dealt.
==================
*/
float32 idSGameCombat::RewardAttackers(gentity_t *self) {
    float32 value, totalDamage = 0;
    sint team, i, maxHealth = 0;

    // Total up all the damage done by every client
    for(i = 0; i < MAX_CLIENTS; i++) {
        totalDamage += (float32)self->credits[ i ];
    }

    if(totalDamage <= 0.0f) {
        return 0.f;
    }

    // Only give credits for killing players and buildables
    if(self->client) {
        value = bggame->GetValueOfPlayer(&self->client->ps);
        team = self->client->pers.teamSelection;
        maxHealth = self->client->ps.stats[ STAT_MAX_HEALTH ];
    } else if(self->s.eType == ET_BUILDABLE) {
        value = bggame->Buildable((buildable_t)self->s.modelindex)->value;

        // only give partial credits for a buildable not yet completed
        if(!self->spawned)
            value *= (float32)(level.time - self->buildTime) /
                     bggame->Buildable((buildable_t)self->s.modelindex)->buildTime;

        team = self->buildableTeam;
        maxHealth = bggame->Buildable((buildable_t)self->s.modelindex)->health;
    } else {
        return totalDamage;
    }

    // Give credits and empty the array
    for(i = 0; i < MAX_CLIENTS; i++) {
        gentity_t *player = g_entities + i;
        schar16 num = value * self->credits[ i ] / totalDamage;
        sint stageValue = num;

        if(totalDamage < maxHealth) {
            stageValue *= totalDamage / maxHealth;
        }

        if(!player->client || !self->credits[ i ] ||
                player->client->ps.stats[ STAT_TEAM ] == team) {
            continue;
        }

        AddScore(player, num);

        // killing buildables earns score, but not credits
        if(self->s.eType != ET_BUILDABLE) {
            idSGameClient::AddCreditToClient(player->client, num, true);

            // add to stage counters
            if(player->client->ps.stats[STAT_TEAM] == TEAM_ALIENS) {
                trap_Cvar_Set("g_alienCredits", va("%d",
                                                   g_alienCredits.integer + stageValue));
            } else if(player->client->ps.stats[STAT_TEAM] == TEAM_HUMANS) {
                trap_Cvar_Set("g_humanCredits", va("%d",
                                                   g_humanCredits.integer + stageValue));
            }
        }

        self->credits[ i ] = 0;
    }

    return totalDamage;
}

/*
==================
idSGameCombat::player_die
==================
*/
void idSGameCombat::player_die(gentity_t *self, gentity_t *inflictor,
                               gentity_t *attacker, sint damage, sint meansOfDeath) {
    gentity_t *ent;
    sint       anim;
    sint       killer;
    sint       i, idk;
    valueType      *killerName, *obit;
    float32     totalDamage = 0.0f;
    bool      tk = false;

    if(self->client->ps.pm_type == PM_DEAD) {
        return;
    }

    if(level.intermissiontime) {
        return;
    }

    self->client->ps.pm_type = PM_DEAD;
    self->suicideTime = 0;

    if(attacker) {
        killer = attacker->s.number;

        if(attacker->client) {
            killerName = attacker->client->pers.netname;
            tk = (attacker != self && attacker->client->ps.stats[STAT_TEAM]
                  == self->client->ps.stats[STAT_TEAM]);
        } else {
            killerName = "<non-client>";
        }
    } else {
        killer = ENTITYNUM_WORLD;
        killerName = "<world>";
    }

    if(killer < 0 || killer >= MAX_CLIENTS) {
        killer = ENTITYNUM_WORLD;
        killerName = "<world>";
    }

    if(meansOfDeath < 0 ||
            meansOfDeath >= sizeof(modNames) / sizeof(modNames[0])) {
        obit = "<bad obituary>";
    } else {
        obit = modNames[ meansOfDeath ];
    }

    idSGameMain::LogPrintf("Kill: %i %i %i: %s^7 killed %s^7 by %s\n", killer,
                           self->s.number, meansOfDeath, killerName, self->client->pers.netname,
                           obit);

    // close any menus the client has open
    idSGameUtils::CloseMenus(self->client->ps.clientNum);

    // deactivate all upgrades
    for(i = UP_NONE + 1; i < UP_NUM_UPGRADES; i++) {
        bggame->DeactivateUpgrade(i, self->client->ps.stats);
    }

    // broadcast the death event to everyone
    ent = idSGameUtils::TempEntity(self->r.currentOrigin, EV_OBITUARY);
    ent->s.eventParm = meansOfDeath;
    ent->s.otherEntityNum = self->s.number;
    ent->s.otherEntityNum2 = killer;
    ent->r.svFlags = SVF_BROADCAST; // send to everyone

    self->enemy = attacker;

    self->client->ps.persistant[ PERS_KILLED ]++;

    if(attacker && attacker->client) {
        attacker->client->lastkilled_client = self->s.number;

        if(attacker == self || idSGameTeam::OnSameTeam(self, attacker)) {

            //punish team kills and suicides
            if(attacker->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS) {
                idSGameClient::AddCreditToClient(attacker->client,
                                                 -ALIEN_TK_SUICIDE_PENALTY, true);
                AddScore(attacker, -ALIEN_TK_SUICIDE_PENALTY);
            } else if(attacker->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS) {
                idSGameClient::AddCreditToClient(attacker->client,
                                                 -HUMAN_TK_SUICIDE_PENALTY, true);
                AddScore(attacker, -HUMAN_TK_SUICIDE_PENALTY);
            }
        } else {
            attacker->client->lastKillTime = level.time;
        }
    } else if(attacker->s.eType != ET_BUILDABLE) {
        if(self->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS) {
            AddScore(self, -ALIEN_TK_SUICIDE_PENALTY);
        } else if(self->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS) {
            AddScore(self, -HUMAN_TK_SUICIDE_PENALTY);
        }
    }

    // give credits for killing this player
    totalDamage = RewardAttackers(self);

    idSGameCmds::ScoreboardMessage(self);      // show scores

    // send updated scores to any clients that are following this one,
    // or they would get stale scoreboards
    for(i = 0 ; i < level.maxclients ; i++) {
        gclient_t *client;

        client = &level.clients[ i ];

        if(client->pers.connected != CON_CONNECTED) {
            continue;
        }

        if(client->sess.spectatorState == SPECTATOR_NOT) {
            continue;
        }

        if(client->sess.spectatorClient == self->s.number) {
            idSGameCmds::ScoreboardMessage(g_entities + i);
        }
    }

    VectorCopy(self->s.origin, self->client->pers.lastDeathLocation);

    self->takedamage = false; // can still be gibbed

    self->s.weapon = WP_NONE;
    self->r.contents = CONTENTS_CORPSE;

    self->s.angles[ PITCH ] = 0;
    self->s.angles[ ROLL ] = 0;
    self->s.angles[ YAW ] = self->s.apos.trBase[ YAW ];
    LookAtKiller(self, inflictor, attacker);

    VectorCopy(self->s.angles, self->client->ps.viewangles);

    self->s.loopSound = 0;

    self->r.maxs[ 2 ] = -8;

    // don't allow respawn until the death anim is done
    // g_forcerespawn may force spawning at some later time
    self->client->respawnTime = level.time + 1700;

    // clear misc
    ::memset(self->client->ps.misc, 0, sizeof(self->client->ps.misc));

    {
        // normal death
        static sint i;

        if(!(self->client->ps.persistant[ PERS_STATE ] & PS_NONSEGMODEL)) {
            switch(i) {
                case 0:
                    anim = BOTH_DEATH1;
                    break;

                case 1:
                    anim = BOTH_DEATH2;
                    break;

                case 2:
                default:
                    anim = BOTH_DEATH3;
                    break;
            }
        } else {
            switch(i) {
                case 0:
                    anim = NSPA_DEATH1;
                    break;

                case 1:
                    anim = NSPA_DEATH2;
                    break;

                case 2:
                default:
                    anim = NSPA_DEATH3;
                    break;
            }
        }

        self->client->ps.legsAnim =
            ((self->client->ps.legsAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT) | anim;

        if(!(self->client->ps.persistant[ PERS_STATE ] & PS_NONSEGMODEL)) {
            self->client->ps.torsoAnim =
                ((self->client->ps.torsoAnim & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT) | anim;
        }

        // use own entityid if killed by non-client to prevent uint8_t overflow
        idSGameUtils::AddEvent(self, EV_DEATH1 + i,
                               (killer < MAX_CLIENTS) ? killer : self - g_entities);

        // globally cycle through the different death animations
        i = (i + 1) % 3;
    }

    trap_LinkEntity(self);
}

/*
===============
idSGameCombat::ParseDmgScript
===============
*/
sint idSGameCombat::ParseDmgScript(damageRegion_t *regions,
                                   valueType *buf) {
    valueType  *token;
    float32 angleSpan, heightSpan;
    sint   count;

    for(count = 0; ; count++) {
        token = COM_Parse(&buf);

        if(!token[0]) {
            break;
        }

        if(strcmp(token, "{")) {
            idSGameMain::Printf("Missing { in damage region file\n");
            break;
        }

        if(count >= MAX_DAMAGE_REGIONS) {
            idSGameMain::Printf("Max damage regions exceeded in damage region file\n");
            break;
        }

        // defaults
        regions[ count ].name[ 0 ] = 0;
        regions[ count ].minHeight = 0.0;
        regions[ count ].maxHeight = 1.0;
        regions[ count ].minAngle = 0;
        regions[ count ].maxAngle = 360;
        regions[ count ].modifier = 1.0;
        regions[ count ].crouch = false;

        while(1) {
            token = COM_ParseExt(&buf, true);

            if(!token[0]) {
                idSGameMain::Printf("Unexpected end of damage region file\n");
                break;
            }

            if(!Q_stricmp(token, "}")) {
                break;
            } else if(!strcmp(token, "name")) {
                token = COM_ParseExt(&buf, false);

                if(token[ 0 ])
                    Q_strncpyz(regions[ count ].name, token,
                               sizeof(regions[ count ].name));
            } else if(!strcmp(token, "minHeight")) {
                token = COM_ParseExt(&buf, false);

                if(!token[0]) {
                    strcpy(token, "0");
                }

                regions[ count ].minHeight = atof(token);
            } else if(!strcmp(token, "maxHeight")) {
                token = COM_ParseExt(&buf, false);

                if(!token[0]) {
                    strcpy(token, "100");
                }

                regions[ count ].maxHeight = atof(token);
            } else if(!strcmp(token, "minAngle")) {
                token = COM_ParseExt(&buf, false);

                if(!token[0]) {
                    strcpy(token, "0");
                }

                regions[ count ].minAngle = atoi(token);
            } else if(!strcmp(token, "maxAngle")) {
                token = COM_ParseExt(&buf, false);

                if(!token[0]) {
                    strcpy(token, "360");
                }

                regions[ count ].maxAngle = atoi(token);
            } else if(!strcmp(token, "modifier")) {
                token = COM_ParseExt(&buf, false);

                if(!token[0]) {
                    strcpy(token, "1.0");
                }

                regions[ count ].modifier = atof(token);
            } else if(!strcmp(token, "crouch")) {
                regions[ count ].crouch = true;
            }
        }

        // Angle portion covered
        angleSpan = regions[ count ].maxAngle - regions[ count ].minAngle;

        if(angleSpan < 0.f) {
            angleSpan += 360.f;
        }

        angleSpan /= 360.f;

        // Height portion covered
        heightSpan = regions[ count ].maxHeight - regions[ count ].minHeight;

        if(heightSpan < 0.f) {
            heightSpan = -heightSpan;
        }

        if(heightSpan > 1.f) {
            heightSpan = 1.f;
        }

        regions[ count ].area = angleSpan * heightSpan;

        if(!regions[ count ].area) {
            regions[ count ].area = 0.00001f;
        }
    }

    return count;
}

/*
============
GetRegionDamageModifier
============
*/
float32 idSGameCombat::GetRegionDamageModifier(gentity_t *targ,
        sint _class, sint piece) {
    damageRegion_t *regions, *overlap;
    float32 modifier = 0.f, areaSum = 0.f;
    sint j, i;
    bool crouch;

    crouch = targ->client->ps.pm_flags & PMF_DUCKED;
    overlap = &g_damageRegions[ _class ][ piece ];

    if(g_debugDamage.integer > 2)
        idSGameMain::Printf("GetRegionDamageModifier():\n"
                            ".   bodyRegion = [%d %d %f %f] (%s)\n"
                            ".   modifier = %f\n",
                            overlap->minAngle, overlap->maxAngle,
                            overlap->minHeight, overlap->maxHeight,
                            overlap->name, overlap->modifier);

    // Find the armour layer modifier, assuming that none of the armour regions
    // overlap and that any areas that are not covered have a modifier of 1.0
    for(j = UP_NONE + 1; j < UP_NUM_UPGRADES; j++) {
        if(!bggame->InventoryContainsUpgrade(j, targ->client->ps.stats) ||
                !g_numArmourRegions[ j ]) {
            continue;
        }

        regions = g_armourRegions[ j ];

        for(i = 0; i < g_numArmourRegions[ j ]; i++) {
            float32 overlapMaxA, regionMinA, regionMaxA, angleSpan, heightSpan, area;

            if(regions[ i ].crouch != crouch) {
                continue;
            }

            // Convert overlap angle to 0 to max
            overlapMaxA = overlap->maxAngle - overlap->minAngle;

            if(overlapMaxA < 0.f) {
                overlapMaxA += 360.f;
            }

            // Convert region angles to match overlap
            regionMinA = regions[ i ].minAngle - overlap->minAngle;

            if(regionMinA < 0.f) {
                regionMinA += 360.f;
            }

            regionMaxA = regions[ i ].maxAngle - overlap->minAngle;

            if(regionMaxA < 0.f) {
                regionMaxA += 360.f;
            }

            // Overlapping Angle portion
            if(regionMinA <= regionMaxA) {
                angleSpan = 0.f;

                if(regionMinA < overlapMaxA) {
                    if(regionMaxA > overlapMaxA) {
                        regionMaxA = overlapMaxA;
                    }

                    angleSpan = regionMaxA - regionMinA;
                }
            } else {
                if(regionMaxA > overlapMaxA) {
                    regionMaxA = overlapMaxA;
                }

                angleSpan = regionMaxA;

                if(regionMinA < overlapMaxA) {
                    angleSpan += overlapMaxA - regionMinA;
                }
            }

            angleSpan /= 360.f;

            // Overlapping height portion
            heightSpan = MIN(overlap->maxHeight, regions[ i ].maxHeight) -
                         MAX(overlap->minHeight, regions[ i ].minHeight);

            if(heightSpan < 0.f) {
                heightSpan = 0.f;
            }

            if(heightSpan > 1.f) {
                heightSpan = 1.f;
            }

            if(g_debugDamage.integer > 2)
                idSGameMain::Printf(".   armourRegion = [%d %d %f %f] (%s)\n"
                                    ".   .   modifier = %f\n"
                                    ".   .   angleSpan = %f\n"
                                    ".   .   heightSpan = %f\n",
                                    regions[ i ].minAngle, regions[ i ].maxAngle,
                                    regions[ i ].minHeight, regions[ i ].maxHeight,
                                    regions[ i ].name, regions[ i ].modifier,
                                    angleSpan, heightSpan);

            areaSum += area = angleSpan * heightSpan;
            modifier += regions[ i ].modifier * area;
        }
    }

    if(g_debugDamage.integer > 2)
        idSGameMain::Printf(".   areaSum = %f\n"
                            ".   armourModifier = %f\n", areaSum, modifier);

    return overlap->modifier * (overlap->area + modifier - areaSum);
}

/*
============
GetNonLocDamageModifier
============
*/
float32 idSGameCombat::GetNonLocDamageModifier(gentity_t *targ,
        sint _class) {
    float32 modifier = 0., area = 0.f, scale = 0.f;
    sint i;
    bool crouch;

    // For every body region, use stretch-armor formula to apply armour modifier
    // for any overlapping area that armour shares with the body region
    crouch = targ->client->ps.pm_flags & PMF_DUCKED;

    for(i = 0; i < g_numDamageRegions[_class]; i++) {
        damageRegion_t *region;

        region = &g_damageRegions[_class][ i ];

        if(region->crouch != crouch) {
            continue;
        }

        modifier += GetRegionDamageModifier(targ, _class, i);
        scale += region->modifier * region->area;
        area += region->area;
    }

    modifier = !scale ? 1.f : 1.f + (modifier / scale - 1.f) * area;

    if(g_debugDamage.integer > 1) {
        idSGameMain::Printf("GetNonLocDamageModifier() modifier:%f, area:%f, scale:%f\n",
                            modifier, area, scale);
    }

    return modifier;
}

/*
============
GetPointDamageModifier

Returns the damage region given an angle and a height proportion
============
*/
float32 idSGameCombat::GetPointDamageModifier(gentity_t *targ,
        damageRegion_t *regions, sint len, float32 angle, float32 height) {
    float32 modifier = 1.f;
    sint i;

    for(i = 0; i < len; i++) {
        if(regions[ i ].crouch != (targ->client->ps.pm_flags & PMF_DUCKED)) {
            continue;
        }

        // Angle must be within range
        if((regions[ i ].minAngle <= regions[ i ].maxAngle &&
                (angle < regions[ i ].minAngle ||
                 angle > regions[ i ].maxAngle)) ||
                (regions[ i ].minAngle > regions[ i ].maxAngle &&
                 angle > regions[ i ].maxAngle && angle < regions[ i ].minAngle)) {
            continue;
        }

        // Height must be within range
        if(height < regions[ i ].minHeight || height > regions[ i ].maxHeight) {
            continue;
        }

        modifier *= regions[ i ].modifier;
    }

    if(g_debugDamage.integer) {
        idSGameMain::Printf("GetDamageRegionModifier(angle = %f, height = %f): %f\n",
                            angle, height, modifier);
    }

    return modifier;
}

/*
============
G_CalcDamageModifier
============
*/
float32 idSGameCombat::CalcDamageModifier(vec3_t point, gentity_t *targ,
        gentity_t *attacker, sint _class, sint dflags) {
    vec3_t  targOrigin, bulletPath, bulletAngle, pMINUSfloor, floor, normal;
    float32   clientHeight, hitRelative, hitRatio, modifier;
    sint     hitRotation, i;

    if(point == nullptr) {
        return 1.0f;
    }

    // Don't need to calculate angles and height for non-locational damage
    if(dflags & DAMAGE_NO_LOCDAMAGE) {
        return GetNonLocDamageModifier(targ, _class);
    }

    // Get the point location relative to the floor under the target
    if(g_unlagged.integer && targ->client && targ->client->unlaggedCalc.used) {
        VectorCopy(targ->client->unlaggedCalc.origin, targOrigin);
    } else {
        VectorCopy(targ->r.currentOrigin, targOrigin);
    }

    bggame->GetClientNormal(&targ->client->ps, normal);
    VectorMA(targOrigin, targ->r.mins[ 2 ], normal, floor);
    VectorSubtract(point, floor, pMINUSfloor);

    // Get the proportion of the target height where the hit landed
    clientHeight = targ->r.maxs[ 2 ] - targ->r.mins[ 2 ];

    if(!clientHeight) {
        clientHeight = 1.f;
    }

    hitRelative = DotProduct(normal, pMINUSfloor) / VectorLength(normal);

    if(hitRelative < 0.0f) {
        hitRelative = 0.0f;
    }

    if(hitRelative > clientHeight) {
        hitRelative = clientHeight;
    }

    hitRatio = hitRelative / clientHeight;

    // Get the yaw of the attack relative to the target's view yaw
    VectorSubtract(targOrigin, point, bulletPath);
    vectoangles(bulletPath, bulletAngle);
    hitRotation = AngleNormalize360(targ->client->ps.viewangles[ YAW ] -
                                    bulletAngle[ YAW ]);

    // Get modifiers from the target's damage regions
    modifier = GetPointDamageModifier(targ, g_damageRegions[_class],
                                      g_numDamageRegions[_class],
                                      hitRotation, hitRatio);

    for(i = UP_NONE + 1; i < UP_NUM_UPGRADES; i++)
        if(bggame->InventoryContainsUpgrade(i, targ->client->ps.stats))
            modifier *= GetPointDamageModifier(targ, g_armourRegions[ i ],
                                               g_numArmourRegions[ i ],
                                               hitRotation, hitRatio);

    return modifier;
}

/*
============
idSGameCombat::InitDamageLocations
============
*/
void idSGameCombat::InitDamageLocations(void) {
    valueType          *modelName;
    valueType          filename[ MAX_QPATH ];
    sint           i;
    sint           len;
    fileHandle_t  fileHandle;
    valueType          buffer[ MAX_DAMAGE_REGION_TEXT ];

    for(i = PCL_NONE + 1; i < PCL_NUM_CLASSES; i++) {
        modelName = bggame->ClassConfig((class_t)i)->modelName;
        Q_vsprintf_s(filename, sizeof(filename), sizeof(filename),
                     "models/players/%s/locdamage.cfg", modelName);

        len = trap_FS_FOpenFile(filename, &fileHandle, FS_READ);

        if(!fileHandle) {
            idSGameMain::Printf(S_COLOR_RED "file not found: %s\n", filename);
            continue;
        }

        if(len >= MAX_DAMAGE_REGION_TEXT) {
            idSGameMain::Printf(S_COLOR_RED
                                "file too large: %s is %i, max allowed is %i", filename, len,
                                MAX_DAMAGE_REGION_TEXT);
            trap_FS_FCloseFile(fileHandle);
            continue;
        }

        trap_FS_Read(buffer, len, fileHandle);
        buffer[len] = 0;
        trap_FS_FCloseFile(fileHandle);

        g_numDamageRegions[ i ] = ParseDmgScript(g_damageRegions[ i ], buffer);
    }

    for(i = UP_NONE + 1; i < UP_NUM_UPGRADES; i++) {
        modelName = bggame->Upgrade((upgrade_t)i)->name;
        Q_vsprintf_s(filename, sizeof(filename), sizeof(filename),
                     "armour/%s.armour", modelName);

        len = trap_FS_FOpenFile(filename, &fileHandle, FS_READ);

        //no file - no parsage
        if(!fileHandle) {
            continue;
        }

        if(len >= MAX_DAMAGE_REGION_TEXT) {
            idSGameMain::Printf(S_COLOR_RED
                                "file too large: %s is %i, max allowed is %i", filename, len,
                                MAX_DAMAGE_REGION_TEXT);
            trap_FS_FCloseFile(fileHandle);
            continue;
        }

        trap_FS_Read(buffer, len, fileHandle);
        buffer[len] = 0;
        trap_FS_FCloseFile(fileHandle);

        g_numArmourRegions[ i ] = ParseDmgScript(g_armourRegions[ i ], buffer);
    }
}


/*
============
T_Damage

targ    entity that is being damaged
inflictor entity that is causing the damage
attacker  entity that caused the inflictor to damage targ
  example: targ=monster, inflictor=rocket, attacker=player

dir     direction of the attack for knockback
point   point at which the damage is being inflicted, used for headshots
damage    amount of damage being inflicted
knockback force to be applied against targ as a result of the damage

inflictor, attacker, dir, and point can be nullptr for environmental effects

dflags    these flags are used to control how T_Damage works
  DAMAGE_RADIUS     damage was indirect (from a nearby explosion)
  DAMAGE_NO_ARMOR     armor does not protect from this damage
  DAMAGE_NO_KNOCKBACK   do not affect velocity, just view angles
  DAMAGE_NO_PROTECTION  kills godmode, armor, everything
============
*/

void idSGameCombat::Damage(gentity_t *targ, gentity_t *inflictor,
                           gentity_t *attacker, vec3_t dir, vec3_t point, sint damage, sint dflags,
                           sint mod) {
    gclient_t *client;
    sint     take;
    sint     save;
    sint     asave = 0;
    sint     knockback;

    // Can't deal damage sometimes
    if(!targ->takedamage || targ->health <= 0 || level.intermissionQueued) {
        return;
    }

    if(!inflictor) {
        inflictor = &g_entities[ ENTITYNUM_WORLD ];
    }

    if(!attacker) {
        attacker = &g_entities[ ENTITYNUM_WORLD ];
    }

    // shootable doors / buttons don't actually have any health
    if(targ->s.eType == ET_MOVER) {
        if(targ->use && (targ->moverState == MOVER_POS1 ||
                         targ->moverState == ROTATOR_POS1)) {
            targ->use(targ, inflictor, attacker);
        }

        if(attacker->client->pers.teamSelection == TEAM_ALIENS) {
            if(mod == MOD_LEVEL3_BOUNCEBALL ||
                    mod == MOD_SLOWBLOB          ||
                    mod == MOD_LEVEL4_TRAMPLE    ||
                    mod == MOD_LEVEL4_CRUSH      ||
                    mod == MOD_LEVEL3_POUNCE     ||
                    mod == MOD_LEVEL1_PCLOUD     ||
                    mod == MOD_LEVEL2_ZAP) {
                idSGameUtils::AddEvent(attacker, EV_ALIENRANGED_HIT, targ->s.number);
            } else {
                idSGameUtils::AddEvent(attacker, EV_ALIEN_HIT, targ->s.number);
            }
        }

        return;
    }

    client = targ->client;

    if(client && client->noclip) {
        return;
    }

    if(!dir) {
        dflags |= DAMAGE_NO_KNOCKBACK;
    } else {
        VectorNormalize(dir);
    }

    knockback = damage;

    if(inflictor->s.weapon != WP_NONE) {
        knockback = (sint)((float32)knockback *
                           bggame->Weapon((weapon_t)inflictor->s.weapon)->knockbackScale);
    }

    if(targ->client) {
        knockback = (sint)((float32)knockback *
                           bggame->Class((class_t)
                                         targ->client->ps.stats[ STAT_CLASS ])->knockbackScale);
    }

    if(knockback > 200) {
        knockback = 200;
    }

    if(targ->flags & FL_NO_KNOCKBACK) {
        knockback = 0;
    }

    if(dflags & DAMAGE_NO_KNOCKBACK) {
        knockback = 0;
    }

    // figure momentum add, even if the damage won't be taken
    if(knockback && targ->client) {
        vec3_t  kvel;
        float32   mass;

        mass = 200;

        VectorScale(dir, g_knockback.value * (float32)knockback / mass, kvel);
        VectorAdd(targ->client->ps.velocity, kvel, targ->client->ps.velocity);

        // set the timer so that the other client can't cancel
        // out the movement immediately
        if(!targ->client->ps.pm_time) {
            sint   t;

            t = knockback * 2;

            if(t < 50) {
                t = 50;
            }

            if(t > 200) {
                t = 200;
            }

            targ->client->ps.pm_time = t;
            targ->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
        }
    }

    // don't do friendly fire on movement attacks
    if((mod == MOD_LEVEL4_TRAMPLE || mod == MOD_LEVEL3_POUNCE ||
            mod == MOD_LEVEL4_CRUSH) &&
            targ->s.eType == ET_BUILDABLE && targ->buildableTeam == TEAM_ALIENS) {
        return;
    }

    // check for completely getting out of the damage
    if(!(dflags & DAMAGE_NO_PROTECTION)) {

        // if TF_NO_FRIENDLY_FIRE is set, don't do damage to the target
        // if the attacker was on the same team
        if(targ != attacker && idSGameTeam::OnSameTeam(targ, attacker)) {
            // don't do friendly fire on movement attacks
            if(mod == MOD_LEVEL4_TRAMPLE || mod == MOD_LEVEL3_POUNCE ||
                    mod == MOD_LEVEL4_CRUSH) {
                return;
            }

            // if dretchpunt is enabled and this is a dretch, do dretchpunt instead of damage
            if(g_dretchPunt.integer &&
                    targ->client->ps.stats[ STAT_CLASS ] == PCL_ALIEN_LEVEL0) {
                vec3_t dir, push;

                VectorSubtract(targ->r.currentOrigin, attacker->r.currentOrigin, dir);
                VectorNormalizeFast(dir);
                VectorScale(dir, (damage * 10.0f), push);
                push[2] = 64.0f;
                VectorAdd(targ->client->ps.velocity, push, targ->client->ps.velocity);
                return;
            }
            // check if friendly fire has been disabled
            else if(!g_friendlyFire.integer) {
                if(!g_friendlyFireHumans.integer &&
                        targ->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS) {
                    return;
                }

                if(!g_friendlyFireAliens.integer &&
                        targ->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS) {
                    if(mod == MOD_LEVEL3_BOUNCEBALL ||
                            mod == MOD_SLOWBLOB          ||
                            mod == MOD_LEVEL4_TRAMPLE    ||
                            mod == MOD_LEVEL4_CRUSH      ||
                            mod == MOD_LEVEL3_POUNCE     ||
                            mod == MOD_LEVEL1_PCLOUD     ||
                            mod == MOD_LEVEL2_ZAP) {
                        idSGameUtils::AddEvent(attacker, EV_ALIENRANGED_TEAMHIT, targ->s.number);
                    } else {
                        idSGameUtils::AddEvent(attacker, EV_ALIEN_TEAMHIT, targ->s.number);
                    }

                    return;
                } else {
                    if(mod == MOD_LEVEL3_BOUNCEBALL ||
                            mod == MOD_SLOWBLOB          ||
                            mod == MOD_LEVEL4_TRAMPLE    ||
                            mod == MOD_LEVEL4_CRUSH      ||
                            mod == MOD_LEVEL3_POUNCE     ||
                            mod == MOD_LEVEL1_PCLOUD     ||
                            mod == MOD_LEVEL2_ZAP) {
                        idSGameUtils::AddEvent(attacker, EV_ALIENRANGED_MISS, targ->s.number);
                    } else {
                        idSGameUtils::AddEvent(attacker, EV_ALIEN_MISS, targ->s.number);
                    }
                }
            }
        }

        if(targ->s.eType == ET_BUILDABLE && attacker->client) {
            if(targ->buildableTeam == attacker->client->pers.teamSelection) {
                if(!g_friendlyBuildableFire.integer) {
                    if(mod == MOD_LEVEL3_BOUNCEBALL ||
                            mod == MOD_SLOWBLOB          ||
                            mod == MOD_LEVEL4_TRAMPLE    ||
                            mod == MOD_LEVEL4_CRUSH      ||
                            mod == MOD_LEVEL3_POUNCE     ||
                            mod == MOD_LEVEL1_PCLOUD     ||
                            mod == MOD_LEVEL2_ZAP) {
                        idSGameUtils::AddEvent(attacker, EV_ALIENRANGED_MISS, targ->s.number);
                    } else {
                        idSGameUtils::AddEvent(attacker, EV_ALIEN_MISS, targ->s.number);
                    }

                    return;
                } else if(mod == MOD_LEVEL3_BOUNCEBALL ||
                          mod == MOD_SLOWBLOB          ||
                          mod == MOD_LEVEL4_TRAMPLE    ||
                          mod == MOD_LEVEL4_CRUSH      ||
                          mod == MOD_LEVEL3_POUNCE     ||
                          mod == MOD_LEVEL1_PCLOUD     ||
                          mod == MOD_LEVEL2_ZAP) {
                    idSGameUtils::AddEvent(attacker, EV_ALIENRANGED_HIT, targ->s.number);
                } else {
                    idSGameUtils::AddEvent(attacker, EV_ALIEN_HIT, targ->s.number);
                }
            }

            // base is under attack warning if DCC'd
            if(targ->buildableTeam == TEAM_HUMANS && idSGameBuildable::FindDCC(targ) &&
                    level.time > level.humanBaseAttackTimer && mod != MOD_DECONSTRUCT &&
                    mod != MOD_SUICIDE) {
                level.humanBaseAttackTimer = level.time + DC_ATTACK_PERIOD;
                idSGameUtils::BroadcastEvent(EV_DCC_ATTACK, 0);
            }
        }

        // check for godmode
        if(targ->flags & FL_GODMODE) {
            return;
        }
    }

    // add to the attacker's hit counter
    if(attacker->client && targ != attacker && targ->health > 0
            && targ->s.eType != ET_MISSILE
            && targ->s.eType != ET_GENERAL) {
        if(idSGameTeam::OnSameTeam(targ, attacker)) {
            attacker->client->ps.persistant[ PERS_HITS ]--;
        } else {
            attacker->client->ps.persistant[ PERS_HITS ]++;
        }
    }

    take = damage;
    save = 0;

    // add to the damage inflicted on a player this frame
    // the total will be turned into screen blends and view angle kicks
    // at the end of the frame
    if(client) {
        if(attacker) {
            client->ps.persistant[ PERS_ATTACKER ] = attacker->s.number;
        } else {
            client->ps.persistant[ PERS_ATTACKER ] = ENTITYNUM_WORLD;
        }

        client->damage_armor += asave;
        client->damage_blood += take;
        client->damage_knockback += knockback;

        if(dir) {
            VectorCopy(dir, client->damage_from);
            client->damage_fromWorld = false;
        } else {
            VectorCopy(targ->r.currentOrigin, client->damage_from);
            client->damage_fromWorld = true;
        }

        // set the last client who damaged the target
        targ->client->lasthurt_client = attacker->s.number;
        targ->client->lasthurt_mod = mod;
        take = (sint)(take * CalcDamageModifier(point, targ, attacker,
                                                client->ps.stats[ STAT_CLASS ], dflags) + 0.5f);

        //if boosted poison every attack
        if(attacker->client &&
                attacker->client->ps.stats[ STAT_STATE ] & SS_BOOSTED) {
            if(targ->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS &&
                    mod != MOD_LEVEL2_ZAP && mod != MOD_POISON &&
                    mod != MOD_LEVEL1_PCLOUD &&
                    targ->client->poisonImmunityTime < level.time) {
                targ->client->ps.stats[ STAT_STATE ] |= SS_POISONED;
                targ->client->lastPoisonTime = level.time;
                targ->client->lastPoisonClient = attacker;
            }
        }
    }

    if(take < 1) {
        take = 1;
    }

    if(g_debugDamage.integer) {
        idSGameMain::Printf("%i: client:%i health:%i damage:%i armor:%i\n",
                            level.time, targ->s.number, targ->health, take, asave);
    }

    // do the damage
    if(take) {
        if(attacker->client &&
                attacker->client->pers.teamSelection == TEAM_ALIENS) {
            if(mod == MOD_LEVEL3_BOUNCEBALL ||
                    mod == MOD_SLOWBLOB          ||
                    mod == MOD_LEVEL4_TRAMPLE    ||
                    mod == MOD_LEVEL4_CRUSH      ||
                    mod == MOD_LEVEL3_POUNCE     ||
                    mod == MOD_LEVEL1_PCLOUD     ||
                    mod == MOD_LEVEL2_ZAP) {
                idSGameUtils::AddEvent(attacker, EV_ALIENRANGED_HIT, targ->s.number);
            } else {
                idSGameUtils::AddEvent(attacker, EV_ALIEN_HIT, targ->s.number);
            }
        }

        targ->health = targ->health - take;

        if(targ->client) {
            targ->client->ps.stats[ STAT_HEALTH ] = targ->health;
        }

        targ->lastDamageTime = level.time;
        targ->nextRegenTime = level.time + ALIEN_REGEN_DAMAGE_TIME;

        // add to the attackers "account" on the target
        if(attacker->client && attacker != targ &&
                !idSGameTeam::OnSameTeam(targ, attacker)) {
            targ->credits[ attacker->client->ps.clientNum ] += take;
        }

        if(targ->health <= 0) {
            if(client) {
                targ->flags |= FL_NO_KNOCKBACK;
            }

            if(targ->health < -999) {
                targ->health = -999;
            }

            targ->enemy = attacker;
            targ->die(targ, inflictor, attacker, take, mod);
            return;
        } else if(targ->pain) {
            targ->pain(targ, attacker, take);
        }
    }
}


/*
============
idSGameCombat::CanDamage

Returns true if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
bool idSGameCombat::CanDamage(gentity_t *targ, vec3_t origin) {
    vec3_t  dest;
    trace_t tr;
    vec3_t  midpoint;

    // use the midpoint of the bounds instead of the origin, because
    // bmodels may have their origin is 0,0,0
    VectorAdd(targ->r.absmin, targ->r.absmax, midpoint);
    VectorScale(midpoint, 0.5, midpoint);

    VectorCopy(midpoint, dest);
    trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE,
               MASK_SOLID);

    if(tr.fraction == 1.0  || tr.entityNum == targ->s.number) {
        return true;
    }

    // this should probably check in the plane of projection,
    // rather than in world coordinate, and also include Z
    VectorCopy(midpoint, dest);
    dest[ 0 ] += 15.0;
    dest[ 1 ] += 15.0;
    trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE,
               MASK_SOLID);

    if(tr.fraction == 1.0) {
        return true;
    }

    VectorCopy(midpoint, dest);
    dest[ 0 ] += 15.0;
    dest[ 1 ] -= 15.0;
    trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE,
               MASK_SOLID);

    if(tr.fraction == 1.0) {
        return true;
    }

    VectorCopy(midpoint, dest);
    dest[ 0 ] -= 15.0;
    dest[ 1 ] += 15.0;
    trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE,
               MASK_SOLID);

    if(tr.fraction == 1.0) {
        return true;
    }

    VectorCopy(midpoint, dest);
    dest[ 0 ] -= 15.0;
    dest[ 1 ] -= 15.0;
    trap_Trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE,
               MASK_SOLID);

    if(tr.fraction == 1.0) {
        return true;
    }

    return false;
}

/*
============
idSGameCombat::SelectiveRadiusDamage
============
*/
bool idSGameCombat::SelectiveRadiusDamage(vec3_t origin,
        gentity_t *attacker, float32 damage, float32 radius, gentity_t *ignore,
        sint mod, sint team) {
    float32     points, dist;
    gentity_t *ent;
    sint       entityList[ MAX_GENTITIES ];
    sint       numListedEntities;
    vec3_t    mins, maxs;
    vec3_t    v;
    vec3_t    dir;
    sint       i, e;
    bool  hitClient = false;

    if(radius < 1) {
        radius = 1;
    }

    for(i = 0; i < 3; i++) {
        mins[ i ] = origin[ i ] - radius;
        maxs[ i ] = origin[ i ] + radius;
    }

    numListedEntities = trap_EntitiesInBox(mins, maxs, entityList,
                                           MAX_GENTITIES);

    for(e = 0; e < numListedEntities; e++) {
        ent = &g_entities[ entityList[ e ] ];

        if(ent == ignore) {
            continue;
        }

        if(!ent->takedamage) {
            continue;
        }

        // find the distance from the edge of the bounding box
        for(i = 0 ; i < 3 ; i++) {
            if(origin[ i ] < ent->r.absmin[ i ]) {
                v[ i ] = ent->r.absmin[ i ] - origin[ i ];
            } else if(origin[ i ] > ent->r.absmax[ i ]) {
                v[ i ] = origin[ i ] - ent->r.absmax[ i ];
            } else {
                v[ i ] = 0;
            }
        }

        dist = VectorLength(v);

        if(dist >= radius) {
            continue;
        }

        points = damage * (1.0 - dist / radius);

        if(CanDamage(ent, origin) && ent->client &&
                ent->client->ps.stats[ STAT_TEAM ] != team) {
            VectorSubtract(ent->r.currentOrigin, origin, dir);
            // push the center of mass higher than the origin so players
            // get knocked into the air more
            dir[ 2 ] += 24;
            hitClient = true;
            Damage(ent, nullptr, attacker, dir, origin, (sint)points,
                   DAMAGE_RADIUS | DAMAGE_NO_LOCDAMAGE, mod);
        }
    }

    return hitClient;
}


/*
============
idSGameCombat::RadiusDamage
============
*/
bool idSGameCombat::RadiusDamage(vec3_t origin, gentity_t *attacker,
                                 float32 damage, float32 radius, gentity_t *ignore, sint mod) {
    float32     points, dist;
    gentity_t *ent;
    sint       entityList[ MAX_GENTITIES ];
    sint       numListedEntities;
    vec3_t    mins, maxs;
    vec3_t    v;
    vec3_t    dir;
    sint       i, e;
    bool  hitClient = false;

    if(radius < 1) {
        radius = 1;
    }

    for(i = 0; i < 3; i++) {
        mins[ i ] = origin[ i ] - radius;
        maxs[ i ] = origin[ i ] + radius;
    }

    numListedEntities = trap_EntitiesInBox(mins, maxs, entityList,
                                           MAX_GENTITIES);

    for(e = 0; e < numListedEntities; e++) {
        ent = &g_entities[ entityList[ e ] ];

        if(ent == ignore) {
            continue;
        }

        if(!ent->takedamage) {
            continue;
        }

        // find the distance from the edge of the bounding box
        for(i = 0; i < 3; i++) {
            if(origin[ i ] < ent->r.absmin[ i ]) {
                v[ i ] = ent->r.absmin[ i ] - origin[ i ];
            } else if(origin[ i ] > ent->r.absmax[ i ]) {
                v[ i ] = origin[ i ] - ent->r.absmax[ i ];
            } else {
                v[ i ] = 0;
            }
        }

        dist = VectorLength(v);

        if(dist >= radius) {
            continue;
        }

        points = damage * (1.0 - dist / radius);

        if(CanDamage(ent, origin)) {
            VectorSubtract(ent->r.currentOrigin, origin, dir);
            // push the center of mass higher than the origin so players
            // get knocked into the air more
            dir[ 2 ] += 24;
            hitClient = true;
            Damage(ent, nullptr, attacker, dir, origin, (sint)points,
                   DAMAGE_RADIUS | DAMAGE_NO_LOCDAMAGE, mod);
        }
    }

    return hitClient;
}

/*
================
idSGameCombat::LogDestruction

Log deconstruct/destroy events
================
*/
void idSGameCombat::LogDestruction(gentity_t *self, gentity_t *actor,
                                   sint mod) {
    if(!actor || !actor->client) {
        return;
    }

    if(actor->client->ps.stats[ STAT_TEAM ] ==
            bggame->Buildable((buildable_t)self->s.modelindex)->team) {
        idSGameUtils::TeamCommand((team_t)actor->client->ps.stats[ STAT_TEAM ],
                                  va("print \"%s ^3%s^7 by %s\n\"",
                                     bggame->Buildable((buildable_t)self->s.modelindex)->humanName,
                                     mod == MOD_DECONSTRUCT ? "DECONSTRUCTED" : "DESTROYED",
                                     actor->client->pers.netname));
    }

    if(mod == MOD_DECONSTRUCT)
        idSGameMain::LogPrintf("Decon: %d %d %d: %s^7 deconstructed %s\n",
                               actor->client->ps.clientNum,
                               self->s.modelindex,
                               mod,
                               actor->client->pers.netname,
                               bggame->Buildable((buildable_t)self->s.modelindex)->name);
    else
        idSGameMain::LogPrintf("Decon: %d %d %d: %s^7 destroyed %s by %s\n",
                               actor->client->ps.clientNum,
                               self->s.modelindex,
                               mod,
                               actor->client->pers.netname,
                               bggame->Buildable((buildable_t)self->s.modelindex)->name,
                               modNames[ mod ]);
}
