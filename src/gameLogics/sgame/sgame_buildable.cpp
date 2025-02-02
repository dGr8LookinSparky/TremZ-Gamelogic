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
// File name:   sgame_buildable.cpp
// Created:
// Compilers:   Microsoft (R) C/C++ Optimizing Compiler Version 19.26.28806 for x64,
//              gcc (Ubuntu 9.3.0-10ubuntu2) 9.3.0
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#include <sgame/sgame_precompiled.hpp>

/*
===============
idSGameBuildable::idSGameBuildable
===============
*/
idSGameBuildable::idSGameBuildable(void) {
}

/*
===============
idSGameBuildable::~idSGameBuildable
===============
*/
idSGameBuildable::~idSGameBuildable(void) {
}

/*
================
idSGameBuildable::SetBuildableAnim

Triggers an animation client side
================
*/
void idSGameBuildable::SetBuildableAnim(gentity_t *ent,
                                        buildableAnimNumber_t anim, bool force) {
    sint localAnim = anim | (ent->s.legsAnim & ANIM_TOGGLEBIT);

    if(force) {
        localAnim |= ANIM_FORCEBIT;
    }

    // don't flip the togglebit more than once per frame
    if(ent->animTime != level.time) {
        ent->animTime = level.time;
        localAnim ^= ANIM_TOGGLEBIT;
    }

    ent->s.legsAnim = localAnim;
}

/*
================
idSGameBuildable::SetIdleBuildableAnim

Set the animation to use whilst no other animations are running
================
*/
void idSGameBuildable::SetIdleBuildableAnim(gentity_t *ent,
        buildableAnimNumber_t anim) {
    ent->s.torsoAnim = anim;
}

/*
===============
idSGameBuildable::CheckSpawnPoint

Check if a spawn at a specified point is valid
===============
*/
gentity_t *idSGameBuildable::CheckSpawnPoint(sint spawnNum, vec3_t origin,
        vec3_t normal, buildable_t spawn, vec3_t spawnOrigin) {
    float32   displacement;
    vec3_t  mins, maxs;
    vec3_t  cmins, cmaxs;
    vec3_t  localOrigin;
    trace_t tr;

    bggame->BuildableBoundingBox(spawn, mins, maxs);

    if(spawn == BA_A_SPAWN) {
        VectorSet(cmins, -MAX_ALIEN_BBOX, -MAX_ALIEN_BBOX, -MAX_ALIEN_BBOX);
        VectorSet(cmaxs,  MAX_ALIEN_BBOX,  MAX_ALIEN_BBOX,  MAX_ALIEN_BBOX);

        displacement = (maxs[ 2 ] + MAX_ALIEN_BBOX) * M_ROOT3;
        VectorMA(origin, displacement, normal, localOrigin);
    } else if(spawn == BA_H_SPAWN) {
        bggame->ClassBoundingBox(PCL_HUMAN, cmins, cmaxs, nullptr, nullptr,
                                 nullptr);

        VectorCopy(origin, localOrigin);
        localOrigin[ 2 ] += maxs[ 2 ] + fabs(cmins[ 2 ]) + 1.0f;
    } else {
        return nullptr;
    }

    trap_Trace(&tr, origin, nullptr, nullptr, localOrigin, spawnNum,
               MASK_SHOT);

    if(tr.entityNum != ENTITYNUM_NONE) {
        return &g_entities[ tr.entityNum ];
    }

    trap_Trace(&tr, localOrigin, cmins, cmaxs, localOrigin, ENTITYNUM_NONE,
               MASK_PLAYERSOLID);

    if(tr.entityNum != ENTITYNUM_NONE) {
        return &g_entities[ tr.entityNum ];
    }

    if(spawnOrigin != nullptr) {
        VectorCopy(localOrigin, spawnOrigin);
    }

    return nullptr;
}

/*
================
G_NumberOfDependants

Return number of entities that depend on this one
================
*/
sint idSGameBuildable::NumberOfDependants(gentity_t *self) {
    sint       i, n = 0;
    gentity_t *ent;

    for(i = MAX_CLIENTS, ent = g_entities + i; i < level.num_entities;
            i++, ent++) {
        if(ent->s.eType != ET_BUILDABLE) {
            continue;
        }

        if(ent->parentNode == self) {
            n++;
        }
    }

    return n;
}

#define POWER_REFRESH_TIME  2000

/*
================
G_FindPower

attempt to find power for self, return true if successful
================
*/
bool idSGameBuildable::FindPower(gentity_t *self) {
    sint       i;
    gentity_t *ent;
    gentity_t *closestPower = nullptr;
    sint       distance = 0;
    sint       minDistance = REPEATER_BASESIZE + 1;
    vec3_t    temp_v;

    if(self->buildableTeam != TEAM_HUMANS) {
        return false;
    }

    //reactor is always powered
    if(self->s.modelindex == BA_H_REACTOR) {
        return true;
    }

    //if this already has power then stop now
    if(self->parentNode && self->parentNode->powered) {
        return true;
    }

    //reset parent
    self->parentNode = nullptr;

    //iterate through entities
    for(i = MAX_CLIENTS, ent = g_entities + i; i < level.num_entities;
            i++, ent++) {
        if(ent->s.eType != ET_BUILDABLE) {
            continue;
        }

        //if entity is a power item calculate the distance to it
        if((ent->s.modelindex == BA_H_REACTOR ||
                ent->s.modelindex == BA_H_REPEATER) &&
                ent->spawned && ent->powered) {
            VectorSubtract(self->s.origin, ent->s.origin, temp_v);
            distance = VectorLength(temp_v);

            // Always prefer a reactor if there is one in range
            if(ent->s.modelindex == BA_H_REACTOR && distance <= REACTOR_BASESIZE) {
                self->parentNode = ent;
                return true;
            }

            if(distance < minDistance) {
                closestPower = ent;
                minDistance = distance;
            }
        }
    }

    //if there were no power items nearby give up
    if(closestPower) {
        self->parentNode = closestPower;
        return true;
    }

    return false;
}

/*
================
PowerEntityForPoint

Simple wrapper to G_FindPower to find the entity providing
power for the specified point
================
*/
gentity_t *idSGameBuildable::PowerEntityForPoint(vec3_t origin) {
    gentity_t dummy;

    dummy.parentNode = nullptr;
    dummy.buildableTeam = TEAM_HUMANS;
    dummy.s.modelindex = BA_NONE;
    VectorCopy(origin, dummy.s.origin);

    if(FindPower(&dummy)) {
        return dummy.parentNode;
    }

    return nullptr;
}

/*
================
G_FindRepeater

attempt to find a repeater in range of self, return true if successful
================
*/
bool idSGameBuildable::FindRepeater(gentity_t *self) {
    sint       i;
    gentity_t *ent;
    gentity_t *closestPower = nullptr;
    sint       distance = 0;
    sint       minDistance = REPEATER_BASESIZE + 1;
    vec3_t    temp_v;

    if(self->buildableTeam != TEAM_HUMANS) {
        return false;
    }

    //iterate through entities
    for(i = MAX_CLIENTS, ent = g_entities + i; i < level.num_entities;
            i++, ent++) {
        if(ent->s.eType != ET_BUILDABLE) {
            continue;
        }

        //if entity is a power item calculate the distance to it
        if(ent->s.modelindex == BA_H_REPEATER &&
                ent->spawned && ent->powered) {
            VectorSubtract(self->s.origin, ent->s.origin, temp_v);
            distance = VectorLength(temp_v);

            if(distance < minDistance) {
                closestPower = ent;
                minDistance = distance;
            }
        }
    }

    //if there were no power items nearby give up
    if(closestPower) {
        self->parentNode = closestPower;
        return true;
    }

    return false;
}
/*
================
G_RepeaterEntityForPoint

Simple wrapper to G_FindRepeater to find a  repeater providing
power for the specified point
================
*/
gentity_t *idSGameBuildable::RepeaterEntityForPoint(vec3_t origin) {
    gentity_t dummy;

    dummy.parentNode = nullptr;
    dummy.buildableTeam = TEAM_HUMANS;
    dummy.s.modelindex = BA_NONE;
    VectorCopy(origin, dummy.s.origin);

    if(FindRepeater(&dummy)) {
        return dummy.parentNode;
    }

    return nullptr;
}
/*
================
idSGameBuildable::IsPowered

Check if a location has power, returning the entity type
that is providing it
================
*/
buildable_t idSGameBuildable::IsPowered(vec3_t origin) {
    gentity_t *ent = PowerEntityForPoint(origin);

    if(ent) {
        return (buildable_t)ent->s.modelindex;
    }

    return BA_NONE;
}

/*
================
idSGameBuildable::FindDCC

attempt to find a controlling DCC for self, return number found
================
*/
sint idSGameBuildable::FindDCC(gentity_t *self) {
    sint i;
    gentity_t *ent;
    sint distance = 0;
    sint minDistance = 10000;
    vec3_t temp_v;
    sint foundDCC = 0;

    if(self->buildableTeam != TEAM_HUMANS) {
        return 0;
    }

    //iterate through entities
    for(i = MAX_CLIENTS, ent = g_entities + i; i < level.num_entities;
            i++, ent++) {
        if(ent->s.eType != ET_BUILDABLE) {
            continue;
        }

        //if entity is a dcc calculate the distance to it
        if(ent->s.modelindex == BA_H_DCC && ent->spawned && ent->powered) {
            VectorSubtract(self->s.origin, ent->s.origin, temp_v);
            distance = VectorLength(temp_v);

            if((!foundDCC || distance < minDistance) && ent->powered) {
                foundDCC++;
                minDistance = distance;
                foundDCC = true;
            }
        }
    }

    return foundDCC;
}

/*
================
idSGameBuildable::IsDCCBuilt

simple wrapper to idSGameBuildable::FindDCC to check for a dcc
================
*/
bool idSGameBuildable::IsDCCBuilt(void) {
    gentity_t dummy;

    ::memset(&dummy, 0, sizeof(gentity_t));

    dummy.buildableTeam = TEAM_HUMANS;

    return FindDCC(&dummy);
}

/*
================
idSGameBuildable::FindOvermind

Attempt to find an overmind for self
================
*/
bool idSGameBuildable::FindOvermind(gentity_t *self) {
    sint       i;
    gentity_t *ent;

    if(self->buildableTeam != TEAM_ALIENS) {
        return false;
    }

    //if this already has overmind then stop now
    if(self->overmindNode && self->overmindNode->health > 0) {
        return true;
    }

    //reset parent
    self->overmindNode = nullptr;

    //iterate through entities
    for(i = MAX_CLIENTS, ent = g_entities + i; i < level.num_entities;
            i++, ent++) {
        if(ent->s.eType != ET_BUILDABLE) {
            continue;
        }

        //if entity is an overmind calculate the distance to it
        if(ent->s.modelindex == BA_A_OVERMIND && ent->spawned && ent->health > 0) {
            self->overmindNode = ent;
            return true;
        }
    }

    return false;
}

/*
================
idSGameBuildable::FindCreep

attempt to find creep for self, return true if successful
================
*/
bool idSGameBuildable::FindCreep(gentity_t *self) {
    sint       i;
    gentity_t *ent;
    gentity_t *closestSpawn = nullptr;
    sint       distance = 0;
    sint       minDistance = 10000;
    vec3_t    temp_v;

    //don't check for creep if flying through the air
    if(self->s.groundEntityNum == -1) {
        return true;
    }

    //if self does not have a parentNode or it's parentNode is invalid find a new one
    if(self->client || self->parentNode == nullptr ||
            !self->parentNode->inuse ||
            self->parentNode->health <= 0) {
        for(i = MAX_CLIENTS, ent = g_entities + i;
                i < level.num_entities; i++, ent++) {
            if(ent->s.eType != ET_BUILDABLE) {
                continue;
            }

            if((ent->s.modelindex == BA_A_SPAWN ||
                    ent->s.modelindex == BA_A_OVERMIND) &&
                    ent->spawned && ent->health > 0) {
                VectorSubtract(self->s.origin, ent->s.origin, temp_v);
                distance = VectorLength(temp_v);

                if(distance < minDistance) {
                    closestSpawn = ent;
                    minDistance = distance;
                }
            }
        }

        if(minDistance <= CREEP_BASESIZE) {
            if(!self->client) {
                self->parentNode = closestSpawn;
            }

            return true;
        }

        return false;
    }

    if(self->client) {
        return false;
    }

    //if we haven't returned by now then we must already have a valid parent
    return true;
}

/*
================
G_IsCreepHere

simple wrapper to idSGameBuildable::FindCreep to check if a location has creep
================
*/
bool idSGameBuildable::IsCreepHere(vec3_t origin) {
    gentity_t dummy;

    ::memset(&dummy, 0, sizeof(gentity_t));

    dummy.parentNode = nullptr;
    dummy.s.modelindex = BA_NONE;
    VectorCopy(origin, dummy.s.origin);

    return FindCreep(&dummy);
}

/*
================
G_CreepSlow

Set any nearby humans' SS_CREEPSLOWED flag
================
*/
void idSGameBuildable::CreepSlow(gentity_t *self) {
    sint         entityList[ MAX_GENTITIES ];
    vec3_t      range;
    vec3_t      mins, maxs;
    sint         i, num;
    gentity_t   *enemy;
    buildable_t buildable = (buildable_t)self->s.modelindex;
    float32       creepSize = (float32)bggame->Buildable(buildable)->creepSize;

    VectorSet(range, creepSize, creepSize, creepSize);

    VectorAdd(self->s.origin, range, maxs);
    VectorSubtract(self->s.origin, range, mins);

    //find humans
    num = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

    for(i = 0; i < num; i++) {
        enemy = &g_entities[ entityList[ i ] ];

        if(enemy->client && enemy->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS &&
                enemy->client->ps.groundEntityNum != ENTITYNUM_NONE) {
            enemy->client->ps.stats[ STAT_STATE ] |= SS_CREEPSLOWED;
            enemy->client->lastCreepSlowTime = level.time;
        }
    }
}

/*
================
nullDieFunction

hack to prevent compilers complaining about function pointer -> nullptr conversion
================
*/
void idSGameBuildable::nullDieFunction(gentity_t *self,
                                       gentity_t *inflictor, gentity_t *attacker, sint damage, sint mod) {
}

/*
================
AGeneric_CreepRecede

Called when an alien buildable dies
================
*/
void idSGameBuildable::AGeneric_CreepRecede(gentity_t *self) {
    //if the creep just died begin the recession
    if(!(self->s.eFlags & EF_DEAD)) {
        self->s.eFlags |= EF_DEAD;
        QueueBuildPoints(self);
        idSGameUtils::AddEvent(self, EV_BUILD_DESTROY, 0);

        if(self->spawned) {
            self->s.time = -level.time;
        } else
            self->s.time = -(level.time -
                             (sint)((float32)CREEP_SCALEDOWN_TIME *
                                    (1.0f - ((float32)(level.time - self->buildTime) /
                                             (float32)bggame->Buildable((buildable_t)self->s.modelindex)->buildTime))));
    }

    //creep is still receeding
    if((self->timestamp + 10000) > level.time) {
        self->nextthink = level.time + 500;
    } else { //creep has died
        idSGameUtils::FreeEntity(self);
    }
}

/*
================
AGeneric_Blast

Called when an Alien buildable explodes after dead state
================
*/
void idSGameBuildable::AGeneric_Blast(gentity_t *self) {
    vec3_t  dir;

    VectorCopy(self->s.origin2, dir);

    //do a bit of radius damage
    idSGameCombat::SelectiveRadiusDamage(self->s.pos.trBase, self,
                                         self->splashDamage, self->splashRadius, self, self->splashMethodOfDeath,
                                         TEAM_ALIENS);

    //pretty events and item cleanup
    self->s.eFlags |= EF_NODRAW; //don't draw the model once it's destroyed
    idSGameUtils::AddEvent(self, EV_ALIEN_BUILDABLE_EXPLOSION, DirToByte(dir));
    self->timestamp = level.time;
    self->think = AGeneric_CreepRecede;
    self->nextthink = level.time + 500;

    self->r.contents = 0;    //stop collisions...
    trap_LinkEntity(self);   //...requires a relink
}

/*
================
AGeneric_Die

Called when an Alien buildable is killed and enters a brief dead state prior to
exploding.
================
*/
void idSGameBuildable::AGeneric_Die(gentity_t *self, gentity_t *inflictor,
                                    gentity_t *attacker, sint damage, sint mod) {
    idSGameCombat::RewardAttackers(self);
    SetBuildableAnim(self, BANIM_DESTROY1, true);
    SetIdleBuildableAnim(self, BANIM_DESTROYED);

    self->die = nullDieFunction;
    self->think = AGeneric_Blast;
    self->s.eFlags &= ~EF_FIRING; //prevent any firing effects

    if(self->spawned) {
        self->nextthink = level.time + 5000;
    } else {
        self->nextthink = level.time;    //blast immediately
    }

    if(attacker && attacker->client) {
        idSGameCombat::LogDestruction(self, attacker, mod);
    }
}

/*
================
AGeneric_CreepCheck

Tests for creep and kills the buildable if there is none
================
*/
void idSGameBuildable::AGeneric_CreepCheck(gentity_t *self) {
    gentity_t *spawn;

    spawn = self->parentNode;

    if(!FindCreep(self)) {
        if(spawn && self->killedBy != ENTITYNUM_NONE) {
            idSGameCombat::Damage(self, nullptr, g_entities + self->killedBy, nullptr,
                                  nullptr, self->health, 0, MOD_NOCREEP);
        } else {
            idSGameCombat::Damage(self, nullptr, nullptr, nullptr, nullptr,
                                  self->health, 0, MOD_NOCREEP);
        }

        return;
    }

    CreepSlow(self);
}

/*
================
AGeneric_Think

A generic think function for Alien buildables
================
*/
void idSGameBuildable::AGeneric_Think(gentity_t *self) {
    self->powered = level.overmindPresent;
    self->nextthink = level.time + bggame->Buildable((buildable_t)
                      self->s.modelindex)->nextthink;
    AGeneric_CreepCheck(self);
}

/*
================
AGeneric_Pain

A generic pain function for Alien buildables
================
*/
void idSGameBuildable::AGeneric_Pain(gentity_t *self, gentity_t *attacker,
                                     sint damage) {
    if(self->health <= 0) {
        return;
    }

    // Alien buildables only have the first pain animation defined
    SetBuildableAnim(self, BANIM_PAIN1, false);
}


/*
================
ASpawn_Die

Called when an alien spawn dies
================
*/
void idSGameBuildable::ASpawn_Die(gentity_t *self, gentity_t *inflictor,
                                  gentity_t *attacker, sint damage, sint mod) {
    sint i;

    AGeneric_Die(self, inflictor, attacker, damage, mod);

    // All supported structures that no longer have creep will have been killed
    // by whoever killed this structure
    for(i = MAX_CLIENTS; i < level.num_entities; i++) {
        gentity_t *ent = g_entities + i;

        if(!ent->inuse || ent->health <= 0 || ent->s.eType != ET_BUILDABLE ||
                ent->parentNode != self) {
            continue;
        }

        ent->killedBy = attacker - g_entities;
    }
}

/*
================
ASpawn_Think

think function for Alien Spawn
================
*/
void idSGameBuildable::ASpawn_Think(gentity_t *self) {
    gentity_t *ent;

    if(self->spawned) {
        //only suicide if at rest
        if(self->s.groundEntityNum) {
            if((ent = CheckSpawnPoint(self->s.number, self->s.origin, self->s.origin2,
                                      BA_A_SPAWN, nullptr)) != nullptr) {
                // If the thing blocking the spawn is a buildable, kill it.
                // If it's part of the map, kill self.
                if(ent->s.eType == ET_BUILDABLE) {
                    idSGameCombat::Damage(ent, nullptr, nullptr, nullptr, nullptr, 10000, 0,
                                          MOD_SUICIDE);
                    SetBuildableAnim(self, BANIM_SPAWN1, true);
                } else if(ent->s.number == ENTITYNUM_WORLD || ent->s.eType == ET_MOVER) {
                    idSGameCombat::Damage(self, nullptr, nullptr, nullptr, nullptr, 10000, 0,
                                          MOD_SUICIDE);
                    return;
                }

                if(ent->s.eType == ET_CORPSE) {
                    idSGameUtils::FreeEntity(ent);    //quietly remove
                }
            }
        }
    }

    CreepSlow(self);

    self->nextthink = level.time + bggame->Buildable((buildable_t)
                      self->s.modelindex)->nextthink;
}


#define OVERMIND_ATTACK_PERIOD 10000
#define OVERMIND_DYING_PERIOD  5000
#define OVERMIND_SPAWNS_PERIOD 30000

/*
================
AOvermind_Think

Think function for Alien Overmind
================
*/
void idSGameBuildable::AOvermind_Think(gentity_t *self) {
    vec3_t range = { OVERMIND_ATTACK_RANGE, OVERMIND_ATTACK_RANGE, OVERMIND_ATTACK_RANGE };
    vec3_t mins, maxs;
    sint    i;

    VectorAdd(self->s.origin, range, maxs);
    VectorSubtract(self->s.origin, range, mins);

    if(self->spawned && (self->health > 0)) {
        //do some damage
        if(idSGameCombat::SelectiveRadiusDamage(self->s.pos.trBase, self,
                                                self->splashDamage, self->splashRadius, self, MOD_OVERMIND, TEAM_ALIENS)) {
            self->timestamp = level.time;
            SetBuildableAnim(self, BANIM_ATTACK1, false);
        }

        // just in case an egg finishes building after we tell overmind to stfu
        if(level.numAlienSpawns > 0) {
            level.overmindMuted = false;
        }

        // shut up during intermission
        if(level.intermissiontime) {
            level.overmindMuted = true;
        }

        //low on spawns
        if(!level.overmindMuted && level.numAlienSpawns <= 0 &&
                level.time > self->overmindSpawnsTimer) {
            bool haveBuilder = false;
            gentity_t *builder;

            self->overmindSpawnsTimer = level.time + OVERMIND_SPAWNS_PERIOD;
            idSGameUtils::BroadcastEvent(EV_OVERMIND_SPAWNS, 0);

            for(i = 0; i < level.numConnectedClients; i++) {
                builder = &g_entities[ level.sortedClients[ i ] ];

                if(builder->health > 0 &&
                        (builder->client->pers.classSelection == PCL_ALIEN_BUILDER0 ||
                         builder->client->pers.classSelection == PCL_ALIEN_BUILDER0_UPG)) {
                    haveBuilder = true;
                    break;
                }
            }

            // aliens now know they have no eggs, but they're screwed, so stfu
            if(!haveBuilder || idSGameMain::TimeTilSuddenDeath() <= 0) {
                level.overmindMuted = true;
            }
        }

        //overmind dying
        if(self->health < (OVERMIND_HEALTH / 10.0f) &&
                level.time > self->overmindDyingTimer) {
            self->overmindDyingTimer = level.time + OVERMIND_DYING_PERIOD;
            idSGameUtils::BroadcastEvent(EV_OVERMIND_DYING, 0);
        }

        //overmind under attack
        if(self->health < self->lastHealth &&
                level.time > self->overmindAttackTimer) {
            self->overmindAttackTimer = level.time + OVERMIND_ATTACK_PERIOD;
            idSGameUtils::BroadcastEvent(EV_OVERMIND_ATTACK, 0);
        }

        self->lastHealth = self->health;
    } else {
        self->overmindSpawnsTimer = level.time + OVERMIND_SPAWNS_PERIOD;
    }

    CreepSlow(self);

    self->nextthink = level.time + bggame->Buildable((buildable_t)
                      self->s.modelindex)->nextthink;
}

/*
================
ABarricade_Pain

Barricade pain animation depends on shrunk state
================
*/
void idSGameBuildable::ABarricade_Pain(gentity_t *self,
                                       gentity_t *attacker, sint damage) {
    if(self->health <= 0) {
        return;
    }

    if(!self->shrunkTime) {
        SetBuildableAnim(self, BANIM_PAIN1, false);
    } else {
        SetBuildableAnim(self, BANIM_PAIN2, false);
    }
}

/*
================
ABarricade_Shrink

Set shrink state for a barricade. When unshrinking, checks to make sure there
is enough room.
================
*/
void idSGameBuildable::ABarricade_Shrink(gentity_t *self, bool shrink) {
    if(!self->spawned || self->health <= 0) {
        shrink = true;
    }

    if(shrink && self->shrunkTime) {
        sint anim;

        // We need to make sure that the animation has been set to shrunk mode
        // because we start out shrunk but with the construct animation when built
        self->shrunkTime = level.time;
        anim = self->s.torsoAnim & ~(ANIM_FORCEBIT | ANIM_TOGGLEBIT);

        if(self->spawned && self->health > 0 && anim != BANIM_DESTROYED) {
            SetIdleBuildableAnim(self, BANIM_DESTROYED);
            SetBuildableAnim(self, BANIM_ATTACK1, true);
        }

        return;
    }

    if(!shrink &&
            (!self->shrunkTime ||
             level.time < self->shrunkTime + BARRICADE_SHRINKTIMEOUT)) {
        return;
    }

    bggame->BuildableBoundingBox(BA_A_BARRICADE, self->r.mins, self->r.maxs);

    if(shrink) {
        self->r.maxs[ 2 ] = (sint)(self->r.maxs[ 2 ] * BARRICADE_SHRINKPROP);
        self->shrunkTime = level.time;

        // shrink animation, the destroy animation is used
        if(self->spawned && self->health > 0) {
            SetBuildableAnim(self, BANIM_ATTACK1, true);
            SetIdleBuildableAnim(self, BANIM_DESTROYED);
        }
    } else {
        trace_t tr;
        sint anim;

        trap_Trace(&tr, self->s.origin, self->r.mins, self->r.maxs,
                   self->s.origin, self->s.number, MASK_PLAYERSOLID);

        if(tr.startsolid || tr.fraction < 1.f) {
            self->r.maxs[ 2 ] = (sint)(self->r.maxs[ 2 ] * BARRICADE_SHRINKPROP);
            return;
        }

        self->shrunkTime = 0;

        // unshrink animation, IDLE2 has been hijacked for this
        anim = self->s.legsAnim & ~(ANIM_FORCEBIT | ANIM_TOGGLEBIT);

        if(self->spawned && self->health > 0 &&
                anim != BANIM_CONSTRUCT1 && anim != BANIM_CONSTRUCT2) {
            SetIdleBuildableAnim(self,
                                 (buildableAnimNumber_t)bggame->Buildable(BA_A_BARRICADE)->idleAnim);
            SetBuildableAnim(self, BANIM_ATTACK2, true);
        }
    }

    // a change in size requires a relink
    if(self->spawned) {
        trap_LinkEntity(self);
    }
}

/*
================
ABarricade_Die

Called when an alien barricade dies
================
*/
void idSGameBuildable::ABarricade_Die(gentity_t *self,
                                      gentity_t *inflictor, gentity_t *attacker, sint damage, sint mod) {
    AGeneric_Die(self, inflictor, attacker, damage, mod);
    ABarricade_Shrink(self, true);
}

/*
================
ABarricade_Think

Think function for Alien Barricade
================
*/
void idSGameBuildable::ABarricade_Think(gentity_t *self) {
    AGeneric_Think(self);
    ABarricade_Shrink(self, !FindOvermind(self));
}

/*
================
ABarricade_Touch

Barricades shrink when they are come into contact with an Alien that can
pass through
================
*/

void idSGameBuildable::ABarricade_Touch(gentity_t *self, gentity_t *other,
                                        trace_t *trace) {
    gclient_t *client = other->client;
    sint client_z, min_z;

    if(!client || client->pers.teamSelection != TEAM_ALIENS) {
        return;
    }

    // Client must be high enough to pass over. Note that STEPSIZE (18) is
    // hardcoded here because we don't include bg_local.h!
    client_z = other->s.origin[ 2 ] + other->r.mins[ 2 ];
    min_z = self->s.origin[ 2 ] - 18 +
            (sint)(self->r.maxs[ 2 ] * BARRICADE_SHRINKPROP);

    if(client_z < min_z) {
        return;
    }

    ABarricade_Shrink(self, true);
}

//==================================================================================

/*
================
AAcidTube_Think

Think function for Alien Acid Tube
================
*/
void idSGameBuildable::AAcidTube_Think(gentity_t *self) {
    sint       entityList[ MAX_GENTITIES ];
    vec3_t    range = { ACIDTUBE_RANGE, ACIDTUBE_RANGE, ACIDTUBE_RANGE };
    vec3_t    mins, maxs;
    sint       i, num;
    gentity_t *enemy;

    self->powered = level.overmindPresent;
    self->nextthink = level.time + bggame->Buildable((buildable_t)
                      self->s.modelindex)->nextthink;

    VectorAdd(self->s.origin, range, maxs);
    VectorSubtract(self->s.origin, range, mins);

    AGeneric_CreepCheck(self);

    // attack nearby humans
    if(self->spawned && self->health > 0 && FindOvermind(self)) {
        num = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

        for(i = 0; i < num; i++) {
            enemy = &g_entities[ entityList[ i ] ];

            if(!idSGameUtils::Visible(self, enemy, CONTENTS_SOLID)) {
                continue;
            }

            if(enemy->client && enemy->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS) {
                // start the attack animation
                if(level.time >= self->timestamp + ACIDTUBE_REPEAT_ANIM) {
                    self->timestamp = level.time;
                    SetBuildableAnim(self, BANIM_ATTACK1, false);
                    idSGameUtils::AddEvent(self, EV_ALIEN_ACIDTUBE,
                                           DirToByte(self->s.origin2));
                }

                idSGameCombat::SelectiveRadiusDamage(self->s.pos.trBase, self,
                                                     ACIDTUBE_DAMAGE, ACIDTUBE_RANGE, self, MOD_ATUBE, TEAM_ALIENS);
                self->nextthink = level.time + ACIDTUBE_REPEAT;
                return;
            }
        }
    }
}




//==================================================================================

/*
================
AHive_CheckTarget

Returns true and fires the hive missile if the target is valid
================
*/
bool idSGameBuildable::AHive_CheckTarget(gentity_t *self,
        gentity_t *enemy) {
    trace_t trace;
    vec3_t tip_origin, dirToTarget;

    // Check if this is a valid target
    if(enemy->health <= 0 || !enemy->client ||
            enemy->client->ps.stats[ STAT_TEAM ] != TEAM_HUMANS) {
        return false;
    }

    // Check if the tip of the hive can see the target
    VectorMA(self->s.pos.trBase, self->r.maxs[ 2 ], self->s.origin2,
             tip_origin);
    trap_Trace(&trace, tip_origin, nullptr, nullptr, enemy->s.pos.trBase,
               self->s.number, MASK_SHOT);

    if(trace.fraction < 1.0f && trace.entityNum != enemy->s.number) {
        return false;
    }

    self->active = true;
    self->target_ent = enemy;
    self->timestamp = level.time + HIVE_REPEAT;

    VectorSubtract(enemy->s.pos.trBase, self->s.pos.trBase, dirToTarget);
    VectorNormalize(dirToTarget);
    vectoangles(dirToTarget, self->turretAim);

    // Fire at target
    idSGameWeapons::FireWeapon(self);
    SetBuildableAnim(self, BANIM_ATTACK1, false);
    return true;
}

/*
================
AHive_Think

Think function for Alien Hive
================
*/
void idSGameBuildable::AHive_Think(gentity_t *self) {
    self->powered = level.overmindPresent;
    self->nextthink = level.time + bggame->Buildable((buildable_t)
                      self->s.modelindex)->nextthink;

    AGeneric_CreepCheck(self);

    // Hive missile hasn't returned in HIVE_REPEAT seconds, forget about it
    if(self->timestamp < level.time) {
        self->active = false;
    }

    // Find a target to attack
    if(self->spawned && !self->active && FindOvermind(self)) {
        sint i, num, entityList[ MAX_GENTITIES ];
        vec3_t mins, maxs,
               range = { HIVE_SENSE_RANGE, HIVE_SENSE_RANGE, HIVE_SENSE_RANGE };

        VectorAdd(self->s.origin, range, maxs);
        VectorSubtract(self->s.origin, range, mins);
        num = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

        for(i = 0; i < num; i++)
            if(AHive_CheckTarget(self, g_entities + entityList[ i ])) {
                return;
            }
    }
}

/*
================
AHive_Pain

pain function for Alien Hive
================
*/
void idSGameBuildable::AHive_Pain(gentity_t *self, gentity_t *attacker,
                                  sint damage) {
    if(self->health <= 0 || !level.overmindPresent) {
        return;
    }

    if(!self->active) {
        AHive_CheckTarget(self, attacker);
    }

    SetBuildableAnim(self, BANIM_PAIN1, false);
}

/*
================
AHive_Die

pain function for Alien Hive
================
*/
void idSGameBuildable::AHive_Die(gentity_t *self, gentity_t *inflictor,
                                 gentity_t *attacker, sint damage, sint mod) {
    if(attacker && attacker->client &&
            attacker->buildableTeam == TEAM_HUMANS &&
            self->spawned && !self->active && FindOvermind(self)) {
        vec3_t dirToTarget;

        self->active = true;
        self->target_ent = attacker;
        self->timestamp = level.time + HIVE_REPEAT;

        VectorSubtract(attacker->s.pos.trBase, self->s.pos.trBase, dirToTarget);
        VectorNormalize(dirToTarget);
        vectoangles(dirToTarget, self->turretAim);

        //fire at target
        idSGameWeapons::FireWeapon(self);
    }

    AGeneric_Die(self, inflictor, attacker, damage, mod);
}

#define HOVEL_TRACE_DEPTH 128.0f

/*
================
idSGameBuildable::AHovel_Blocked

Is this hovel entrance blocked?
================
*/
bool idSGameBuildable::AHovel_Blocked(gentity_t *hovel, gentity_t *player,
                                      bool provideExit) {
    vec3_t    forward, normal, origin, start, end, angles, hovelMaxs;
    vec3_t    mins, maxs;
    float32     displacement;
    trace_t   tr;

    bggame->BuildableBoundingBox(BA_A_HOVEL, nullptr, hovelMaxs);
    bggame->ClassBoundingBox((class_t)player->client->ps.stats[ STAT_CLASS ],
                             mins, maxs, nullptr, nullptr, nullptr);

    VectorCopy(hovel->s.origin2, normal);
    AngleVectors(hovel->s.angles, forward, nullptr, nullptr);
    VectorInverse(forward);

    displacement = VectorMaxComponent(maxs) + VectorMaxComponent(
                       hovelMaxs) + 1.0f;

    VectorMA(hovel->s.origin, displacement, forward, origin);

    VectorCopy(hovel->s.origin, start);
    VectorCopy(origin, end);

    // see if there's something between the hovel and its exit
    // (eg built right up against a wall)
    trap_Trace(&tr, start, nullptr, nullptr, end, player->s.number,
               MASK_PLAYERSOLID);

    if(tr.fraction < 1.0f) {
        return true;
    }

    vectoangles(forward, angles);

    VectorMA(origin, HOVEL_TRACE_DEPTH, normal, start);

    //compute a place up in the air to start the real trace
    trap_Trace(&tr, origin, mins, maxs, start, player->s.number,
               MASK_PLAYERSOLID);

    VectorMA(origin, (HOVEL_TRACE_DEPTH * tr.fraction) - 1.0f, normal, start);
    VectorMA(origin, -HOVEL_TRACE_DEPTH, normal, end);

    trap_Trace(&tr, start, mins, maxs, end, player->s.number,
               MASK_PLAYERSOLID);

    VectorCopy(tr.endpos, origin);

    trap_Trace(&tr, origin, mins, maxs, origin, player->s.number,
               MASK_PLAYERSOLID);

    if(tr.fraction < 1.0f) {
        return true;
    }

    if(provideExit) {
        idSGameUtils::SetOrigin(player, origin);
        VectorCopy(origin, player->client->ps.origin);
        // nudge
        VectorMA(normal, 200.0f, forward, player->client->ps.velocity);
        idSGameClient::SetClientViewAngle(player, angles);
    }

    return false;
}

/*
================
APropHovel_Blocked

Wrapper to test a hovel placement for validity
================
*/

bool idSGameBuildable::APropHovel_Blocked(vec3_t origin, vec3_t angles,
        vec3_t normal, gentity_t *player) {
    gentity_t hovel;

    VectorCopy(origin, hovel.s.origin);
    VectorCopy(angles, hovel.s.angles);
    VectorCopy(normal, hovel.s.origin2);

    return AHovel_Blocked(&hovel, player, false);
}


/*
================
AHovel_Use

Called when an alien uses a hovel
================
*/
void idSGameBuildable::AHovel_Use(gentity_t *self, gentity_t *other,
                                  gentity_t *activator) {
    vec3_t  hovelOrigin, hovelAngles, inverseNormal;

    if(self->spawned && FindOvermind(self)) {
        if(self->active) {
            //this hovel is in use
            idSGameUtils::TriggerMenu(activator->client->ps.clientNum,
                                      MN_A_HOVEL_OCCUPIED);
        } else if(((activator->client->ps.stats[ STAT_CLASS ] ==
                    PCL_ALIEN_BUILDER0) ||
                   (activator->client->ps.stats[ STAT_CLASS ] == PCL_ALIEN_BUILDER0_UPG)) &&
                  activator->health > 0 && self->health > 0) {
            if(AHovel_Blocked(self, activator, false)) {
                //you can get in, but you can't get out
                idSGameUtils::TriggerMenu(activator->client->ps.clientNum,
                                          MN_A_HOVEL_BLOCKED);
                return;
            }

            self->active = true;
            SetBuildableAnim(self, BANIM_ATTACK1, false);

            //prevent lerping
            activator->client->ps.eFlags ^= EF_TELEPORT_BIT;
            activator->client->ps.eFlags |= EF_NODRAW;
            idSGameActive::UnlaggedClear(activator);

            // Cancel pending suicides
            activator->suicideTime = 0;

            activator->client->ps.stats[ STAT_STATE ] |= SS_HOVELING;
            activator->client->hovel = self;
            self->builder = activator;

            VectorCopy(self->s.pos.trBase, hovelOrigin);
            VectorMA(hovelOrigin, 128.0f, self->s.origin2, hovelOrigin);

            VectorCopy(self->s.origin2, inverseNormal);
            VectorInverse(inverseNormal);
            vectoangles(inverseNormal, hovelAngles);

            VectorCopy(activator->s.pos.trBase, activator->client->hovelOrigin);

            idSGameUtils::SetOrigin(activator, hovelOrigin);
            VectorCopy(hovelOrigin, activator->client->ps.origin);
            idSGameClient::SetClientViewAngle(activator, hovelAngles);
        }
    }
}


/*
================
AHovel_Think

Think for alien hovel
================
*/
void idSGameBuildable::AHovel_Think(gentity_t *self) {
    self->powered = level.overmindPresent;
    self->nextthink = level.time + 200;

    AGeneric_CreepCheck(self);

    if(self->spawned) {
        if(self->active) {
            SetIdleBuildableAnim(self, BANIM_IDLE2);
        } else {
            SetIdleBuildableAnim(self, BANIM_IDLE1);
        }
    }
}

/*
================
AHovel_Die

Die for alien hovel
================
*/
void idSGameBuildable::AHovel_Die(gentity_t *self, gentity_t *inflictor,
                                  gentity_t *attacker, sint damage, sint mod) {
    //if the hovel is occupied free the occupant
    if(self->active) {
        gentity_t *builder = self->builder;
        vec3_t    newOrigin;
        vec3_t    newAngles;

        VectorCopy(self->s.angles, newAngles);
        newAngles[ ROLL ] = 0;

        VectorCopy(self->s.origin, newOrigin);
        VectorMA(newOrigin, 1.0f, self->s.origin2, newOrigin);

        //prevent lerping
        builder->client->ps.eFlags ^= EF_TELEPORT_BIT;
        builder->client->ps.eFlags &= ~EF_NODRAW;
        idSGameActive::UnlaggedClear(builder);

        idSGameUtils::SetOrigin(builder, newOrigin);
        VectorCopy(newOrigin, builder->client->ps.origin);
        idSGameClient::SetClientViewAngle(builder, newAngles);

        //client leaves hovel
        builder->client->ps.stats[ STAT_STATE ] &= ~SS_HOVELING;
    }

    AGeneric_Die(self, inflictor, attacker, damage, mod);
    self->nextthink = level.time + 100;
}





//==================================================================================




/*
================
ABooster_Touch

Called when an alien touches a booster
================
*/
void idSGameBuildable::ABooster_Touch(gentity_t *self, gentity_t *other,
                                      trace_t *trace) {
    gclient_t *client = other->client;

    if(!self->spawned || self->health <= 0) {
        return;
    }

    if(!FindOvermind(self)) {
        return;
    }

    if(!client) {
        return;
    }

    if(client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS) {
        return;
    }

    client->ps.stats[ STAT_STATE ] |= SS_BOOSTED;
    client->boostedTime = level.time;
}




//==================================================================================

#define TRAPPER_ACCURACY 10 // lower is better

/*
================
ATrapper_FireOnEnemy

Used by ATrapper_Think to fire at enemy
================
*/
void idSGameBuildable::ATrapper_FireOnEnemy(gentity_t *self,
        sint firespeed, float32 range) {
    gentity_t *enemy = self->enemy;
    vec3_t dirToTarget;
    vec3_t halfAcceleration, thirdJerk;
    float32 distanceToTarget = bggame->Buildable((buildable_t)
                               self->s.modelindex)->turretRange;
    sint lowMsec = 0;
    sint highMsec = (sint)((((distanceToTarget * LOCKBLOB_SPEED) +
                             (distanceToTarget * bggame->Class((class_t)
                                     enemy->client->ps.stats[ STAT_CLASS ])->speed)) / (LOCKBLOB_SPEED *
                                             LOCKBLOB_SPEED)) * 1000.0f);

    VectorScale(enemy->acceleration, 1.0f / 2.0f, halfAcceleration);
    VectorScale(enemy->jerk, 1.0f / 3.0f, thirdJerk);

    // highMsec and lowMsec can only move toward
    // one another, so the loop must terminate
    while(highMsec - lowMsec > TRAPPER_ACCURACY) {
        sint   partitionMsec = (highMsec + lowMsec) / 2;
        float32 time = (float32)partitionMsec / 1000.0f;
        float32 projectileDistance = LOCKBLOB_SPEED * time;

        VectorMA(enemy->s.pos.trBase, time, enemy->s.pos.trDelta, dirToTarget);
        VectorMA(dirToTarget, time * time, halfAcceleration, dirToTarget);
        VectorMA(dirToTarget, time * time * time, thirdJerk, dirToTarget);
        VectorSubtract(dirToTarget, self->s.pos.trBase, dirToTarget);
        distanceToTarget = VectorLength(dirToTarget);

        if(projectileDistance < distanceToTarget) {
            lowMsec = partitionMsec;
        } else if(projectileDistance > distanceToTarget) {
            highMsec = partitionMsec;
        } else if(projectileDistance == distanceToTarget) {
            break;    // unlikely to happen
        }
    }

    VectorNormalize(dirToTarget);
    vectoangles(dirToTarget, self->turretAim);

    //fire at target
    idSGameWeapons::FireWeapon(self);
    SetBuildableAnim(self, BANIM_ATTACK1, false);
    self->count = level.time + firespeed;
}

/*
================
ATrapper_CheckTarget

Used by ATrapper_Think to check enemies for validity
================
*/
bool idSGameBuildable::ATrapper_CheckTarget(gentity_t *self,
        gentity_t *target, sint range) {
    vec3_t    distance;
    trace_t   trace;

    if(!target) {  // Do we have a target?
        return false;
    }

    if(!target->inuse) {  // Does the target still exist?
        return false;
    }

    if(target == self) {  // is the target us?
        return false;
    }

    if(!target->client) {  // is the target a bot or player?
        return false;
    }

    if(target->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS) {  // one of us?
        return false;
    }

    if(target->client->sess.spectatorState !=
            SPECTATOR_NOT) {  // is the target alive?
        return false;
    }

    if(target->health <= 0) {  // is the target still alive?
        return false;
    }

    if(target->client->ps.stats[ STAT_STATE ] & SS_BLOBLOCKED) {  // locked?
        return false;
    }

    VectorSubtract(target->r.currentOrigin, self->r.currentOrigin, distance);

    if(VectorLength(distance) > range) {    // is the target within range?
        return false;
    }

    //only allow a narrow field of "vision"
    VectorNormalize(distance);   //is now direction of target

    if(DotProduct(distance, self->s.origin2) < LOCKBLOB_DOT) {
        return false;
    }

    trap_Trace(&trace, self->s.pos.trBase, nullptr, nullptr,
               target->s.pos.trBase, self->s.number, MASK_SHOT);

    if(trace.contents & CONTENTS_SOLID) {  // can we see the target?
        return false;
    }

    return true;
}

/*
================
ATrapper_FindEnemy

Used by ATrapper_Think to locate enemy gentities
================
*/
void idSGameBuildable::ATrapper_FindEnemy(gentity_t *ent, sint range) {
    gentity_t *target;

    //iterate through entities
    for(target = g_entities; target < &g_entities[ level.num_entities ];
            target++) {
        //if target is not valid keep searching
        if(!ATrapper_CheckTarget(ent, target, range)) {
            continue;
        }

        //we found a target
        ent->enemy = target;
        return;
    }

    //couldn't find a target
    ent->enemy = nullptr;
}

/*
================
ATrapper_Think

think function for Alien Defense
================
*/
void idSGameBuildable::ATrapper_Think(gentity_t *self) {
    sint range = bggame->Buildable((buildable_t)
                                   self->s.modelindex)->turretRange;
    sint firespeed = bggame->Buildable((buildable_t)
                                       self->s.modelindex)->turretFireSpeed;

    self->powered = level.overmindPresent;
    self->nextthink = level.time + bggame->Buildable((buildable_t)
                      self->s.modelindex)->nextthink;

    AGeneric_CreepCheck(self);

    if(self->spawned && FindOvermind(self)) {
        //if the current target is not valid find a new one
        if(!ATrapper_CheckTarget(self, self->enemy, range)) {
            ATrapper_FindEnemy(self, range);
        }

        //if a new target cannot be found don't do anything
        if(!self->enemy) {
            return;
        }

        //if we are pointing at our target and we can fire shoot it
        if(self->count < level.time) {
            ATrapper_FireOnEnemy(self, firespeed, range);
        }
    }
}



//==================================================================================



/*
================
HRepeater_Think

Think for human power repeater
================
*/
void idSGameBuildable::HRepeater_Think(gentity_t *self) {
    sint       i;
    bool  reactor = false;
    gentity_t *ent;

    if(self->spawned) {
        //iterate through entities
        for(i = 1, ent = g_entities + i; i < level.num_entities; i++, ent++) {
            if(ent->s.eType != ET_BUILDABLE) {
                continue;
            }

            if(ent->s.modelindex == BA_H_REACTOR && ent->spawned) {
                reactor = true;
            }
        }
    }

    if(NumberOfDependants(self) == 0) {
        //if no dependants for x seconds then disappear
        if(self->count < 0) {
            self->count = level.time;
        } else if(self->count > 0 &&
                  ((level.time - self->count) > REPEATER_INACTIVE_TIME)) {
            idSGameCombat::Damage(self, nullptr, nullptr, nullptr, nullptr,
                                  self->health, 0, MOD_SUICIDE);
        }
    } else {
        self->count = -1;
    }

    self->powered = reactor;

    self->nextthink = level.time + POWER_REFRESH_TIME;
}

/*
================
HRepeater_Use

Use for human power repeater
================
*/
void idSGameBuildable::HRepeater_Use(gentity_t *self, gentity_t *other,
                                     gentity_t *activator) {
    if(self->health <= 0 || !self->spawned) {
        return;
    }

    if(other && other->client) {
        idSGameWeapons::GiveClientMaxAmmo(other, true);
    }
}

/*
================
HReactor_Think

Think function for Human Reactor
================
*/
void idSGameBuildable::HReactor_Think(gentity_t *self) {
    sint       entityList[ MAX_GENTITIES ];
    vec3_t    range = { REACTOR_ATTACK_RANGE, REACTOR_ATTACK_RANGE, REACTOR_ATTACK_RANGE };
    vec3_t    dccrange = { REACTOR_ATTACK_DCC_RANGE, REACTOR_ATTACK_DCC_RANGE, REACTOR_ATTACK_DCC_RANGE };
    vec3_t    mins, maxs;
    sint       i, num;
    gentity_t *enemy, *tent;

    if(self->dcc) {
        VectorAdd(self->s.origin, dccrange, maxs);
        VectorSubtract(self->s.origin, dccrange, mins);
    } else {
        VectorAdd(self->s.origin, range, maxs);
        VectorSubtract(self->s.origin, range, mins);
    }

    if(self->spawned && (self->health > 0)) {
        bool fired = false;

        // Creates a tesla trail for every target
        num = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

        for(i = 0; i < num; i++) {
            enemy = &g_entities[ entityList[ i ] ];

            if(!enemy->client ||
                    enemy->client->ps.stats[ STAT_TEAM ] != TEAM_ALIENS) {
                continue;
            }

            tent = idSGameUtils::TempEntity(enemy->s.pos.trBase, EV_TESLATRAIL);
            tent->s.generic1 = self->s.number; //src
            tent->s.clientNum = enemy->s.number; //dest
            VectorCopy(self->s.pos.trBase, tent->s.origin2);
            fired = true;
        }

        // Actual damage is done by radius
        if(fired) {
            self->timestamp = level.time;

            if(self->dcc) {
                idSGameCombat::SelectiveRadiusDamage(self->s.pos.trBase, self,
                                                     REACTOR_ATTACK_DCC_DAMAGE, REACTOR_ATTACK_DCC_RANGE, self, MOD_REACTOR,
                                                     TEAM_HUMANS);
            } else {
                idSGameCombat::SelectiveRadiusDamage(self->s.pos.trBase, self,
                                                     REACTOR_ATTACK_DAMAGE, REACTOR_ATTACK_RANGE, self, MOD_REACTOR,
                                                     TEAM_HUMANS);
            }
        }
    }

    if(self->dcc) {
        self->nextthink = level.time + REACTOR_ATTACK_DCC_REPEAT;
    } else {
        self->nextthink = level.time + REACTOR_ATTACK_REPEAT;
    }
}

//==================================================================================



/*
================
HArmoury_Activate

Called when a human activates an Armoury
================
*/
void idSGameBuildable::HArmoury_Activate(gentity_t *self, gentity_t *other,
        gentity_t *activator) {
    if(self->spawned) {
        //only humans can activate this
        if(activator->client->ps.stats[ STAT_TEAM ] != TEAM_HUMANS) {
            return;
        }

        //if this is powered then call the armoury menu
        if(self->powered) {
            idSGameUtils::TriggerMenu(activator->client->ps.clientNum, MN_H_ARMOURY);
        } else {
            idSGameUtils::TriggerMenu(activator->client->ps.clientNum,
                                      MN_H_NOTPOWERED);
        }
    }
}

/*
================
HArmoury_Think

Think for armoury
================
*/
void idSGameBuildable::HArmoury_Think(gentity_t *self) {
    //make sure we have power
    self->nextthink = level.time + POWER_REFRESH_TIME;

    self->powered = FindPower(self);
}

//==================================================================================

/*
================
HDCC_Think

Think for dcc
================
*/
void idSGameBuildable::HDCC_Think(gentity_t *self) {
    //make sure we have power
    self->nextthink = level.time + POWER_REFRESH_TIME;

    self->powered = FindPower(self);
}

//==================================================================================

/*
================
HMedistat_Die

Die function for Human Medistation
================
*/
void idSGameBuildable::HMedistat_Die(gentity_t *self, gentity_t *inflictor,
                                     gentity_t *attacker, sint damage, sint mod) {
    //clear target's healing flag
    if(self->enemy && self->enemy->client) {
        self->enemy->client->ps.stats[ STAT_STATE ] &= ~SS_HEALING_ACTIVE;
    }

    HSpawn_Die(self, inflictor, attacker, damage, mod);
}

/*
================
HMedistat_Think

think function for Human Medistation
================
*/
void idSGameBuildable::HMedistat_Think(gentity_t *self) {
    sint       entityList[ MAX_GENTITIES ];
    vec3_t    mins, maxs;
    sint       i, num;
    gentity_t *player;
    bool  occupied = false;

    self->nextthink = level.time + bggame->Buildable((buildable_t)
                      self->s.modelindex)->nextthink;

    //clear target's healing flag
    if(self->enemy && self->enemy->client) {
        self->enemy->client->ps.stats[ STAT_STATE ] &= ~SS_HEALING_ACTIVE;
    }

    //make sure we have power
    if(!(self->powered = FindPower(self))) {
        if(self->active) {
            SetBuildableAnim(self, BANIM_CONSTRUCT2, true);
            SetIdleBuildableAnim(self, BANIM_IDLE1);
            self->active = false;
            self->enemy = nullptr;
        }

        self->nextthink = level.time + POWER_REFRESH_TIME;
        return;
    }

    if(self->spawned) {
        VectorAdd(self->s.origin, self->r.maxs, maxs);
        VectorAdd(self->s.origin, self->r.mins, mins);

        mins[ 2 ] += fabs(self->r.mins[ 2 ]) + self->r.maxs[ 2 ];
        maxs[ 2 ] += 60; //player height

        //if active use the healing idle
        if(self->active) {
            SetIdleBuildableAnim(self, BANIM_IDLE2);
        }

        //check if a previous occupier is still here
        num = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

        for(i = 0; i < num; i++) {
            player = &g_entities[ entityList[ i ] ];

            //remove poison from everyone, not just the healed player
            if(player->client &&
                    player->client->ps.stats[ STAT_STATE ] & SS_POISONED) {
                player->client->ps.stats[ STAT_STATE ] &= ~SS_POISONED;
            }

            if(self->enemy == player && player->client &&
                    player->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS &&
                    player->health < player->client->ps.stats[ STAT_MAX_HEALTH ] &&
                    player->client->ps.pm_type != PM_DEAD) {
                occupied = true;
                player->client->ps.stats[ STAT_STATE ] |= SS_HEALING_ACTIVE;
            }
        }

        if(!occupied) {
            self->enemy = nullptr;

            //look for something to heal
            for(i = 0; i < num; i++) {
                player = &g_entities[ entityList[ i ] ];

                if(player->client &&
                        player->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS) {
                    if((player->health < player->client->ps.stats[ STAT_MAX_HEALTH ] ||
                            player->client->ps.stats[ STAT_STAMINA ] < MAX_STAMINA) &&
                            player->client->ps.pm_type != PM_DEAD) {
                        self->enemy = player;

                        //start the heal anim
                        if(!self->active) {
                            SetBuildableAnim(self, BANIM_ATTACK1, false);
                            self->active = true;
                            player->client->ps.stats[ STAT_STATE ] |= SS_HEALING_ACTIVE;
                        }
                    } else if(!bggame->InventoryContainsUpgrade(UP_MEDKIT,
                              player->client->ps.stats)) {
                        bggame->AddUpgradeToInventory(UP_MEDKIT, player->client->ps.stats);
                    }
                }
            }
        }

        //nothing left to heal so go back to idling
        if(!self->enemy && self->active) {
            SetBuildableAnim(self, BANIM_CONSTRUCT2, true);
            SetIdleBuildableAnim(self, BANIM_IDLE1);

            self->active = false;
        } else if(self->enemy && self->enemy->client) { //heal!
            if(self->enemy->client->ps.stats[ STAT_STAMINA ] <  MAX_STAMINA) {
                self->enemy->client->ps.stats[ STAT_STAMINA ] += STAMINA_MEDISTAT_RESTORE;
            }

            if(self->enemy->client->ps.stats[ STAT_STAMINA ] > MAX_STAMINA) {
                self->enemy->client->ps.stats[ STAT_STAMINA ] = MAX_STAMINA;
            }

            self->enemy->health++;

            //if they're completely healed, give them a medkit
            if(self->enemy->health >=
                    self->enemy->client->ps.stats[ STAT_MAX_HEALTH ]) {
                self->enemy->health =  self->enemy->client->ps.stats[ STAT_MAX_HEALTH ];

                if(!bggame->InventoryContainsUpgrade(UP_MEDKIT,
                                                     self->enemy->client->ps.stats)) {
                    bggame->AddUpgradeToInventory(UP_MEDKIT, self->enemy->client->ps.stats);
                }
            }
        }
    }
}




//==================================================================================




/*
================
HMGTurret_CheckTarget

Used by HMGTurret_Think to check enemies for validity
================
*/
bool idSGameBuildable::HMGTurret_CheckTarget(gentity_t *self,
        gentity_t *target, bool los_check) {
    trace_t   tr;
    vec3_t    dir, end;

    if(!target || target->health <= 0 || !target->client ||
            target->client->pers.teamSelection != TEAM_ALIENS ||
            (target->client->ps.stats[ STAT_STATE ] & SS_HOVELING)) {
        return false;
    }

    if(!los_check) {
        return true;
    }

    // Accept target if we can line-trace to it
    VectorSubtract(target->s.pos.trBase, self->s.pos.trBase, dir);
    VectorNormalize(dir);
    VectorMA(self->s.pos.trBase, MGTURRET_RANGE, dir, end);
    trap_Trace(&tr, self->s.pos.trBase, nullptr, nullptr, end, self->s.number,
               MASK_SHOT);
    return tr.entityNum == target - g_entities;
}


/*
================
HMGTurret_TrackEnemy

Used by HMGTurret_Think to track enemy location
================
*/
bool idSGameBuildable::HMGTurret_TrackEnemy(gentity_t *self) {
    vec3_t  dirToTarget, dttAdjusted, angleToTarget, angularDiff, xNormal;
    vec3_t  refNormal = { 0.0f, 0.0f, 1.0f };
    float32   temp, rotAngle, angularSpeed;

    angularSpeed = self->lev1Grabbed ? MGTURRET_ANGULARSPEED_GRAB :
                   MGTURRET_ANGULARSPEED;

    VectorSubtract(self->enemy->s.pos.trBase, self->s.pos.trBase, dirToTarget);
    VectorNormalize(dirToTarget);

    CrossProduct(self->s.origin2, refNormal, xNormal);
    VectorNormalize(xNormal);
    rotAngle = RAD2DEG(acos(DotProduct(self->s.origin2, refNormal)));
    RotatePointAroundVector(dttAdjusted, xNormal, dirToTarget, rotAngle);

    vectoangles(dttAdjusted, angleToTarget);

    angularDiff[ PITCH ] = AngleSubtract(self->s.angles2[ PITCH ],
                                         angleToTarget[ PITCH ]);
    angularDiff[ YAW ] = AngleSubtract(self->s.angles2[ YAW ],
                                       angleToTarget[ YAW ]);

    //if not pointing at our target then move accordingly
    if(angularDiff[ PITCH ] < 0 && angularDiff[ PITCH ] < (-angularSpeed)) {
        self->s.angles2[ PITCH ] += angularSpeed;
    } else if(angularDiff[ PITCH ] > 0 &&
              angularDiff[ PITCH ] > angularSpeed) {
        self->s.angles2[ PITCH ] -= angularSpeed;
    } else {
        self->s.angles2[ PITCH ] = angleToTarget[ PITCH ];
    }

    //disallow vertical movement past a certain limit
    temp = fabs(self->s.angles2[ PITCH ]);

    if(temp > 180) {
        temp -= 360;
    }

    if(temp < -MGTURRET_VERTICALCAP) {
        self->s.angles2[ PITCH ] = (-360) + MGTURRET_VERTICALCAP;
    }

    //if not pointing at our target then move accordingly
    if(angularDiff[ YAW ] < 0 && angularDiff[ YAW ] < (-angularSpeed)) {
        self->s.angles2[ YAW ] += angularSpeed;
    } else if(angularDiff[ YAW ] > 0 && angularDiff[ YAW ] > angularSpeed) {
        self->s.angles2[ YAW ] -= angularSpeed;
    } else {
        self->s.angles2[ YAW ] = angleToTarget[ YAW ];
    }

    AngleVectors(self->s.angles2, dttAdjusted, nullptr, nullptr);
    RotatePointAroundVector(dirToTarget, xNormal, dttAdjusted, -rotAngle);
    vectoangles(dirToTarget, self->turretAim);

    //fire if target is within accuracy
    return (fabs(angularDiff[ YAW ]) - angularSpeed <=
            MGTURRET_ACCURACY_TO_FIRE) &&
           (fabs(angularDiff[ PITCH ]) - angularSpeed <=
            MGTURRET_ACCURACY_TO_FIRE);
}


/*
================
HMGTurret_FindEnemy

Used by HMGTurret_Think to locate enemy gentities
================
*/
void idSGameBuildable::HMGTurret_FindEnemy(gentity_t *self) {
    sint       entityList[ MAX_GENTITIES ];
    vec3_t    range;
    vec3_t    mins, maxs;
    sint       i, num;
    gentity_t *target;

    if(self->enemy) {
        self->enemy->targeted = nullptr;
    }

    self->enemy = nullptr;

    // Look for targets in a box around the turret
    VectorSet(range, MGTURRET_RANGE, MGTURRET_RANGE, MGTURRET_RANGE);
    VectorAdd(self->s.origin, range, maxs);
    VectorSubtract(self->s.origin, range, mins);
    num = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

    for(i = 0; i < num; i++) {
        target = &g_entities[ entityList[ i ] ];

        if(!HMGTurret_CheckTarget(self, target, true)) {
            continue;
        }

        self->enemy = target;
        self->enemy->targeted = self;
        return;
    }
}


/*
================
HMGTurret_Think

Think function for MG turret
================
*/
void idSGameBuildable::HMGTurret_Think(gentity_t *self) {
    float32 temp;
    self->nextthink = level.time +
                      bggame->Buildable((buildable_t)self->s.modelindex)->nextthink;

    // Turn off client side muzzle flashes
    self->s.eFlags &= ~EF_FIRING;

    // If not spawned don't do anything
    if(!self->spawned) {
        return;
    }

    // If not powered droop forward
    if(!(self->powered = FindPower(self))) {
        //unwind the turret pitch
        temp = fabs(self->s.angles2[ PITCH ]);

        if(temp > 180) {
            temp -= 360;
        }

        //pitch down a little
        if(temp < MGTURRET_VERTICALCAP) {
            temp += MGTURRET_DROOP_RATE;
        }

        //are we already aimed all the way down?
        if(temp >= MGTURRET_VERTICALCAP) {
            //we are all the way down
            self->s.angles2[ PITCH ] = MGTURRET_VERTICALCAP;
            self->nextthink = level.time + POWER_REFRESH_TIME;
        } else {
            self->s.angles2[ PITCH ] = temp;
        }

        return;
    }

    // If the current target is not valid find a new enemy
    if(!HMGTurret_CheckTarget(self, self->enemy, true)) {
        self->active = false;
        self->turretSpinupTime = -1;
        HMGTurret_FindEnemy(self);
    }

    if(!self->enemy) {
        return;
    }

    // Track until we can hit the target
    if(!HMGTurret_TrackEnemy(self)) {
        self->active = false;
        self->turretSpinupTime = -1;
        return;
    }

    // Update spin state
    if(!self->active && self->count < level.time) {
        self->active = true;
        self->turretSpinupTime = level.time + MGTURRET_SPINUP_TIME;
        idSGameUtils::AddEvent(self, EV_MGTURRET_SPINUP, 0);
    }

    // Not firing or haven't spun up yet
    if(!self->active || self->turretSpinupTime > level.time) {
        return;
    }

    // Fire repeat delay
    if(self->count > level.time) {
        return;
    }

    idSGameWeapons::FireWeapon(self);
    self->s.eFlags |= EF_FIRING;
    self->count = level.time + bggame->Buildable((buildable_t)
                  self->s.modelindex)->turretFireSpeed;
    idSGameUtils::AddEvent(self, EV_FIRE_WEAPON, 0);
    SetBuildableAnim(self, BANIM_ATTACK1, false);
}




//==================================================================================




/*
================
HTeslaGen_Think

Think function for Tesla Generator
================
*/
void idSGameBuildable::HTeslaGen_Think(gentity_t *self) {
    self->nextthink = level.time + bggame->Buildable((buildable_t)
                      self->s.modelindex)->nextthink;

    //if not powered don't do anything and check again for power next think
    if(!(self->powered = FindPower(self))) {
        self->s.eFlags &= ~EF_FIRING;
        self->nextthink = level.time + POWER_REFRESH_TIME;
        return;
    }

    if(self->spawned && self->count < level.time) {
        vec3_t range, mins, maxs;
        sint entityList[ MAX_GENTITIES ], i, num;

        // Communicates firing state to client
        self->s.eFlags &= ~EF_FIRING;

        VectorSet(range, TESLAGEN_RANGE, TESLAGEN_RANGE, TESLAGEN_RANGE);
        VectorAdd(self->s.origin, range, maxs);
        VectorSubtract(self->s.origin, range, mins);

        // Attack nearby Aliens
        num = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

        for(i = 0; i < num; i++) {
            self->enemy = &g_entities[ entityList[ i ] ];

            if(self->enemy->client && self->enemy->health > 0 &&
                    self->enemy->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS &&
                    Distance(self->enemy->s.pos.trBase,
                             self->s.pos.trBase) <= TESLAGEN_RANGE) {
                idSGameWeapons::FireWeapon(self);
            }
        }

        self->enemy = nullptr;

        if(self->s.eFlags & EF_FIRING) {
            idSGameUtils::AddEvent(self, EV_FIRE_WEAPON, 0);

            //doesn't really need an anim
            //SetBuildableAnim( self, BANIM_ATTACK1, false );

            self->count = level.time + TESLAGEN_REPEAT;
        }
    }
}




//==================================================================================




/*
================
HSpawn_Disappear

Called when a human spawn is destroyed before it is spawned
think function
================
*/
void idSGameBuildable::HSpawn_Disappear(gentity_t *self) {
    vec3_t  dir;

    // we don't have a valid direction, so just point straight up
    dir[ 0 ] = dir[ 1 ] = 0;
    dir[ 2 ] = 1;

    self->s.eFlags |= EF_NODRAW; //don't draw the model once its destroyed
    self->timestamp = level.time;

    idSGameUtils::FreeEntity(self);
}


/*
================
HSpawn_blast

Called when a human spawn explodes
think function
================
*/
void idSGameBuildable::HSpawn_Blast(gentity_t *self) {
    vec3_t  dir;

    // we don't have a valid direction, so just point straight up
    dir[ 0 ] = dir[ 1 ] = 0;
    dir[ 2 ] = 1;

    self->timestamp = level.time;

    //do some radius damage
    idSGameCombat::RadiusDamage(self->s.pos.trBase, self, self->splashDamage,
                                self->splashRadius, self, self->splashMethodOfDeath);

    // begin freeing build points
    QueueBuildPoints(self);
    // turn into an explosion
    self->s.eType = (entityType_t)(ET_EVENTS + EV_HUMAN_BUILDABLE_EXPLOSION);
    self->freeAfterEvent = true;
    idSGameUtils::AddEvent(self, EV_HUMAN_BUILDABLE_EXPLOSION, DirToByte(dir));
}


/*
================
HSpawn_die

Called when a human spawn dies
================
*/
void idSGameBuildable::HSpawn_Die(gentity_t *self, gentity_t *inflictor,
                                  gentity_t *attacker, sint damage, sint mod) {
    idSGameCombat::RewardAttackers(self);
    SetBuildableAnim(self, BANIM_DESTROY1, true);
    SetIdleBuildableAnim(self, BANIM_DESTROYED);

    self->die = nullDieFunction;
    self->powered = false; //free up power
    self->s.eFlags &= ~EF_FIRING; //prevent any firing effects

    if(self->spawned) {
        self->think = HSpawn_Blast;
        self->nextthink = level.time + HUMAN_DETONATION_DELAY;
    } else {
        self->think = HSpawn_Disappear;
        self->nextthink = level.time; //blast immediately
    }

    if(attacker && attacker->client) {
        idSGameCombat::LogDestruction(self, attacker, mod);
    }
}

/*
================
HSpawn_Think

Think for human spawn
================
*/
void idSGameBuildable::HSpawn_Think(gentity_t *self) {
    gentity_t *ent;

    // spawns work without power
    self->powered = true;

    if(self->spawned) {
        //only suicide if at rest
        if(self->s.groundEntityNum) {
            if((ent = CheckSpawnPoint(self->s.number, self->s.origin, self->s.origin2,
                                      BA_H_SPAWN, nullptr)) != nullptr) {
                // If the thing blocking the spawn is a buildable, kill it.
                // If it's part of the map, kill self.
                if(ent->s.eType == ET_BUILDABLE) {
                    idSGameCombat::Damage(ent, nullptr, nullptr, nullptr, nullptr, 10000, 0,
                                          MOD_SUICIDE);
                    SetBuildableAnim(self, BANIM_SPAWN1, true);
                } else if(ent->s.number == ENTITYNUM_WORLD || ent->s.eType == ET_MOVER) {
                    idSGameCombat::Damage(self, nullptr, nullptr, nullptr, nullptr, 10000, 0,
                                          MOD_SUICIDE);
                    return;
                }

                if(ent->s.eType == ET_CORPSE) {
                    idSGameUtils::FreeEntity(ent);    //quietly remove
                }
            }
        }
    }

    self->nextthink = level.time + bggame->Buildable((buildable_t)
                      self->s.modelindex)->nextthink;
}

/*
============
idSGameBuildable::QueueBuildPoints
============
*/
void idSGameBuildable::QueueBuildPoints(gentity_t *self) {
    gentity_t *killer = &g_entities[ self->killedBy ];

    if(self->killedBy != ENTITYNUM_NONE && killer->client &&
            //there's a tiny chance that a turret could hit a tube
            //and kill it
            killer->client->ps.stats[ STAT_TEAM ] == self->buildableTeam) {
        return;    //don't take away build points if killed by a teammate,
    }

    //mostly happens from MOD_NOCREEP

    switch(self->buildableTeam) {
        default:
        case TEAM_NONE:
            return;

        case TEAM_ALIENS:
            if(!level.alienBuildPointQueue) {
                level.alienNextQueueTime = level.time + g_alienBuildQueueTime.integer;
            }

            level.alienBuildPointQueue +=
                bggame->Buildable((buildable_t)self->s.modelindex)->buildPoints;
            break;

        case TEAM_HUMANS:
            if(!level.humanBuildPointQueue) {
                level.humanNextQueueTime = level.time + g_humanBuildQueueTime.integer;
            }

            level.humanBuildPointQueue +=
                bggame->Buildable((buildable_t)self->s.modelindex)->buildPoints;
            break;
    }
}

/*
============
G_BuildableTouchTriggers

Find all trigger entities that a buildable touches.
============
*/
void idSGameBuildable::BuildableTouchTriggers(gentity_t *ent) {
    sint       i, num;
    sint       touch[ MAX_GENTITIES ];
    gentity_t *hit;
    trace_t   trace;
    vec3_t    mins, maxs;
    vec3_t    bmins, bmaxs;
    static    vec3_t range = { 10, 10, 10 };

    // dead buildables don't activate triggers!
    if(ent->health <= 0) {
        return;
    }

    bggame->BuildableBoundingBox((buildable_t)ent->s.modelindex, bmins, bmaxs);

    VectorAdd(ent->s.origin, bmins, mins);
    VectorAdd(ent->s.origin, bmaxs, maxs);

    VectorSubtract(mins, range, mins);
    VectorAdd(maxs, range, maxs);

    num = trap_EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);

    VectorAdd(ent->s.origin, bmins, mins);
    VectorAdd(ent->s.origin, bmaxs, maxs);

    for(i = 0; i < num; i++) {
        hit = &g_entities[ touch[ i ] ];

        if(!hit->touch) {
            continue;
        }

        if(!(hit->r.contents & CONTENTS_TRIGGER)) {
            continue;
        }

        //ignore buildables not yet spawned
        if(!ent->spawned) {
            continue;
        }

        if(!trap_EntityContact(mins, maxs, hit)) {
            continue;
        }

        ::memset(&trace, 0, sizeof(trace));

        if(hit->touch) {
            hit->touch(hit, ent, &trace);
        }
    }
}


/*
===============
idSGameBuildable::BuildableThink

General think function for buildables
===============
*/
void idSGameBuildable::BuildableThink(gentity_t *ent, sint msec) {
    sint bHealth = bggame->Buildable((buildable_t)ent->s.modelindex)->health;
    sint bRegen = bggame->Buildable((buildable_t)ent->s.modelindex)->regenRate;
    sint bTime = bggame->Buildable((buildable_t)ent->s.modelindex)->buildTime;

    //toggle spawned flag for buildables
    if(!ent->spawned && ent->health > 0) {
        if(ent->buildTime + bTime < level.time) {
            ent->spawned = true;
        }
    }

    // Timer actions
    ent->time1000 += msec;

    if(ent->time1000 >= 1000) {
        ent->time1000 -= 1000;

        if(!ent->spawned && ent->health > 0) {
            ent->health += (sint)(ceil((float32)bHealth / (float32)(bTime * 0.001)));
        } else if(ent->health > 0 && ent->health < bHealth) {
            if(ent->buildableTeam == TEAM_ALIENS && bRegen &&
                    (ent->lastDamageTime + ALIEN_REGEN_DAMAGE_TIME) < level.time) {
                ent->health += bRegen;
            } else if(ent->buildableTeam == TEAM_HUMANS && ent->dcc &&
                      (ent->lastDamageTime + HUMAN_REGEN_DAMAGE_TIME) < level.time) {
                ent->health += DC_HEALRATE * ent->dcc;
            }
        }

        if(ent->health >= bHealth) {
            sint i;
            ent->health = bHealth;

            for(i = 0; i < MAX_CLIENTS; i++) {
                ent->credits[ i ] = 0;
            }
        }

    }

    if(ent->lev1Grabbed && ent->lev1GrabTime + LEVEL1_GRAB_TIME < level.time) {
        ent->lev1Grabbed = false;
    }

    if(ent->clientSpawnTime > 0) {
        ent->clientSpawnTime -= msec;
    }

    if(ent->clientSpawnTime < 0) {
        ent->clientSpawnTime = 0;
    }

    // Pack health
    ent->dcc = (ent->buildableTeam != TEAM_HUMANS) ? 0 : FindDCC(ent);

    if(ent->health <= 0) {
        ent->s.generic1 = 0;
    } else {
        ent->s.generic1 = (uchar8)((B_HEALTH_MASK * ent->health + bHealth - 1) /
                                   bHealth);
    }

    // Set flags
    ent->s.eFlags &= ~(EF_B_POWERED | EF_B_SPAWNED | EF_B_MARKED);

    if(ent->powered) {
        ent->s.eFlags |= EF_B_POWERED;
    }

    if(ent->spawned) {
        ent->s.eFlags |= EF_B_SPAWNED;
    }

    if(ent->deconstruct) {
        ent->s.eFlags |= EF_B_MARKED;
    }

    //check if this buildable is touching any triggers
    BuildableTouchTriggers(ent);

    //fall back on normal physics routines
    idSGamePhysics::Physics(ent, msec);
}


/*
===============
idSGameBuildable::BuildableRange

Check whether a point is within some range of a type of buildable
===============
*/
bool idSGameBuildable::BuildableRange(vec3_t origin, float32 r,
                                      buildable_t buildable) {
    sint       entityList[ MAX_GENTITIES ];
    vec3_t    range;
    vec3_t    mins, maxs;
    sint       i, num;
    gentity_t *ent;

    VectorSet(range, r, r, r);
    VectorAdd(origin, range, maxs);
    VectorSubtract(origin, range, mins);

    num = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

    for(i = 0; i < num; i++) {
        ent = &g_entities[ entityList[ i ] ];

        if(ent->s.eType != ET_BUILDABLE) {
            continue;
        }

        if(ent->buildableTeam == TEAM_HUMANS && !ent->powered) {
            continue;
        }

        if(ent->s.modelindex == buildable && ent->spawned) {
            return true;
        }
    }

    return false;
}

/*
================
idSGameBuildable::FindBuildable

Finds a buildable of the specified type
================
*/
gentity_t *idSGameBuildable::FindBuildable(buildable_t buildable) {
    sint       i;
    gentity_t *ent;

    for(i = 1, ent = g_entities + i; i < level.num_entities; i++, ent++) {
        if(ent->s.eType != ET_BUILDABLE) {
            continue;
        }

        if(ent->s.modelindex == buildable) {
            return ent;
        }
    }

    return nullptr;
}

/*
===============
G_BuildablesIntersect

Test if two buildables intersect each other
===============
*/
bool idSGameBuildable::BuildablesIntersect(buildable_t a, vec3_t originA,
        buildable_t b, vec3_t originB) {
    vec3_t minsA, maxsA;
    vec3_t minsB, maxsB;

    bggame->BuildableBoundingBox(a, minsA, maxsA);
    VectorAdd(minsA, originA, minsA);
    VectorAdd(maxsA, originA, maxsA);

    bggame->BuildableBoundingBox(b, minsB, maxsB);
    VectorAdd(minsB, originB, minsB);
    VectorAdd(maxsB, originB, maxsB);

    return BoundsIntersect(minsA, maxsA, minsB, maxsB);
}

/*
===============
idSGameBuildable::CompareBuildablesForRemoval

qsort comparison function for a buildable removal list
===============
*/
sint idSGameBuildable::CompareBuildablesForRemoval(const void *a,
        const void *b) {
    sint       precedence[ ] = {
        BA_NONE,

        BA_A_BARRICADE,
        BA_A_ACIDTUBE,
        BA_A_TRAPPER,
        BA_A_HIVE,
        BA_A_BOOSTER,
        BA_A_HOVEL,
        BA_A_SPAWN,
        BA_A_OVERMIND,

        BA_H_MGTURRET,
        BA_H_TESLAGEN,
        BA_H_DCC,
        BA_H_MEDISTAT,
        BA_H_ARMOURY,
        BA_H_SPAWN,
        BA_H_REPEATER,
        BA_H_REACTOR
    };

    gentity_t *buildableA, *buildableB;
    sint       i;
    sint       aPrecedence = 0, bPrecedence = 0;
    bool  aMatches = false, bMatches = false;

    buildableA = *(gentity_t **)a;
    buildableB = *(gentity_t **)b;

    // Prefer the one that collides with the thing we're building
    aMatches = BuildablesIntersect(cmpBuildable, cmpOrigin,
                                   (buildable_t)buildableA->s.modelindex, buildableA->s.origin);
    bMatches = BuildablesIntersect(cmpBuildable, cmpOrigin,
                                   (buildable_t)buildableB->s.modelindex, buildableB->s.origin);

    if(aMatches && !bMatches) {
        return -1;
    }

    if(!aMatches && bMatches) {
        return 1;
    }

    // If one matches the thing we're building, prefer it
    aMatches = (buildableA->s.modelindex == cmpBuildable);
    bMatches = (buildableB->s.modelindex == cmpBuildable);

    if(aMatches && !bMatches) {
        return -1;
    }

    if(!aMatches && bMatches) {
        return 1;
    }

    // They're the same type
    if(buildableA->s.modelindex == buildableB->s.modelindex) {
        gentity_t *powerEntity = PowerEntityForPoint(cmpOrigin);

        // Prefer the entity that is providing power for this point
        aMatches = (powerEntity == buildableA);
        bMatches = (powerEntity == buildableB);

        if(aMatches && !bMatches) {
            return -1;
        }

        if(!aMatches && bMatches) {
            return 1;
        }

        // Pick the one marked earliest
        return buildableA->deconstructTime - buildableB->deconstructTime;
    }

    // Resort to preference list
    for(i = 0; i < sizeof(precedence) / sizeof(precedence[ 0 ]); i++) {
        if(buildableA->s.modelindex == precedence[ i ]) {
            aPrecedence = i;
        }

        if(buildableB->s.modelindex == precedence[ i ]) {
            bPrecedence = i;
        }
    }

    return aPrecedence - bPrecedence;
}

/*
===============
G_FreeMarkedBuildables

Free up build points for a team by deconstructing marked buildables
===============
*/
void idSGameBuildable::FreeMarkedBuildables(gentity_t *deconner) {
    sint       i;
    gentity_t *ent;

    if(!g_markDeconstruct.integer) {
        return;    // Not enabled, can't deconstruct anything
    }

    for(i = 0; i < level.numBuildablesForRemoval; i++) {
        ent = level.markedBuildables[ i ];

        idSGameCombat::Damage(ent, nullptr, deconner, nullptr, nullptr,
                              ent->health, 0, MOD_DECONSTRUCT);
        idSGameUtils::FreeEntity(ent);
    }
}

/*
===============
G_SufficientBPAvailable

Determine if enough build points can be released for the buildable
and list the buildables that must be destroyed if this is the case
===============
*/
itemBuildError_t idSGameBuildable::SufficientBPAvailable(
    buildable_t buildable, vec3_t origin) {
    sint               i;
    sint               numBuildables = 0;
    sint               pointsYielded = 0;
    gentity_t         *ent;
    team_t            team = bggame->Buildable(buildable)->team;
    sint               buildPoints = bggame->Buildable(buildable)->buildPoints;
    sint               remainingBP, remainingSpawns;
    bool          collision = false;
    sint               collisionCount = 0;
    bool          repeaterInRange = false;
    sint               repeaterInRangeCount = 0;
    itemBuildError_t  bpError;
    buildable_t       spawn;
    buildable_t       core;
    sint               spawnCount = 0;

    level.numBuildablesForRemoval = 0;

    if(team == TEAM_ALIENS) {
        remainingBP     = level.alienBuildPoints;
        remainingSpawns = level.numAlienSpawns;
        bpError         = IBE_NOALIENBP;
        spawn           = BA_A_SPAWN;
        core            = BA_A_OVERMIND;
    } else if(team == TEAM_HUMANS) {
        remainingBP     = level.humanBuildPoints;
        remainingSpawns = level.numHumanSpawns;
        bpError         = IBE_NOHUMANBP;
        spawn           = BA_H_SPAWN;
        core            = BA_H_REACTOR;
    } else {
        Com_Error(ERR_FATAL, "team is %d\n", team);
        return IBE_NONE;
    }

    // Simple non-marking case
    if(!g_markDeconstruct.integer) {
        if(remainingBP - buildPoints < 0) {
            return bpError;
        }

        // Check for buildable<->buildable collisions
        for(i = MAX_CLIENTS, ent = g_entities + i; i < level.num_entities;
                i++, ent++) {
            if(ent->s.eType != ET_BUILDABLE) {
                continue;
            }

            if(BuildablesIntersect(buildable, origin, (buildable_t)ent->s.modelindex,
                                   ent->s.origin)) {
                return IBE_NOROOM;
            }
        }

        return IBE_NONE;
    }

    // Set buildPoints to the number extra that are required
    buildPoints -= remainingBP;

    // Build a list of buildable entities
    for(i = MAX_CLIENTS, ent = g_entities + i; i < level.num_entities;
            i++, ent++) {
        if(ent->s.eType != ET_BUILDABLE) {
            continue;
        }

        collision = BuildablesIntersect(buildable, origin,
                                        (buildable_t)ent->s.modelindex, ent->s.origin);

        if(collision) {
            // Don't allow replacements at all
            if(g_markDeconstruct.integer == 1) {
                return IBE_NOROOM;
            }

            // Only allow replacements of the same type
            if(g_markDeconstruct.integer == 2 && ent->s.modelindex != buildable) {
                return IBE_NOROOM;
            }

            // Any other setting means anything goes

            collisionCount++;
        }

        // Check if this is a repeater and it's in range
        if(buildable == BA_H_REPEATER &&
                buildable == ent->s.modelindex &&
                Distance(ent->s.origin, origin) < REPEATER_BASESIZE) {
            repeaterInRange = true;
            repeaterInRangeCount++;
        } else {
            repeaterInRange = false;
        }

        if(!ent->inuse) {
            continue;
        }

        if(ent->health <= 0) {
            continue;
        }

        if(ent->buildableTeam != team) {
            continue;
        }

        // Don't allow destruction of hovel with granger inside
        if(ent->s.modelindex == BA_A_HOVEL && ent->active) {
            continue;
        }

        // Explicitly disallow replacement of the core buildable with anything
        // other than the core buildable
        if(ent->s.modelindex == core && buildable != core) {
            continue;
        }

        if(ent->deconstruct) {
            level.markedBuildables[ numBuildables++ ] = ent;

            // Buildables that are marked here will always end up at the front of the
            // removal list, so just incrementing numBuildablesForRemoval is sufficient
            if(collision || repeaterInRange) {
                // Collided with something, so we definitely have to remove it or
                // it's a repeater that intersects the new repeater's power area,
                // so it must be removed

                if(collision) {
                    collisionCount--;
                }

                if(repeaterInRange) {
                    repeaterInRangeCount--;
                }

                pointsYielded += bggame->Buildable((buildable_t)
                                                   ent->s.modelindex)->buildPoints;
                level.numBuildablesForRemoval++;
            } else if(bggame->Buildable((buildable_t)ent->s.modelindex)->uniqueTest &&
                      ent->s.modelindex == buildable) {
                // If it's a unique buildable, it must be replaced by the same type
                pointsYielded += bggame->Buildable((buildable_t)
                                                   ent->s.modelindex)->buildPoints;
                level.numBuildablesForRemoval++;
            }
        }
    }

    // We still need build points, but have no candidates for removal
    if(buildPoints > 0 && numBuildables == 0) {
        return bpError;
    }

    // Collided with something we can't remove
    if(collisionCount > 0) {
        return IBE_NOROOM;
    }

    // There are one or more repeaters we can't remove
    if(repeaterInRangeCount > 0) {
        return IBE_RPTPOWERHERE;
    }

    // Sort the list
    cmpBuildable = buildable;
    VectorCopy(origin, cmpOrigin);
    qsort(level.markedBuildables, numBuildables,
          sizeof(level.markedBuildables[ 0 ]), CompareBuildablesForRemoval);

    // Determine if there are enough markees to yield the required BP
    for(; pointsYielded < buildPoints &&
            level.numBuildablesForRemoval < numBuildables;
            level.numBuildablesForRemoval++) {
        ent = level.markedBuildables[ level.numBuildablesForRemoval ];
        pointsYielded += bggame->Buildable((buildable_t)
                                           ent->s.modelindex)->buildPoints;
    }

    for(i = 0; i < level.numBuildablesForRemoval; i++) {
        if(level.markedBuildables[ i ]->s.modelindex == spawn) {
            spawnCount++;
        }
    }

    // Make sure we're not removing the last spawn
    if(!g_cheats.integer && remainingSpawns > 0 &&
            (remainingSpawns - spawnCount) < 1) {
        return IBE_LASTSPAWN;
    }

    // Not enough points yielded
    if(pointsYielded < buildPoints) {
        return bpError;
    }

    return IBE_NONE;
}

/*
================
G_SetBuildableLinkState

Links or unlinks all the buildable entities
================
*/
void idSGameBuildable::SetBuildableLinkState(bool link) {
    sint       i;
    gentity_t *ent;

    for(i = 1, ent = g_entities + i; i < level.num_entities; i++, ent++) {
        if(ent->s.eType != ET_BUILDABLE) {
            continue;
        }

        if(link) {
            trap_LinkEntity(ent);
        } else {
            trap_UnlinkEntity(ent);
        }
    }
}

void idSGameBuildable::SetBuildableMarkedLinkState(bool link) {
    sint       i;
    gentity_t *ent;

    for(i = 0; i < level.numBuildablesForRemoval; i++) {
        ent = level.markedBuildables[ i ];

        if(link) {
            trap_LinkEntity(ent);
        } else {
            trap_UnlinkEntity(ent);
        }
    }
}

/*
================
idSGameBuildable::CanBuild

Checks to see if a buildable can be built
================
*/
itemBuildError_t idSGameBuildable::CanBuild(gentity_t *ent,
        buildable_t buildable, sint distance, vec3_t origin) {
    vec3_t            angles;
    vec3_t            entity_origin, normal;
    vec3_t            mins, maxs;
    trace_t           tr1, tr2, tr3;
    itemBuildError_t  reason = IBE_NONE, tempReason;
    gentity_t         *tempent;
    float32             minNormal;
    bool          invert;
    sint               contents;
    playerState_t     *ps = &ent->client->ps;
    sint               buildPoints;

    // Stop all buildables from interacting with traces
    SetBuildableLinkState(false);

    bggame->BuildableBoundingBox(buildable, mins, maxs);

    bggame->PositionBuildableRelativeToPlayer(ps, mins, maxs, trap_Trace,
            entity_origin, angles, &tr1);
    trap_Trace(&tr2, entity_origin, mins, maxs, entity_origin, ent->s.number,
               MASK_PLAYERSOLID);
    trap_Trace(&tr3, ps->origin, nullptr, nullptr, entity_origin,
               ent->s.number, MASK_PLAYERSOLID);

    VectorCopy(entity_origin, origin);

    VectorCopy(tr1.plane.normal, normal);
    minNormal = bggame->Buildable(buildable)->minNormal;
    invert = bggame->Buildable(buildable)->invertNormal;

    //can we build at this angle?
    if(!(normal[ 2 ] >= minNormal || (invert && normal[ 2 ] <= -minNormal))) {
        reason = IBE_NORMAL;
    }

    if(tr1.entityNum != ENTITYNUM_WORLD) {
        reason = IBE_NORMAL;
    }

    contents = trap_PointContents(entity_origin, -1);
    buildPoints = bggame->Buildable(buildable)->buildPoints;

    if(ent->client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS) {
        //alien criteria

        // Check there is an Overmind
        if(buildable != BA_A_OVERMIND) {
            if(!level.overmindPresent) {
                reason = IBE_NOOVERMIND;
            }
        }

        //check there is creep near by for building on
        if(bggame->Buildable(buildable)->creepTest) {
            if(!IsCreepHere(entity_origin)) {
                reason = IBE_NOCREEP;
            }
        }

        if(buildable == BA_A_HOVEL) {
            vec3_t    builderMins, builderMaxs;

            //this assumes the adv builder is the biggest thing that'll use the hovel
            bggame->ClassBoundingBox(PCL_ALIEN_BUILDER0_UPG, builderMins, builderMaxs,
                                     nullptr, nullptr, nullptr);

            if(APropHovel_Blocked(origin, angles, normal, ent)) {
                reason = IBE_HOVELEXIT;
            }
        }

        // Check permission to build here
        if(tr1.surfaceFlags & SURF_NOALIENBUILD ||
                contents & CONTENTS_NOALIENBUILD) {
            reason = IBE_PERMISSION;
        }
    } else if(ent->client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS) {
        //human criteria

        // Check for power
        if(IsPowered(entity_origin) == BA_NONE) {
            //tell player to build a repeater to provide power
            if(buildable != BA_H_REACTOR && buildable != BA_H_REPEATER) {
                reason = IBE_NOPOWERHERE;
            }
        }

        //this buildable requires a DCC
        if(bggame->Buildable(buildable)->dccTest && !IsDCCBuilt()) {
            reason = IBE_NODCC;
        }

        //check that there is a parent reactor when building a repeater
        if(buildable == BA_H_REPEATER) {
            tempent = FindBuildable(BA_H_REACTOR);

            if(tempent == nullptr) {  // No reactor
                reason = IBE_RPTNOREAC;
            }
            /*      else if( g_markDeconstruct.integer && IsPowered( entity_origin ) == BA_H_REACTOR )
                    reason = IBE_RPTPOWERHERE;*/
            else if(!g_markDeconstruct.integer &&
                    RepeaterEntityForPoint(entity_origin)) {
                reason = IBE_RPTPOWERHERE;
            }
        }

        // Check permission to build here
        if(tr1.surfaceFlags & SURF_NOHUMANBUILD ||
                contents & CONTENTS_NOHUMANBUILD) {
            reason = IBE_PERMISSION;
        }
    }

    // Check permission to build here
    if(tr1.surfaceFlags & SURF_NOBUILD || contents & CONTENTS_NOBUILD) {
        reason = IBE_PERMISSION;
    }

    // Can we only have one of these?
    if(bggame->Buildable(buildable)->uniqueTest) {
        tempent = FindBuildable(buildable);

        if(tempent && !tempent->deconstruct && !(tempent->s.eFlags & EF_DEAD)) {
            switch(buildable) {
                case BA_A_OVERMIND:
                    reason = IBE_ONEOVERMIND;
                    break;

                case BA_A_HOVEL:
                    reason = IBE_ONEHOVEL;
                    break;

                case BA_H_REACTOR:
                    reason = IBE_ONEREACTOR;
                    break;

                default:
                    Com_Error(ERR_FATAL, "No reason for denying build of %d\n", buildable);
                    break;
            }
        }
    }

    if((tempReason = SufficientBPAvailable(buildable, origin)) != IBE_NONE) {
        reason = tempReason;
    }

    // Relink buildables
    SetBuildableLinkState(true);

    //check there is enough room to spawn from (presuming this is a spawn)
    if(reason == IBE_NONE) {
        SetBuildableMarkedLinkState(false);

        if(CheckSpawnPoint(ENTITYNUM_NONE, origin, normal, buildable,
                           nullptr) != nullptr) {
            reason = IBE_NORMAL;
        }

        SetBuildableMarkedLinkState(true);
    }

    //this item does not fit here
    if(reason == IBE_NONE && (tr2.fraction < 1.0 || tr3.fraction < 1.0)) {
        reason = IBE_NOROOM;
    }

    if(reason != IBE_NONE) {
        level.numBuildablesForRemoval = 0;
    }

    return reason;
}


/*
================
G_Build

Spawns a buildable
================
*/
gentity_t *idSGameBuildable::Build(gentity_t *builder,
                                   buildable_t buildable, vec3_t origin, vec3_t angles) {
    gentity_t *built;
    vec3_t    normal;

    // Free existing buildables
    FreeMarkedBuildables(builder);

    // Spawn the buildable
    built = idSGameUtils::Spawn();
    built->s.eType = ET_BUILDABLE;
    built->killedBy = ENTITYNUM_NONE;
    built->classname = bggame->Buildable(buildable)->entityName;
    built->s.modelindex = buildable;
    built->buildableTeam = (team_t)(built->s.modelindex2 = bggame->Buildable(
                                        buildable)->team);
    bggame->BuildableBoundingBox(buildable, built->r.mins, built->r.maxs);

    // detect the buildable's normal vector
    if(!builder->client) {
        // initial base layout created by server

        if(builder->s.origin2[ 0 ] ||
                builder->s.origin2[ 1 ] ||
                builder->s.origin2[ 2 ]) {
            VectorCopy(builder->s.origin2, normal);
        } else if(bggame->Buildable(buildable)->traj == TR_BUOYANCY) {
            VectorSet(normal, 0.0f, 0.0f, -1.0f);
        } else {
            VectorSet(normal, 0.0f, 0.0f, 1.0f);
        }
    } else {
        // in-game building by a player
        bggame->GetClientNormal(&builder->client->ps, normal);
    }

    // when building the initial layout, spawn the entity slightly off its
    // target surface so that it can be "dropped" onto it
    if(!builder->client) {
        VectorMA(origin, 1.0f, normal, origin);
    }

    built->health = 1;

    built->splashDamage = bggame->Buildable(buildable)->splashDamage;
    built->splashRadius = bggame->Buildable(buildable)->splashRadius;
    built->splashMethodOfDeath = bggame->Buildable(buildable)->meansOfDeath;

    built->nextthink = bggame->Buildable(buildable)->nextthink;

    built->takedamage = true;
    built->spawned = false;
    built->buildTime = built->s.time = level.time;

    // build instantly in cheat mode
    if(builder->client && g_cheats.integer) {
        built->health = bggame->Buildable(buildable)->health;
        built->buildTime = built->s.time =
                               level.time - bggame->Buildable(buildable)->buildTime;
    }

    //things that vary for each buildable that aren't in the dbase
    switch(buildable) {
        case BA_A_SPAWN:
            built->die = ASpawn_Die;
            built->think = ASpawn_Think;
            built->pain = AGeneric_Pain;
            break;

        case BA_A_BARRICADE:
            built->die = ABarricade_Die;
            built->think = ABarricade_Think;
            built->pain = ABarricade_Pain;
            built->touch = ABarricade_Touch;
            built->shrunkTime = 0;
            ABarricade_Shrink(built, true);
            break;

        case BA_A_BOOSTER:
            built->die = AGeneric_Die;
            built->think = AGeneric_Think;
            built->pain = AGeneric_Pain;
            built->touch = ABooster_Touch;
            break;

        case BA_A_ACIDTUBE:
            built->die = AGeneric_Die;
            built->think = AAcidTube_Think;
            built->pain = AGeneric_Pain;
            break;

        case BA_A_HIVE:
            built->die = AHive_Die;
            built->think = AHive_Think;
            built->pain = AHive_Pain;
            break;

        case BA_A_TRAPPER:
            built->die = AGeneric_Die;
            built->think = ATrapper_Think;
            built->pain = AGeneric_Pain;
            break;

        case BA_A_OVERMIND:
            built->die = ASpawn_Die;
            built->think = AOvermind_Think;
            built->pain = AGeneric_Pain;
            break;

        case BA_A_HOVEL:
            built->die = AHovel_Die;
            built->use = AHovel_Use;
            built->think = AHovel_Think;
            built->pain = AGeneric_Pain;
            break;

        case BA_H_SPAWN:
            built->die = HSpawn_Die;
            built->think = HSpawn_Think;
            break;

        case BA_H_MGTURRET:
            built->die = HSpawn_Die;
            built->think = HMGTurret_Think;
            break;

        case BA_H_TESLAGEN:
            built->die = HSpawn_Die;
            built->think = HTeslaGen_Think;
            break;

        case BA_H_ARMOURY:
            built->think = HArmoury_Think;
            built->die = HSpawn_Die;
            built->use = HArmoury_Activate;
            break;

        case BA_H_DCC:
            built->think = HDCC_Think;
            built->die = HSpawn_Die;
            break;

        case BA_H_MEDISTAT:
            built->think = HMedistat_Think;
            built->die = HMedistat_Die;
            break;

        case BA_H_REACTOR:
            built->think = HReactor_Think;
            built->die = HSpawn_Die;
            built->use = HRepeater_Use;
            built->powered = built->active = true;
            break;

        case BA_H_REPEATER:
            built->think = HRepeater_Think;
            built->die = HSpawn_Die;
            built->use = HRepeater_Use;
            built->count = -1;
            break;

        default:
            //erk
            break;
    }

    built->s.number = built - g_entities;
    built->r.contents = CONTENTS_BODY;
    built->clipmask = MASK_PLAYERSOLID;
    built->enemy = nullptr;
    built->s.weapon = bggame->Buildable(buildable)->turretProjType;

    if(builder->client) {
        built->builtBy = builder->client->ps.clientNum;
    } else {
        built->builtBy = -1;
    }

    idSGameUtils::SetOrigin(built, origin);

    // gently nudge the buildable onto the surface :)
    VectorScale(normal, -50.0f, built->s.pos.trDelta);

    // set turret angles
    VectorCopy(builder->s.angles2, built->s.angles2);

    VectorCopy(angles, built->s.angles);
    built->s.angles[ PITCH ] = 0.0f;
    built->s.angles2[ YAW ] = angles[ YAW ];
    built->s.pos.trType = bggame->Buildable(buildable)->traj;
    built->s.pos.trTime = level.time;
    built->physicsBounce = bggame->Buildable(buildable)->bounce;
    built->s.groundEntityNum = -1;

    if(bggame->Buildable((buildable_t)built->s.modelindex)->team ==
            TEAM_ALIENS) {
        built->powered = true;
    } else if((built->powered = FindPower(built))) {
        built->powered = true;
    }

    VectorCopy(normal, built->s.origin2);

    idSGameUtils::AddEvent(built, EV_BUILD_CONSTRUCT, 0);

    SetIdleBuildableAnim(built,
                         (buildableAnimNumber_t)bggame->Buildable(buildable)->idleAnim);

    if(built->builtBy >= 0) {
        SetBuildableAnim(built, BANIM_CONSTRUCT1, true);
    }

    trap_LinkEntity(built);

    return built;
}

/*
=================
idSGameBuildable::BuildIfValid
=================
*/
bool idSGameBuildable::BuildIfValid(gentity_t *ent,
                                    buildable_t buildable) {
    float32         dist;
    vec3_t        origin;

    dist = bggame->Class((class_t)
                         ent->client->ps.stats[ STAT_CLASS ])->buildDist;

    switch(CanBuild(ent, buildable, dist, origin)) {
        case IBE_NONE:
            Build(ent, buildable, origin, ent->s.apos.trBase);
            return true;

        case IBE_NOALIENBP:
            idSGameUtils::TriggerMenu(ent->client->ps.clientNum, MN_A_NOBP);
            return false;

        case IBE_NOOVERMIND:
            idSGameUtils::TriggerMenu(ent->client->ps.clientNum, MN_A_NOOVMND);
            return false;

        case IBE_NOCREEP:
            idSGameUtils::TriggerMenu(ent->client->ps.clientNum, MN_A_NOCREEP);
            return false;

        case IBE_ONEOVERMIND:
            idSGameUtils::TriggerMenu(ent->client->ps.clientNum, MN_A_ONEOVERMIND);
            return false;

        case IBE_ONEHOVEL:
            idSGameUtils::TriggerMenu(ent->client->ps.clientNum, MN_A_ONEHOVEL);
            return false;

        case IBE_HOVELEXIT:
            idSGameUtils::TriggerMenu(ent->client->ps.clientNum, MN_A_HOVEL_EXIT);
            return false;

        case IBE_NORMAL:
            idSGameUtils::TriggerMenu(ent->client->ps.clientNum, MN_B_NORMAL);
            return false;

        case IBE_PERMISSION:
            idSGameUtils::TriggerMenu(ent->client->ps.clientNum, MN_B_NORMAL);
            return false;

        case IBE_ONEREACTOR:
            idSGameUtils::TriggerMenu(ent->client->ps.clientNum, MN_H_ONEREACTOR);
            return false;

        case IBE_NOPOWERHERE:
            idSGameUtils::TriggerMenu(ent->client->ps.clientNum, MN_H_NOPOWERHERE);
            return false;

        case IBE_NOROOM:
            idSGameUtils::TriggerMenu(ent->client->ps.clientNum, MN_B_NOROOM);
            return false;

        case IBE_NOHUMANBP:
            idSGameUtils::TriggerMenu(ent->client->ps.clientNum, MN_H_NOBP);
            return false;

        case IBE_NODCC:
            idSGameUtils::TriggerMenu(ent->client->ps.clientNum, MN_H_NODCC);
            return false;

        case IBE_RPTPOWERHERE:
            idSGameUtils::TriggerMenu(ent->client->ps.clientNum, MN_H_RPTPOWERHERE);
            return false;

        case IBE_LASTSPAWN:
            idSGameUtils::TriggerMenu(ent->client->ps.clientNum, MN_B_LASTSPAWN);
            return false;

        default:
            break;
    }

    return false;
}

/*
================
G_FinishSpawningBuildable

Traces down to find where an item should rest, instead of letting them
free fall from their spawn points
================
*/
void idSGameBuildable::FinishSpawningBuildable(gentity_t *ent) {
    trace_t     tr;
    vec3_t      dest;
    gentity_t   *built;
    buildable_t buildable = (buildable_t)ent->s.modelindex;

    built = Build(ent, buildable, ent->s.pos.trBase, ent->s.angles);
    idSGameUtils::FreeEntity(ent);

    built->takedamage = true;
    built->spawned = true; //map entities are already spawned
    built->health = bggame->Buildable(buildable)->health;

    // drop towards normal surface
    VectorScale(built->s.origin2, -4096.0f, dest);
    VectorAdd(dest, built->s.origin, dest);

    trap_Trace(&tr, built->s.origin, built->r.mins, built->r.maxs, dest,
               built->s.number, built->clipmask);

    if(tr.startsolid) {
        idSGameMain::Printf(S_COLOR_YELLOW
                            "idSGameBuildable::FinishSpawningBuildable: %s startsolid at %s\n",
                            built->classname, idSGameUtils::vtos(built->s.origin));
        idSGameUtils::FreeEntity(built);
        return;
    }

    //point items in the correct direction
    VectorCopy(tr.plane.normal, built->s.origin2);

    // allow to ride movers
    built->s.groundEntityNum = tr.entityNum;

    idSGameUtils::SetOrigin(built, tr.endpos);

    trap_LinkEntity(built);
}

/*
============
idSGameBuildable::SpawnBuildable

Sets the clipping size and plants the object on the floor.

Items can't be immediately dropped to floor, because they might
be on an entity that hasn't spawned yet.
============
*/
void idSGameBuildable::SpawnBuildable(gentity_t *ent,
                                      buildable_t buildable) {
    ent->s.modelindex = buildable;

    // some movers spawn on the second frame, so delay item
    // spawns until the third frame so they can ride trains
    ent->nextthink = level.time + FRAMETIME * 2;
    ent->think = FinishSpawningBuildable;
}

/*
============
idSGameBuildable::LayoutSave
============
*/
void idSGameBuildable::LayoutSave(valueType *name) {
    valueType map[ MAX_QPATH ];
    valueType fileName[ MAX_OSPATH ];
    fileHandle_t f;
    sint len;
    sint i;
    gentity_t *ent;
    valueType *s;

    trap_Cvar_VariableStringBuffer("mapname", map, sizeof(map));

    if(!map[ 0 ]) {
        idSGameMain::Printf("LayoutSave( ): no map is loaded\n");
        return;
    }

    Q_vsprintf_s(fileName, sizeof(fileName), sizeof(fileName),
                 "layouts/%s/%s.dat", map, name);

    len = trap_FS_FOpenFile(fileName, &f, FS_WRITE);

    if(len < 0) {
        idSGameMain::Printf("layoutsave: could not open %s\n", fileName);
        return;
    }

    idSGameMain::Printf("layoutsave: saving layout to %s\n", fileName);

    for(i = MAX_CLIENTS; i < level.num_entities; i++) {
        ent = &level.gentities[ i ];

        if(ent->s.eType != ET_BUILDABLE) {
            continue;
        }

        s = va("%i %f %f %f %f %f %f %f %f %f %f %f %f\n",
               ent->s.modelindex,
               ent->s.pos.trBase[ 0 ],
               ent->s.pos.trBase[ 1 ],
               ent->s.pos.trBase[ 2 ],
               ent->s.angles[ 0 ],
               ent->s.angles[ 1 ],
               ent->s.angles[ 2 ],
               ent->s.origin2[ 0 ],
               ent->s.origin2[ 1 ],
               ent->s.origin2[ 2 ],
               ent->s.angles2[ 0 ],
               ent->s.angles2[ 1 ],
               ent->s.angles2[ 2 ]);
        trap_FS_Write(s, strlen(s), f);
    }

    trap_FS_FCloseFile(f);
}

/*
============
idSGameBuildable::LayoutList
============
*/
sint idSGameBuildable::LayoutList(pointer map, valueType *list, sint len) {
    // up to 128 single character layout names could fit in layouts
    valueType fileList[(MAX_CVAR_VALUE_STRING / 2) * 5 ] = {""};
    valueType layouts[ MAX_CVAR_VALUE_STRING ] = {""};
    sint numFiles, i, fileLen = 0, listLen;
    sint  count = 0;
    valueType *filePtr;

    Q_strcat(layouts, sizeof(layouts), "*BUILTIN* ");
    numFiles = trap_FS_GetFileList(va("layouts/%s", map), ".dat",
                                   fileList, sizeof(fileList));
    filePtr = fileList;

    for(i = 0; i < numFiles; i++, filePtr += fileLen + 1) {
        fileLen = strlen(filePtr);
        listLen = strlen(layouts);

        if(fileLen < 5) {
            continue;
        }

        // list is full, stop trying to add to it
        if((listLen + fileLen) >= sizeof(layouts)) {
            break;
        }

        Q_strcat(layouts,  sizeof(layouts), filePtr);
        listLen = strlen(layouts);

        // strip extension and add space delimiter
        layouts[ listLen - 4 ] = ' ';
        layouts[ listLen - 3 ] = '\0';
        count++;
    }

    if(count != numFiles) {
        idSGameMain::Printf(S_COLOR_YELLOW
                            "WARNING: layout list was truncated to %d "
                            "layouts, but %d layout files exist in layouts/%s/.\n",
                            count, numFiles, map);
    }

    Q_strncpyz(list, layouts, len);
    return count + 1;
}

/*
============
idSGameBuildable::LayoutSelect

set level.layout based on g_layouts or g_layoutAuto
============
*/
void idSGameBuildable::LayoutSelect(void) {
    valueType fileName[ MAX_OSPATH ];
    valueType layouts[ MAX_CVAR_VALUE_STRING ];
    valueType layouts2[ MAX_CVAR_VALUE_STRING ];
    valueType *l;
    valueType map[ MAX_QPATH ];
    valueType *s;
    sint cnt = 0;
    sint layoutNum;

    Q_strncpyz(layouts, g_layouts.string, sizeof(layouts));
    trap_Cvar_VariableStringBuffer("mapname", map, sizeof(map));

    // one time use cvar
    trap_Cvar_Set("g_layouts", "");

    // pick an included layout at random if no list has been provided
    if(!layouts[ 0 ] && g_layoutAuto.integer) {
        LayoutList(map, layouts, sizeof(layouts));
    }

    if(!layouts[ 0 ]) {
        return;
    }

    Q_strncpyz(layouts2, layouts, sizeof(layouts2));
    l = &layouts2[ 0 ];
    layouts[ 0 ] = '\0';

    while(1) {
        s = COM_ParseExt(&l, false);

        if(!*s) {
            break;
        }

        if(!Q_stricmp(s, "*BUILTIN*")) {
            Q_strcat(layouts, sizeof(layouts), s);
            Q_strcat(layouts, sizeof(layouts), " ");
            cnt++;
            continue;
        }

        Q_vsprintf_s(fileName, sizeof(fileName), sizeof(fileName),
                     "layouts/%s/%s.dat", map, s);

        if(trap_FS_FOpenFile(fileName, nullptr, FS_READ) > 0) {
            Q_strcat(layouts, sizeof(layouts), s);
            Q_strcat(layouts, sizeof(layouts), " ");
            cnt++;
        } else {
            idSGameMain::Printf(S_COLOR_YELLOW
                                "idSGameBuildable::LayoutSelect: WARNING: layout \"%s\" does not exist\n",
                                s);
        }
    }

    if(!cnt) {
        idSGameMain::Printf(S_COLOR_RED
                            "idSGameBuildable::LayoutSelect: ERROR: none of the specified layouts could be "
                            "found, using map default\n");
        return;
    }

    layoutNum = (rand() % cnt) + 1;
    cnt = 0;

    Q_strncpyz(layouts2, layouts, sizeof(layouts2));
    l = &layouts2[ 0 ];

    while(1) {
        s = COM_ParseExt(&l, false);

        if(!*s) {
            break;
        }

        Q_strncpyz(level.layout, s, sizeof(level.layout));
        cnt++;

        if(cnt >= layoutNum) {
            break;
        }
    }

    idSGameMain::Printf("idSGameBuildable::LayoutSelect: Using layout \"%s\" from list (%s)\n",
                        level.layout, layouts);
}

/*
============
G_LayoutBuildItem
============
*/
void idSGameBuildable::LayoutBuildItem(buildable_t buildable,
                                       vec3_t origin, vec3_t angles, vec3_t origin2, vec3_t angles2) {
    gentity_t *builder;

    builder = idSGameUtils::Spawn();
    builder->client = 0;
    VectorCopy(origin, builder->s.pos.trBase);
    VectorCopy(angles, builder->s.angles);
    VectorCopy(origin2, builder->s.origin2);
    VectorCopy(angles2, builder->s.angles2);
    SpawnBuildable(builder, buildable);
}

/*
============
idSGameBuildable::LayoutLoad

load the layout .dat file indicated by level.layout and spawn buildables
as if a builder was creating them
============
*/
void idSGameBuildable::LayoutLoad(void) {
    fileHandle_t f;
    sint len;
    valueType *layout;
    valueType map[ MAX_QPATH ];
    sint buildable = BA_NONE;
    vec3_t origin = { 0.0f, 0.0f, 0.0f };
    vec3_t angles = { 0.0f, 0.0f, 0.0f };
    vec3_t origin2 = { 0.0f, 0.0f, 0.0f };
    vec3_t angles2 = { 0.0f, 0.0f, 0.0f };
    valueType line[ MAX_STRING_CHARS ];
    sint i = 0;

    if(!level.layout[ 0 ] || !Q_stricmp(level.layout, "*BUILTIN*")) {
        return;
    }

    trap_Cvar_VariableStringBuffer("mapname", map, sizeof(map));
    len = trap_FS_FOpenFile(va("layouts/%s/%s.dat", map, level.layout),
                            &f, FS_READ);

    if(len < 0) {
        idSGameMain::Printf("ERROR: layout %s could not be opened\n",
                            level.layout);
        return;
    }

    layout = (valueType *)bggame->Alloc(len + 1);
    trap_FS_Read(layout, len, f);
    *(layout + len) = '\0';
    trap_FS_FCloseFile(f);

    while(*layout) {
        if(i >= sizeof(line) - 1) {
            idSGameMain::Printf(S_COLOR_RED
                                "ERROR: line overflow in %s before \"%s\"\n",
                                va("layouts/%s/%s.dat", map, level.layout), line);
            return;
        }

        line[ i++ ] = *layout;
        line[ i ] = '\0';

        if(*layout == '\n') {
            i = 0;
            sscanf(line, "%d %f %f %f %f %f %f %f %f %f %f %f %f\n",
                   &buildable,
                   &origin[ 0 ], &origin[ 1 ], &origin[ 2 ],
                   &angles[ 0 ], &angles[ 1 ], &angles[ 2 ],
                   &origin2[ 0 ], &origin2[ 1 ], &origin2[ 2 ],
                   &angles2[ 0 ], &angles2[ 1 ], &angles2[ 2 ]);

            if(buildable > BA_NONE && buildable < BA_NUM_BUILDABLES) {
                LayoutBuildItem((buildable_t)buildable, origin, angles, origin2, angles2);
            } else {
                idSGameMain::Printf(S_COLOR_YELLOW
                                    "WARNING: bad buildable number (%d) in layout.  skipping\n", buildable);
            }
        }

        layout++;
    }
}

/*
============
idSGameBuildable::BaseSelfDestruct
============
*/
void idSGameBuildable::BaseSelfDestruct(team_t team) {
    sint       i;
    gentity_t *ent;

    for(i = MAX_CLIENTS; i < level.num_entities; i++) {
        ent = &level.gentities[ i ];

        if(ent->health <= 0) {
            continue;
        }

        if(ent->s.eType != ET_BUILDABLE) {
            continue;
        }

        if(ent->buildableTeam != team) {
            continue;
        }

        idSGameCombat::Damage(ent, nullptr, nullptr, nullptr, nullptr, 10000, 0,
                              MOD_SUICIDE);
    }
}

