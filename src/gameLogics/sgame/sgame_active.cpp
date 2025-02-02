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
// File name:   sgame_active.cpp
// Created:
// Compilers:   Microsoft (R) C/C++ Optimizing Compiler Version 19.26.28806 for x64,
//              gcc (Ubuntu 9.3.0-10ubuntu2) 9.3.0
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#include <sgame/sgame_precompiled.hpp>

/*
===============
idSGameActive::idSGamActive
===============
*/
idSGameActive::idSGameActive(void) {
}

/*
===============
idSGameActive::~idSGameActive
===============
*/
idSGameActive::~idSGameActive(void) {
}

/*
===============
idSGameActive ::DamageFeedback

Called just before a snapshot is sent to the given player.
Totals up all damage and generates both the player_state_t
damage values to that client for pain blends and kicks, and
global pain sound events for all clients.
===============
*/
void idSGameActive::DamageFeedback(gentity_t *player) {
    gclient_t *client;
    float32 count;
    vec3_t angles;

    client = player->client;

    if(!PM_Live(client->ps.pm_type)) {
        return;
    }

    // total points of damage shot at the player this frame
    count = client->damage_blood + client->damage_armor;

    if(count == 0) {
        return;   // didn't take any damage
    }

    if(count > 255) {
        count = 255;
    }

    // send the information to the client

    // world damage (falling, slime, etc) uses a special code
    // to make the blend blob centered instead of positional
    if(client->damage_fromWorld) {
        client->ps.damagePitch = 255;
        client->ps.damageYaw = 255;

        client->damage_fromWorld = false;
    } else {
        vectoangles(client->damage_from, angles);
        client->ps.damagePitch = angles[ PITCH ] / 360.0 * 256;
        client->ps.damageYaw = angles[ YAW ] / 360.0 * 256;
    }

    // play an apropriate pain sound
    if((level.time > player->pain_debounce_time) &&
            !(player->flags & FL_GODMODE)) {
        player->pain_debounce_time = level.time + 700;
        idSGameUtils::AddEvent(player, EV_PAIN,
                               player->health > 255 ? 255 : player->health);
        client->ps.damageEvent++;
    }

    client->ps.damageCount = count;

    //
    // clear totals
    //
    client->damage_blood = 0;
    client->damage_armor = 0;
    client->damage_knockback = 0;
}

/*
=============
idSGameActive::WorldEffects

Check for lava / slime contents and drowning
=============
*/
void idSGameActive::WorldEffects(gentity_t *ent) {
    sint waterlevel;

    if(ent->client->noclip) {
        ent->client->airOutTime = level.time + 12000; // don't need air
        return;
    }

    waterlevel = ent->waterlevel;

    //
    // check for drowning
    //
    if(waterlevel == 3) {
        // if out of air, start drowning
        if(ent->client->airOutTime < level.time) {
            // drown!
            ent->client->airOutTime += 1000;

            if(ent->health > 0) {
                // take more damage the longer underwater
                ent->damage += 2;

                if(ent->damage > 15) {
                    ent->damage = 15;
                }

                // play a gurp sound instead of a normal pain sound
                if(ent->health <= ent->damage) {
                    idSGameUtils::Sound(ent, CHAN_VOICE,
                                        idSGameUtils::SoundIndex("*drown.wav"));
                } else if(rand() & 1) {
                    idSGameUtils::Sound(ent, CHAN_VOICE,
                                        idSGameUtils::SoundIndex("sound/player/gurp1.wav"));
                } else {
                    idSGameUtils::Sound(ent, CHAN_VOICE,
                                        idSGameUtils::SoundIndex("sound/player/gurp2.wav"));
                }

                // don't play a normal pain sound
                ent->pain_debounce_time = level.time + 200;

                idSGameCombat::Damage(ent, nullptr, nullptr, nullptr, nullptr, ent->damage,
                                      DAMAGE_NO_ARMOR, MOD_WATER);
            }
        }
    } else {
        ent->client->airOutTime = level.time + 12000;
        ent->damage = 2;
    }

    // check for sizzle damage (move to pmove?)
    if(waterlevel && (ent->watertype & (CONTENTS_LAVA | CONTENTS_SLIME))) {
        if(ent->health > 0 && ent->pain_debounce_time <= level.time) {
            if(ent->watertype & CONTENTS_LAVA) {
                idSGameCombat::Damage(ent, nullptr, nullptr, nullptr, nullptr,
                                      30 * waterlevel, 0, MOD_LAVA);
            }

            if(ent->watertype & CONTENTS_SLIME) {
                idSGameCombat::Damage(ent, nullptr, nullptr, nullptr, nullptr,
                                      10 * waterlevel, 0, MOD_SLIME);
            }
        }
    }
}

/*
===============
idSGameActive::SetClientSound
===============
*/
void idSGameActive::SetClientSound(gentity_t *ent) {
    if(ent->waterlevel &&
            (ent->watertype & (CONTENTS_LAVA | CONTENTS_SLIME))) {
        ent->client->ps.loopSound = level.snd_fry;
    } else {
        ent->client->ps.loopSound = 0;
    }
}

/*
==============
idSGameActive::ClientShove
==============
*/
sint idSGameActive::GetClientMass(gentity_t *ent) {
    sint entMass = 100;

    if(ent->client->pers.teamSelection == TEAM_ALIENS) {
        entMass = bggame->Class(ent->client->pers.classSelection)->health;
    } else if(ent->client->pers.teamSelection == TEAM_HUMANS) {
        if(bggame->InventoryContainsUpgrade(UP_BATTLESUIT,
                                            ent->client->ps.stats)) {
            entMass *= 2;
        }
    } else {
        return 0;
    }

    return entMass;
}

/*
===============
idSGameActive::ClientShove
===============
*/
void idSGameActive::ClientShove(gentity_t *ent, gentity_t *victim) {
    // FIXME: do this with class knockbackscale
    vec3_t dir, push;
    float32 force;
    sint entMass, vicMass;

    // Don't push if the entity is not trying to move
    if(!ent->client->pers.cmd.rightmove &&
            !ent->client->pers.cmd.forwardmove && !ent->client->pers.cmd.upmove) {
        return;
    }

    // Cannot push enemy players unless they are walking on the player
    if(!idSGameTeam::OnSameTeam(ent, victim) &&
            victim->client->ps.groundEntityNum != ent - g_entities) {
        return;
    }

    // Shove force is scaled by relative mass
    entMass = GetClientMass(ent);
    vicMass = GetClientMass(victim);

    if(vicMass <= 0 || entMass <= 0) {
        return;
    }

    force = g_shove.value * entMass / vicMass;

    if(force < 0) {
        force = 0;
    }

    if(force > 150) {
        force = 150;
    }

    // Give the victim some shove velocity
    VectorSubtract(victim->r.currentOrigin, ent->r.currentOrigin, dir);
    VectorNormalizeFast(dir);
    VectorScale(dir, force, push);
    VectorAdd(victim->client->ps.velocity, push, victim->client->ps.velocity);

    // Set the pmove timer so that the other client can't cancel
    // out the movement immediately
    if(!victim->client->ps.pm_time) {
        sint time;

        time = force * 2 + 0.5f;

        if(time < 50) {
            time = 50;
        }

        if(time > 200) {
            time = 200;
        }

        victim->client->ps.pm_time = time;
        victim->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;

        // Knock victim off if they are wallwalking on us
        if((victim->client->ps.groundEntityNum == ent - g_entities) &&
                (victim->client->ps.stats[STAT_STATE] & SS_WALLCLIMBING)) {
            victim->client->ps.pm_flags |= PMF_TIME_KNOCKOFF;
        }
    }
}

/*
==============
v::ClientImpacts
==============
*/
void idSGameActive::ClientImpacts(gentity_t *ent, pmove_t *pm) {
    sint i;
    trace_t trace;
    gentity_t *other;

    // clear a fake trace struct for touch function
    ::memset(&trace, 0, sizeof(trace));

    for(i = 0; i < pm->numtouch; i++) {
        other = &g_entities[ pm->touchents[ i ] ];

        // see G_UnlaggedDetectCollisions(), this is the inverse of that.
        // if our movement is blocked by another player's real position,
        // don't use the unlagged position for them because they are
        // blocking or server-side Pmove() from reaching it
        if(other->client && other->client->unlaggedCalc.used) {
            other->client->unlaggedCalc.used = false;
        }

        // tyrant impact attacks
        if(ent->client->ps.weapon == WP_ALEVEL4) {
            idSGameWeapons::ChargeAttack(ent, other);
            idSGameWeapons::CrushAttack(ent, other);
        }

        // shove players
        if(ent->client && other->client) {
            ClientShove(ent, other);
        }

        // touch triggers
        if(other->touch) {
            other->touch(other, ent, &trace);
        }
    }
}

/*
============
idSGameActive::OtherTouchTriggers

Touch Triggers even if ent is not a client
============
*/
void idSGameActive::OtherTouchTriggers(gentity_t *ent) {
    sint             num;
    sint             touch[MAX_GENTITIES];
    gentity_t      *hit;
    trace_t         trace;
    vec3_t          mins, maxs;
    sint             i;
    static vec3_t   range = { 40, 40, 52 };

    VectorSubtract(ent->r.currentOrigin, range, mins);
    VectorAdd(ent->r.currentOrigin, range, maxs);

    num = trap_EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);

    // can't use ent->absmin, because that has a one unit pad
    VectorAdd(ent->r.currentOrigin, ent->r.mins, mins);
    VectorAdd(ent->r.currentOrigin, ent->r.maxs, maxs);

    for(i = 0; i < num; i++) {
        hit = &g_entities[touch[i]];

        if(!hit->touch && !ent->touch) {
            continue;
        }

        if(!(hit->r.contents & CONTENTS_TRIGGER)) {
            continue;
        }

        if(hit->s.eType == ET_ITEM) {
            continue;

        } else {
            if(!trap_EntityContactCapsule(mins, maxs, hit)) {
                continue;
            }
        }

        ::memset(&trace, 0, sizeof(trace));

        if(hit->touch) {
            hit->touch(hit, ent, &trace);
        }
    }
}

/*
============
idSGameActive::TouchTriggers

Find all trigger entities that ent's current position touches.
Spectators will only interact with teleporters.
============
*/
void idSGameActive::TouchTriggers(gentity_t *ent) {
    sint i, num, touch[MAX_GENTITIES];
    gentity_t *hit;
    trace_t trace;
    vec3_t mins, maxs, pmins, pmaxs;
    static vec3_t range = { 10, 10, 10 };

    if(!ent->client) {
        return;
    }

    // dead clients don't activate triggers!
    if(ent->client->ps.stats[STAT_HEALTH] <= 0) {
        return;
    }

    bggame->ClassBoundingBox((class_t)ent->client->ps.stats[ STAT_CLASS ],
                             pmins, pmaxs, nullptr, nullptr, nullptr);

    VectorAdd(ent->client->ps.origin, pmins, mins);
    VectorAdd(ent->client->ps.origin, pmaxs, maxs);

    VectorSubtract(mins, range, mins);
    VectorAdd(maxs, range, maxs);

    num = trap_EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);

    // can't use ent->absmin, because that has a one unit pad
    VectorAdd(ent->client->ps.origin, ent->r.mins, mins);
    VectorAdd(ent->client->ps.origin, ent->r.maxs, maxs);

    for(i = 0; i < num; i++) {
        hit = &g_entities[ touch[ i ] ];

        if(!hit->touch && !ent->touch) {
            continue;
        }

        if(!(hit->r.contents & CONTENTS_TRIGGER)) {
            continue;
        }

        // ignore most entities if a spectator
        if((ent->client->sess.spectatorState != SPECTATOR_NOT) ||
                (ent->client->ps.stats[ STAT_STATE ] & SS_HOVELING)) {
            // this is ugly but adding a new ET_? type will
            // most likely cause network incompatibilities
            if(hit->s.eType != ET_TELEPORT_TRIGGER &&
                    hit->touch != idSGameMover::Touch_DoorTrigger) {
                //check for manually triggered doors
                idSGameMover::manualTriggerSpectator(hit, ent);
                continue;
            }
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
=================
idSGameActive::SpectatorThink
=================
*/
void idSGameActive::SpectatorThink(gentity_t *ent, usercmd_t *ucmd) {
    pmove_t pm;
    gclient_t *client;
    sint clientNum;
    bool attack1, attack3, following, queued;

    client = ent->client;

    client->oldbuttons = client->buttons;
    client->buttons = ucmd->buttons;

    attack1 = (client->buttons & BUTTON_ATTACK) &&
              !(client->oldbuttons & BUTTON_ATTACK);
    attack3 = (client->buttons & BUTTON_USE_HOLDABLE) &&
              !(client->oldbuttons & BUTTON_USE_HOLDABLE);

    // We are in following mode only if we are following a non-spectating client
    following = client->sess.spectatorState == SPECTATOR_FOLLOW;

    if(following) {
        clientNum = client->sess.spectatorClient;

        if(clientNum < 0 || clientNum > level.maxclients ||
                (!g_entities[clientNum].client &&
                 !level.clients[clientNum].pers.demoClient) ||
                level.clients[clientNum].sess.spectatorState != SPECTATOR_NOT) {
            following = false;
        }
    }

    // Check to see if we are in the spawn queue
    if(client->pers.teamSelection == TEAM_ALIENS) {
        queued = idSGameMain::SearchSpawnQueue(&level.alienSpawnQueue,
                                               ent - g_entities);
    } else if(client->pers.teamSelection == TEAM_HUMANS) {
        queued = idSGameMain::SearchSpawnQueue(&level.humanSpawnQueue,
                                               ent - g_entities);
    } else {
        queued = false;
    }

    // Wants to get out of spawn queue
    if(attack1 && queued) {
        if(client->sess.spectatorState == SPECTATOR_FOLLOW) {
            idSGameCmds::StopFollowing(ent);
        }

        if(client->ps.stats[STAT_TEAM] == TEAM_ALIENS) {
            idSGameMain::RemoveFromSpawnQueue(&level.alienSpawnQueue,
                                              client->ps.clientNum);
        } else if(client->ps.stats[STAT_TEAM] == TEAM_HUMANS) {
            idSGameMain::RemoveFromSpawnQueue(&level.humanSpawnQueue,
                                              client->ps.clientNum);
        }

        client->pers.classSelection = PCL_NONE;
        client->ps.stats[ STAT_CLASS ] = PCL_NONE;
        client->ps.pm_flags &= ~PMF_QUEUED;
        queued = false;
    }

    // Wants to get into spawn queue
    else if(attack1) {
        if(client->sess.spectatorState == SPECTATOR_FOLLOW) {
            idSGameCmds::StopFollowing(ent);
        }

        if(client->pers.teamSelection == TEAM_NONE) {
            idSGameUtils::TriggerMenu(client->ps.clientNum, MN_TEAM);
        } else if(client->pers.teamSelection == TEAM_ALIENS) {
            idSGameUtils::TriggerMenu(client->ps.clientNum, MN_A_CLASS);
        } else if(client->pers.teamSelection == TEAM_HUMANS) {
            idSGameUtils::TriggerMenu(client->ps.clientNum, MN_H_SPAWN);
        }
    }

    // We are either not following anyone or following a spectator
    if(!following) {
        if(client->sess.spectatorState == SPECTATOR_LOCKED ||
                client->sess.spectatorState == SPECTATOR_FOLLOW) {
            client->ps.pm_type = PM_FREEZE;
        } else if(client->noclip) {
            client->ps.pm_type = PM_NOCLIP;
        } else {
            client->ps.pm_type = PM_SPECTATOR;
        }

        if(queued) {
            client->ps.pm_flags |= PMF_QUEUED;
        }

        client->ps.speed = client->pers.flySpeed;

        client->ps.stats[ STAT_STAMINA ] = 0;
        client->ps.stats[ STAT_MISC ] = 0;
        client->ps.stats[ STAT_BUILDABLE ] = 0;
        client->ps.stats[ STAT_CLASS ] = PCL_NONE;
        client->ps.weapon = WP_NONE;

        // Set up for pmove
        ::memset(&pm, 0, sizeof(pm));
        pm.ps = &client->ps;
        pm.cmd = *ucmd;
        pm.tracemask = MASK_DEADSOLID; // spectators can fly through bodies
        pm.trace = trap_Trace;
        pm.pointcontents = trap_PointContents;

        // Perform a pmove
        bggame->Pmove(&pm);

        // Save results of pmove
        VectorCopy(client->ps.origin, ent->s.origin);

        TouchTriggers(ent);
        trap_UnlinkEntity(ent);

        // Set the queue position and spawn count for the client side
        if(client->ps.pm_flags & PMF_QUEUED) {
            if(client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS) {
                client->ps.persistant[ PERS_QUEUEPOS ] = idSGameMain::GetPosInSpawnQueue(
                            &level.alienSpawnQueue, client->ps.clientNum);
                client->ps.persistant[ PERS_SPAWNS ] = level.numAlienSpawns;
            } else if(client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS) {
                client->ps.persistant[ PERS_QUEUEPOS ] = idSGameMain::GetPosInSpawnQueue(
                            &level.humanSpawnQueue, client->ps.clientNum);
                client->ps.persistant[ PERS_SPAWNS ] = level.numHumanSpawns;
            }
        }
    }
}

/*
=================
idSGameActive::ClientInactivityTimer

Returns false if the client is dropped
=================
*/
bool idSGameActive::ClientInactivityTimer(gclient_t *client) {
    if(!g_inactivity.integer) {
        // give everyone some time, so if the operator sets g_inactivity during
        // gameplay, everyone isn't kicked
        client->inactivityTime = level.time + 60 * 1000;
        client->inactivityWarning = false;
    } else if(client->pers.cmd.forwardmove || client->pers.cmd.rightmove ||
              client->pers.cmd.upmove || (client->pers.cmd.buttons & BUTTON_ATTACK)) {
        client->inactivityTime = level.time + g_inactivity.integer * 1000;
        client->inactivityWarning = false;
    } else if(!client->pers.localClient) {
        if(level.time > client->inactivityTime) {
            trap_DropClient(client - level.clients, "Dropped due to inactivity", 0);
            return false;
        }

        if(level.time > client->inactivityTime - 10000 &&
                !client->inactivityWarning) {
            client->inactivityWarning = true;
            trap_SendServerCommand(client - level.clients,
                                   "cp \"Ten seconds until inactivity drop!\n\"");
        }
    }

    return true;
}

/*
==================
idSGameActive::ClientTimerActions

Actions that happen once a second
==================
*/
void idSGameActive::ClientTimerActions(gentity_t *ent, sint msec) {
    gclient_t *client;
    usercmd_t *ucmd;
    sint aForward, aRight, i;
    bool walking = false, stopped = false, crouched = false, jumping = false,
         strafing = false;

    ucmd = &ent->client->pers.cmd;

    aForward = fabs(ucmd->forwardmove);
    aRight = fabs(ucmd->rightmove);

    if(aForward == 0 && aRight == 0) {
        stopped = true;
    } else if(aForward <= 64 && aRight <= 64) {
        walking = true;
    }

    if(aRight > 0) {
        strafing = true;
    }

    if(ucmd->upmove > 0) {
        jumping = true;
    } else if(ent->client->ps.pm_flags & PMF_DUCKED) {
        crouched = true;
    }

    client = ent->client;
    client->time100 += msec;
    client->time1000 += msec;

    while(client->time100 >= 100) {
        weapon_t weapon = bggame->GetPlayerWeapon(&client->ps);

        client->time100 -= 100;

        // Restore or subtract stamina
        if(stopped || client->ps.pm_type == PM_JETPACK) {
            client->ps.stats[STAT_STAMINA] += STAMINA_STOP_RESTORE;
        } else if(client->ps.stats[STAT_STATE] & SS_SPEEDBOOST) {
            client->ps.stats[STAT_STAMINA] -= STAMINA_SPRINT_TAKE;
        } else if(walking || crouched) {
            client->ps.stats[STAT_STAMINA] += STAMINA_WALK_RESTORE;
        }

        // Check stamina limits
        if(client->ps.stats[STAT_STAMINA] > MAX_STAMINA) {
            client->ps.stats[STAT_STAMINA] = MAX_STAMINA;
        } else if(client->ps.stats[STAT_STAMINA] < -MAX_STAMINA) {
            client->ps.stats[STAT_STAMINA] = -MAX_STAMINA;
        }

        // Update build timer
        if(weapon == WP_ABUILD || weapon == WP_ABUILD2 ||
                bggame->InventoryContainsWeapon(WP_HBUILD, client->ps.stats)) {
            if(client->ps.stats[STAT_MISC] > 0) {
                client->ps.stats[STAT_MISC] -= 100;
            }

            if(client->ps.stats[STAT_MISC] < 0) {
                client->ps.stats[STAT_MISC] = 0;
            }
        }

        switch(weapon) {
            case WP_ABUILD:
            case WP_ABUILD2:
            case WP_HBUILD:

                // Set validity bit on buildable
                if((client->ps.stats[STAT_BUILDABLE] & ~SB_VALID_TOGGLEBIT) > BA_NONE) {
                    sint dist = bggame->Class((class_t)
                                              ent->client->ps.stats[STAT_CLASS])->buildDist;
                    vec3_t dummy;

                    if(idSGameBuildable::CanBuild(ent,
                                                  (buildable_t)(client->ps.stats[STAT_BUILDABLE] & ~SB_VALID_TOGGLEBIT),
                                                  dist, dummy) == IBE_NONE) {
                        client->ps.stats[STAT_BUILDABLE] |= SB_VALID_TOGGLEBIT;
                    } else {
                        client->ps.stats[STAT_BUILDABLE] &= ~SB_VALID_TOGGLEBIT;
                    }

                    // Let the client know which buildables will be removed by building
                    for(i = 0; i < MAX_MISC; i++) {
                        if(i < level.numBuildablesForRemoval) {
                            client->ps.misc[i] = level.markedBuildables[i]->s.number;
                        } else {
                            client->ps.misc[i] = 0;
                        }
                    }
                } else {
                    for(i = 0; i < MAX_MISC; i++) {
                        client->ps.misc[i] = 0;
                    }
                }

                break;

            default:
                break;
        }

        if(ent->client->pers.teamSelection == TEAM_HUMANS &&
                (client->ps.stats[ STAT_STATE ] & SS_HEALING_2X)) {
            sint remainingStartupTime = MEDKIT_STARTUP_TIME - (level.time -
                                        client->lastMedKitTime);

            if(remainingStartupTime < 0) {
                if(ent->health < ent->client->ps.stats[ STAT_MAX_HEALTH ] &&
                        ent->client->medKitHealthToRestore && ent->client->ps.pm_type != PM_DEAD) {
                    ent->client->medKitHealthToRestore--;
                    ent->health++;
                } else {
                    ent->client->ps.stats[STAT_STATE] &= ~SS_HEALING_2X;
                }
            } else {
                if(ent->health < ent->client->ps.stats[ STAT_MAX_HEALTH ] &&
                        ent->client->medKitHealthToRestore && ent->client->ps.pm_type != PM_DEAD) {
                    //partial increase
                    if(level.time > client->medKitIncrementTime) {
                        ent->client->medKitHealthToRestore--;
                        ent->health++;

                        client->medKitIncrementTime = level.time + (remainingStartupTime /
                                                      MEDKIT_STARTUP_SPEED);
                    }
                } else {
                    ent->client->ps.stats[STAT_STATE] &= ~SS_HEALING_2X;
                }
            }
        }
    }

    while(client->time1000 >= 1000) {
        client->time1000 -= 1000;

        //client is poisoned
        if(client->ps.stats[ STAT_STATE ] & SS_POISONED) {
            sint damage = ALIEN_POISON_DMG;

            if(bggame->InventoryContainsUpgrade(UP_BATTLESUIT, client->ps.stats)) {
                damage -= BSUIT_POISON_PROTECTION;
            }

            if(bggame->InventoryContainsUpgrade(UP_HELMET, client->ps.stats)) {
                damage -= HELMET_POISON_PROTECTION;
            }

            if(bggame->InventoryContainsUpgrade(UP_LIGHTARMOUR, client->ps.stats)) {
                damage -= LIGHTARMOUR_POISON_PROTECTION;
            }

            idSGameCombat::Damage(ent, client->lastPoisonClient,
                                  client->lastPoisonClient, nullptr, 0, damage, 0, MOD_POISON);
        }

        // turn off life support when a team admits defeat
        if(client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS &&
                level.surrenderTeam == TEAM_ALIENS) {
            idSGameCombat::Damage(ent, nullptr, nullptr, nullptr, nullptr,
                                  bggame->Class((class_t)client->ps.stats[ STAT_CLASS ])->regenRate,
                                  DAMAGE_NO_ARMOR, MOD_SUICIDE);
        } else if(client->ps.stats[ STAT_TEAM ] == TEAM_HUMANS &&
                  level.surrenderTeam == TEAM_HUMANS) {
            idSGameCombat::Damage(ent, nullptr, nullptr, nullptr, nullptr, 5,
                                  DAMAGE_NO_ARMOR, MOD_SUICIDE);
        }

        // lose some voice enthusiasm
        if(client->voiceEnthusiasm > 0.0f) {
            client->voiceEnthusiasm -= VOICE_ENTHUSIASM_DECAY;
        } else {
            client->voiceEnthusiasm = 0.0f;
        }
    }

    // Regenerate Adv. Dragoon barbs
    if(client->ps.weapon == WP_ALEVEL3_UPG) {
        if(client->ps.ammo < bggame->Weapon(WP_ALEVEL3_UPG)->maxAmmo) {
            if(ent->timestamp + LEVEL3_BOUNCEBALL_REGEN < level.time) {
                client->ps.ammo++;
                ent->timestamp = level.time;
            }
        } else {
            ent->timestamp = level.time;
        }
    }
}

/*
====================
idSGameActive::ClientIntermissionThink
====================
*/
void idSGameActive::ClientIntermissionThink(gclient_t *client) {
    client->ps.eFlags &= ~EF_FIRING;
    client->ps.eFlags &= ~EF_FIRING2;

    // the level will exit when everyone wants to or after timeouts

    // swap and latch button actions
    client->oldbuttons = client->buttons;
    client->buttons = client->pers.cmd.buttons;

    if(client->buttons & (BUTTON_ATTACK | BUTTON_USE_HOLDABLE) &
            (client->oldbuttons ^ client->buttons)) {
        client->readyToExit = 1;
    }
}


/*
================
idSGameActive::ClientEvents

Events will be passed on to the clients for presentation,
but any server game effects are handled here
================
*/
void idSGameActive::ClientEvents(gentity_t *ent, sint oldEventSequence) {
    sint i, _event, damage;
    gclient_t *client;
    vec3_t dir, point, mins;
    float32 fallDistance;
    class_t _class;

    client = ent->client;
    _class = (class_t)client->ps.stats[ STAT_CLASS ];

    if(oldEventSequence < client->ps.eventSequence - MAX_EVENTS) {
        oldEventSequence = client->ps.eventSequence - MAX_EVENTS;
    }

    for(i = oldEventSequence; i < client->ps.eventSequence; i++) {
        _event = client->ps.events[ i & (MAX_EVENTS - 1) ];

        switch(_event) {
            case EV_FALL_MEDIUM:
            case EV_FALL_FAR:
                if(ent->s.eType != ET_PLAYER) {
                    break;    // not in the player model
                }

                fallDistance = ((float32)client->ps.stats[ STAT_FALLDIST ] -
                                MIN_FALL_DISTANCE) / (MAX_FALL_DISTANCE - MIN_FALL_DISTANCE);

                if(fallDistance < 0.0f) {
                    fallDistance = 0.0f;
                } else if(fallDistance > 1.0f) {
                    fallDistance = 1.0f;
                }

                damage = (sint)((float32)bggame->Class(_class)->health * bggame->Class(
                                    _class)->fallDamage * fallDistance);

                VectorSet(dir, 0, 0, 1);
                bggame->ClassBoundingBox(_class, mins, nullptr, nullptr, nullptr, nullptr);
                mins[ 0 ] = mins[ 1 ] = 0.0f;
                VectorAdd(client->ps.origin, mins, point);

                ent->pain_debounce_time = level.time + 200; // no normal pain sound
                idSGameCombat::Damage(ent, nullptr, nullptr, dir, point, damage,
                                      DAMAGE_NO_LOCDAMAGE, MOD_FALLING);
                break;

            case EV_FIRE_WEAPON:
                idSGameWeapons::FireWeapon(ent);
                break;

            case EV_FIRE_WEAPON2:
                idSGameWeapons::FireWeapon2(ent);
                break;

            case EV_FIRE_WEAPON3:
                idSGameWeapons::FireWeapon3(ent);
                break;

            case EV_NOAMMO:
                break;

            default:
                break;
        }
    }
}

/*
==============
idSGameActive::SendPendingPredictableEvents
==============
*/
void idSGameActive::SendPendingPredictableEvents(playerState_t *ps) {
    gentity_t *t;
    sint _event, seq, extEvent, number;

    // if there are still events pending
    if(ps->entityEventSequence < ps->eventSequence) {
        // create a temporary entity for this event which is sent to everyone
        // except the client who generated the event
        seq = ps->entityEventSequence & (MAX_EVENTS - 1);
        _event = ps->events[ seq ] | ((ps->entityEventSequence & 3) << 8);

        // set external event to zero before calling bggame->PlayerStateToEntityState
        extEvent = ps->externalEvent;
        ps->externalEvent = 0;

        // create temporary entity for event
        t = idSGameUtils::TempEntity(ps->origin, _event);
        number = t->s.number;

        bggame->PlayerStateToEntityState(ps, &t->s, true);

        t->s.number = number;
        t->s.eType = (entityType_t)(ET_EVENTS + _event);
        t->s.eFlags |= EF_PLAYER_EVENT;
        t->s.otherEntityNum = ps->clientNum;

        // send to everyone except the client who generated the event
        t->r.svFlags |= SVF_NOTSINGLECLIENT;
        t->r.singleClient = ps->clientNum;

        // set back external event
        ps->externalEvent = extEvent;
    }
}

/*
==============
idSGameActive::UnlaggedStore

Called on every server frame.  Stores position data for the client at that
into client->unlaggedHist[] and the time into level.unlaggedTimes[].
This data is used by idSGameActive::UnlaggedCalc()
==============
*/
void idSGameActive::UnlaggedStore(void) {
    sint i = 0;
    gentity_t *ent;
    unlagged_t *save;

    if(!g_unlagged.integer) {
        return;
    }

    level.unlaggedIndex++;

    if(level.unlaggedIndex >= MAX_UNLAGGED_MARKERS) {
        level.unlaggedIndex = 0;
    }

    level.unlaggedTimes[ level.unlaggedIndex ] = level.time;

    for(i = 0; i < level.maxclients; i++) {
        ent = &g_entities[ i ];
        save = &ent->client->unlaggedHist[ level.unlaggedIndex ];
        save->used = false;

        if(!ent->r.linked || !(ent->r.contents & CONTENTS_BODY)) {
            continue;
        }

        if(ent->client->pers.connected != CON_CONNECTED) {
            continue;
        }

        VectorCopy(ent->r.mins, save->mins);
        VectorCopy(ent->r.maxs, save->maxs);
        VectorCopy(ent->s.pos.trBase, save->origin);
        save->used = true;
    }
}

/*
==============
idSGameActive::UnlaggedClear

Mark all unlaggedHist[] markers for this client invalid.  Useful for
preventing teleporting and death.
==============
*/
void idSGameActive::UnlaggedClear(gentity_t *ent) {
    sint i;

    for(i = 0; i < MAX_UNLAGGED_MARKERS; i++) {
        ent->client->unlaggedHist[i].used = false;
    }
}

/*
==============
idSGameActive::UnlaggedCalc

Loops through all active clients and calculates their predicted position
for time then stores it in client->unlaggedCalc
==============
*/
void idSGameActive::UnlaggedCalc(sint time, gentity_t *rewindEnt) {
    sint i = 0;
    sint startIndex = level.unlaggedIndex;
    sint stopIndex = -1;
    sint frameMsec = 0;
    float32 lerp = 0.5f;
    gentity_t *ent;

    if(!g_unlagged.integer) {
        return;
    }

    // clear any calculated values from a previous run
    for(i = 0; i < level.maxclients; i++) {
        ent = &g_entities[ i ];
        ent->client->unlaggedCalc.used = false;
    }

    for(i = 0; i < MAX_UNLAGGED_MARKERS; i++) {
        if(level.unlaggedTimes[startIndex] <= time) {
            break;
        }

        stopIndex = startIndex;

        if(--startIndex < 0) {
            startIndex = MAX_UNLAGGED_MARKERS - 1;
        }
    }

    if(i == MAX_UNLAGGED_MARKERS) {
        // if we searched all markers and the oldest one still isn't old enough
        // just use the oldest marker with no lerping
        lerp = 0.0f;
    }

    // client is on the current frame, no need for unlagged
    if(stopIndex == -1) {
        return;
    }

    // lerp between two markers
    frameMsec = level.unlaggedTimes[ stopIndex ] -
                level.unlaggedTimes[ startIndex ];

    if(frameMsec > 0) {
        lerp = (float32)(time - level.unlaggedTimes[ startIndex ]) /
               (float32)frameMsec;
    }

    for(i = 0; i < level.maxclients; i++) {
        ent = &g_entities[ i ];

        if(ent == rewindEnt) {
            continue;
        }

        if(!ent->r.linked || !(ent->r.contents & CONTENTS_BODY)) {
            continue;
        }

        if(ent->client->pers.connected != CON_CONNECTED) {
            continue;
        }

        //if( ent->client->ps.stats[ STAT_STATE ] & SS_GRABBED )
        //  continue;

        if(!ent->client->unlaggedHist[startIndex].used) {
            continue;
        }

        if(!ent->client->unlaggedHist[stopIndex].used) {
            continue;
        }

        // between two unlagged markers
        VectorLerpTrem(lerp, ent->client->unlaggedHist[ startIndex ].mins,
                       ent->client->unlaggedHist[ stopIndex ].mins,
                       ent->client->unlaggedCalc.mins);
        VectorLerpTrem(lerp, ent->client->unlaggedHist[ startIndex ].maxs,
                       ent->client->unlaggedHist[ stopIndex ].maxs,
                       ent->client->unlaggedCalc.maxs);
        VectorLerpTrem(lerp, ent->client->unlaggedHist[ startIndex ].origin,
                       ent->client->unlaggedHist[ stopIndex ].origin,
                       ent->client->unlaggedCalc.origin);

        ent->client->unlaggedCalc.used = true;
    }
}

/*
==============
idSGameActive::UnlaggedOff

Reverses the changes made to all active clients by idSGameActive::UnlaggedOn()
==============
*/
void idSGameActive::UnlaggedOff(void) {
    sint i = 0;
    gentity_t *ent;

    if(!g_unlagged.integer) {
        return;
    }

    for(i = 0; i < level.maxclients; i++) {
        ent = &g_entities[ i ];

        if(!ent->client->unlaggedBackup.used) {
            continue;
        }

        VectorCopy(ent->client->unlaggedBackup.mins, ent->r.mins);
        VectorCopy(ent->client->unlaggedBackup.maxs, ent->r.maxs);
        VectorCopy(ent->client->unlaggedBackup.origin, ent->r.currentOrigin);
        ent->client->unlaggedBackup.used = false;
        trap_LinkEntity(ent);
    }
}

/*
==============
idSGameActive::UnlaggedOn

Called after idSGameActive::UnlaggedCalc() to apply the calculated values to all active
clients.  Once finished tracing, idSGameActive::UnlaggedOff() must be called to restore
the clients' position data

As an optimization, all clients that have an unlagged position that is
not touchable at "range" from "muzzle" will be ignored.  This is required
to prevent a huge amount of trap_LinkEntity() calls per user cmd.
==============
*/

void idSGameActive::UnlaggedOn(gentity_t *attacker, vec3_t muzzle,
                               float32 range) {
    sint i = 0;
    gentity_t *ent;
    unlagged_t *calc;

    if(!g_unlagged.integer) {
        return;
    }

    if(!attacker->client->pers.useUnlagged) {
        return;
    }

    for(i = 0; i < level.maxclients; i++) {
        ent = &g_entities[ i ];
        calc = &ent->client->unlaggedCalc;

        if(!calc->used) {
            continue;
        }

        if(ent->client->unlaggedBackup.used) {
            continue;
        }

        if(!ent->r.linked || !(ent->r.contents & CONTENTS_BODY)) {
            continue;
        }

        if(VectorCompare(ent->r.currentOrigin, calc->origin)) {
            continue;
        }

        if(muzzle) {
            float32 r1 = Distance(calc->origin, calc->maxs);
            float32 r2 = Distance(calc->origin, calc->mins);
            float32 maxRadius = (r1 > r2) ? r1 : r2;

            if(Distance(muzzle, calc->origin) > range + maxRadius) {
                continue;
            }
        }

        // create a backup of the real positions
        VectorCopy(ent->r.mins, ent->client->unlaggedBackup.mins);
        VectorCopy(ent->r.maxs, ent->client->unlaggedBackup.maxs);
        VectorCopy(ent->r.currentOrigin, ent->client->unlaggedBackup.origin);
        ent->client->unlaggedBackup.used = true;

        // move the client to the calculated unlagged position
        VectorCopy(calc->mins, ent->r.mins);
        VectorCopy(calc->maxs, ent->r.maxs);
        VectorCopy(calc->origin, ent->r.currentOrigin);
        trap_LinkEntity(ent);
    }
}

/*
==============
idSGameActive::G_UnlaggedDetectCollisions

cgame prediction will predict a client's own position all the way up to
the current time, but only updates other player's positions up to the
postition sent in the most recent snapshot.

This allows player X to essentially "move through" the position of player Y
when player X's cmd is processed with Pmove() on the server.  This is because
player Y was clipping player X's Pmove() on his client, but when the same
cmd is processed with Pmove on the server it is not clipped.

Long story short (too late): don't use unlagged positions for players who
were blocking this player X's client-side Pmove().  This makes the assumption
that if player X's movement was blocked in the client he's going to still
be up against player Y when the Pmove() is run on the server with the
same cmd.

NOTE: this must be called after Pmove() and idSGameActive::UnlaggedCalc()
==============
*/
void idSGameActive::UnlaggedDetectCollisions(gentity_t *ent) {
    unlagged_t *calc;
    trace_t tr;
    float32 r1, r2, range;

    if(!g_unlagged.integer) {
        return;
    }

    if(!ent->client->pers.useUnlagged) {
        return;
    }

    calc = &ent->client->unlaggedCalc;

    // if the client isn't moving, this is not necessary
    if(VectorCompare(ent->client->oldOrigin, ent->client->ps.origin)) {
        return;
    }

    range = Distance(ent->client->oldOrigin, ent->client->ps.origin);

    // increase the range by the player's largest possible radius since it's
    // the players bounding box that collides, not their origin
    r1 = Distance(calc->origin, calc->mins);
    r2 = Distance(calc->origin, calc->maxs);
    range += (r1 > r2) ? r1 : r2;

    UnlaggedOn(ent, ent->client->oldOrigin, range);

    trap_Trace(&tr, ent->client->oldOrigin, ent->r.mins, ent->r.maxs,
               ent->client->ps.origin, ent->s.number,  MASK_PLAYERSOLID);

    if(tr.entityNum >= 0 && tr.entityNum < MAX_CLIENTS) {
        g_entities[tr.entityNum].client->unlaggedCalc.used = false;
    }

    UnlaggedOff();
}

/*
==============
idSGameActive::CheckZap
==============
*/
void idSGameActive::CheckZap(gentity_t *ent) {
    sint i;

    if(!ent->zapping) {
        // clear out established targets
        for(i = 0; i < LEVEL2_AREAZAP_MAX_TARGETS; i++) {
            ent->zapTargets[ i ] = -1;
        }

        ent->zapDmg = 0.0f;
    }

    ent->wasZapping = ent->zapping;
    ent->zapping = false;

    if(ent->client->ps.weapon == WP_ALEVEL2_UPG &&
            (ent->client->pers.cmd.buttons & BUTTON_ATTACK2)) {
        ent->zapping = true;
    }

    if(ent->wasZapping && !ent->zapping) {
        ent->client->ps.weaponTime = LEVEL2_AREAZAP_REPEAT;
    }
}

/*
==============
idSGameActive::ClientThink_real

This will be called once for each client frame, which will
usually be a couple times for each server frame on fast clients.

If "g_synchronousClients 1" is set, this will be called exactly
once for each server frame, which makes for smooth demo recording.
==============
*/
void idSGameActive::ClientThink_real(gentity_t *ent) {
    gclient_t *client;
    pmove_t pm;
    sint oldEventSequence, msec;
    usercmd_t *ucmd;

    client = ent->client;

    // don't think if the client is not yet connected (and thus not yet spawned in)
    if(client->pers.connected != CON_CONNECTED) {
        return;
    }

    // mark the time, so the connection sprite can be removed
    ucmd = &ent->client->pers.cmd;

    // sanity check the command time to prevent speedup cheating
    if(ucmd->serverTime > level.time + 200) {
        ucmd->serverTime = level.time + 200;
    }

    if(ucmd->serverTime < level.time - 1000) {
        ucmd->serverTime = level.time - 1000;
    }

    msec = ucmd->serverTime - client->ps.commandTime;

    // following others may result in bad times, but we still want
    // to check for follow toggles
    if(msec < 1 && client->sess.spectatorState != SPECTATOR_FOLLOW) {
        return;
    }

    if(msec > 200) {
        msec = 200;
    }

    client->unlaggedTime = ucmd->serverTime;

    if(pmove_msec.integer < 8) {
        trap_Cvar_Set("pmove_msec", "8");
        trap_Cvar_Update(&pmove_msec);
    } else if(pmove_msec.integer > 33) {
        trap_Cvar_Set("pmove_msec", "33");
        trap_Cvar_Update(&pmove_msec);
    }

    if(pmove_fixed.integer || client->pers.pmoveFixed) {
        ucmd->serverTime = ((ucmd->serverTime + pmove_msec.integer - 1) /
                            pmove_msec.integer) * pmove_msec.integer;
        //if (ucmd->serverTime - client->ps.commandTime <= 0)
        //  return;
    }

    // check for exiting intermission
    if(level.intermissiontime) {
        ClientIntermissionThink(client);
        return;
    }

    // spectators don't do much
    if(client->sess.spectatorState != SPECTATOR_NOT) {
        if(client->sess.spectatorState == SPECTATOR_SCOREBOARD) {
            return;
        }

        SpectatorThink(ent, ucmd);
        return;
    }

    idSGamePtr::UpdatePTRConnection(client);

    // check for inactivity timer, but never drop the local client of a non-dedicated server
    if(!ClientInactivityTimer(client)) {
        return;
    }

    // calculate where ent is currently seeing all the other active clients
    UnlaggedCalc(ent->client->unlaggedTime, ent);

    if(client->noclip) {
        client->ps.pm_type = PM_NOCLIP;
    } else if(client->ps.stats[STAT_HEALTH] <= 0) {
        client->ps.pm_type = PM_DEAD;
    } else if(client->ps.stats[STAT_STATE] & SS_HOVELING) {
        client->ps.pm_type = PM_FREEZE;
    } else if(client->ps.stats[STAT_STATE] & SS_BLOBLOCKED ||
              client->ps.stats[STAT_STATE] & SS_GRABBED) {
        client->ps.pm_type = PM_GRABBED;
    } else if(bggame->InventoryContainsUpgrade(UP_JETPACK, client->ps.stats) &&
              bggame->UpgradeIsActive(UP_JETPACK, client->ps.stats)) {
        client->ps.pm_type = PM_JETPACK;
    } else {
        client->ps.pm_type = PM_NORMAL;
    }

    if((client->ps.stats[STAT_STATE] & SS_GRABBED) &&
            client->grabExpiryTime < level.time) {
        client->ps.stats[STAT_STATE] &= ~SS_GRABBED;
    }

    if((client->ps.stats[STAT_STATE] & SS_BLOBLOCKED) &&
            client->lastLockTime + LOCKBLOB_LOCKTIME < level.time) {
        client->ps.stats[STAT_STATE] &= ~SS_BLOBLOCKED;
    }

    if((client->ps.stats[STAT_STATE] & SS_SLOWLOCKED) &&
            client->lastSlowTime + ABUILDER_BLOB_TIME < level.time) {
        client->ps.stats[STAT_STATE] &= ~SS_SLOWLOCKED;
    }

    // Update boosted state flags
    client->ps.stats[ STAT_STATE ] &= ~SS_BOOSTEDWARNING;

    if(client->ps.stats[ STAT_STATE ] & SS_BOOSTED) {
        if(level.time - client->boostedTime >= BOOST_TIME) {
            client->ps.stats[STAT_STATE] &= ~SS_BOOSTED;
        } else if(level.time - client->boostedTime >= BOOST_WARN_TIME) {
            client->ps.stats[STAT_STATE] |= SS_BOOSTEDWARNING;
        }
    }

    // Check if poison cloud has worn off
    if((client->ps.eFlags & EF_POISONCLOUDED) &&
            bggame->PlayerPoisonCloudTime(&client->ps) - level.time +
            client->lastPoisonCloudedTime <= 0) {
        client->ps.eFlags &= ~EF_POISONCLOUDED;
    }

    if(client->ps.stats[STAT_STATE] & SS_POISONED &&
            client->lastPoisonTime + ALIEN_POISON_TIME < level.time) {
        client->ps.stats[STAT_STATE] &= ~SS_POISONED;
    }

    client->ps.gravity = g_gravity.value;

    if(bggame->InventoryContainsUpgrade(UP_MEDKIT, client->ps.stats) &&
            bggame->UpgradeIsActive(UP_MEDKIT, client->ps.stats)) {
        //if currently using a medkit or have no need for a medkit now
        if(client->ps.stats[ STAT_STATE ] & SS_HEALING_2X ||
                (client->ps.stats[ STAT_HEALTH ] == client->ps.stats[ STAT_MAX_HEALTH ] &&
                 !(client->ps.stats[ STAT_STATE ] & SS_POISONED))) {
            bggame->DeactivateUpgrade(UP_MEDKIT, client->ps.stats);
        } else if(client->ps.stats[ STAT_HEALTH ] > 0) {
            //remove anti toxin
            bggame->DeactivateUpgrade(UP_MEDKIT, client->ps.stats);
            bggame->RemoveUpgradeFromInventory(UP_MEDKIT, client->ps.stats);

            client->ps.stats[ STAT_STATE ] &= ~SS_POISONED;
            client->poisonImmunityTime = level.time + MEDKIT_POISON_IMMUNITY_TIME;
            client->ps.stats[ STAT_STATE ] |= SS_HEALING_2X;
            client->lastMedKitTime = level.time;
            client->medKitHealthToRestore = client->ps.stats[ STAT_MAX_HEALTH ] -
                                            client->ps.stats[ STAT_HEALTH ];
            client->medKitIncrementTime = level.time + (MEDKIT_STARTUP_TIME /
                                          MEDKIT_STARTUP_SPEED);

            idSGameUtils::AddEvent(ent, EV_MEDKIT_USED, 0);
        }
    }

    // Replenish alien health
    if(level.surrenderTeam != client->pers.teamSelection &&
            ent->nextRegenTime >= 0 && ent->nextRegenTime < level.time) {
        float32 regenRate = bggame->Class((class_t)
                                          ent->client->ps.stats[ STAT_CLASS ])->regenRate;

        if(ent->health <= 0 || ent->nextRegenTime < 0 || regenRate == 0) {
            ent->nextRegenTime = -1; // no regen
        } else {
            sint entityList[ MAX_GENTITIES ];
            sint i, num, count, interval;
            vec3_t range, mins, maxs;
            float32 modifier = 1.0f;

            VectorSet(range, REGEN_BOOST_RANGE, REGEN_BOOST_RANGE, REGEN_BOOST_RANGE);
            VectorAdd(client->ps.origin, range, maxs);
            VectorSubtract(client->ps.origin, range, mins);

            num = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

            for(i = 0; i < num; i++) {
                gentity_t *boost = &g_entities[ entityList[ i ] ];

                if(modifier < BOOSTER_REGEN_MOD && boost->s.eType == ET_BUILDABLE &&
                        boost->s.modelindex == BA_A_BOOSTER && boost->spawned &&
                        boost->health > 0 && level.overmindPresent) {
                    modifier = BOOSTER_REGEN_MOD;
                    continue;
                }

                if(boost->s.eType == ET_PLAYER && boost->client &&
                        boost->client->pers.teamSelection == ent->client->pers.teamSelection &&
                        boost->health > 0) {
                    class_t _class = (class_t)boost->client->ps.stats[ STAT_CLASS ];

                    if(_class == PCL_ALIEN_LEVEL1 && modifier < LEVEL1_REGEN_MOD) {
                        modifier = LEVEL1_REGEN_MOD;
                    } else if(_class == PCL_ALIEN_LEVEL1_UPG &&
                              modifier < LEVEL1_UPG_REGEN_MOD) {
                        modifier = LEVEL1_UPG_REGEN_MOD;
                    }
                }
            }

            // Transmit heal rate to the client so it can be displayed on the HUD
            client->ps.stats[ STAT_STATE ] |= SS_HEALING_ACTIVE;
            client->ps.stats[ STAT_STATE ] &= ~(SS_HEALING_2X | SS_HEALING_3X);

            if(modifier == 1.0f && !idSGameBuildable::FindCreep(ent)) {
                client->ps.stats[ STAT_STATE ] &= ~SS_HEALING_ACTIVE;
                modifier *= ALIEN_REGEN_NOCREEP_MOD;
            } else if(modifier >= 3.0f) {
                client->ps.stats[STAT_STATE] |= SS_HEALING_3X;
            } else if(modifier >= 2.0f) {
                client->ps.stats[STAT_STATE] |= SS_HEALING_2X;
            }

            interval = 1000 / (regenRate * modifier);
            // if recovery interval is less than frametime, compensate
            count = 1 + (level.time - ent->nextRegenTime) / interval;

            ent->health += count;
            ent->nextRegenTime += count * interval;

            // if at max health, clear damage counters
            if(ent->health >= client->ps.stats[ STAT_MAX_HEALTH ]) {
                ent->health = client->ps.stats[ STAT_MAX_HEALTH ];

                for(i = 0; i < MAX_CLIENTS; i++) {
                    ent->credits[i] = 0;
                }
            }
        }
    }

    if(bggame->InventoryContainsUpgrade(UP_GRENADE, client->ps.stats) &&
            bggame->UpgradeIsActive(UP_GRENADE, client->ps.stats)) {
        sint lastWeapon = ent->s.weapon;

        //remove grenade
        bggame->DeactivateUpgrade(UP_GRENADE, client->ps.stats);
        bggame->RemoveUpgradeFromInventory(UP_GRENADE, client->ps.stats);

        //M-M-M-M-MONSTER HACK
        ent->s.weapon = WP_GRENADE;
        idSGameWeapons::FireWeapon(ent);
        ent->s.weapon = lastWeapon;
    }

    // set speed
    if(client->ps.pm_type == PM_NOCLIP) {
        client->ps.speed = client->pers.flySpeed;
    } else {
        client->ps.speed = g_speed.value * bggame->Class((class_t)
                           client->ps.stats[STAT_CLASS])->speed;
    }

    if(client->lastCreepSlowTime + CREEP_TIMEOUT < level.time) {
        client->ps.stats[STAT_STATE] &= ~SS_CREEPSLOWED;
    }

    //randomly disable the jet pack if damaged
    if(bggame->InventoryContainsUpgrade(UP_JETPACK, client->ps.stats) &&
            bggame->UpgradeIsActive(UP_JETPACK, client->ps.stats)) {
        if(ent->lastDamageTime + JETPACK_DISABLE_TIME > level.time) {
            if(random() > JETPACK_DISABLE_CHANCE) {
                client->ps.pm_type = PM_NORMAL;
            }
        }

        //switch jetpack off if no reactor
        if(!level.reactorPresent) {
            bggame->DeactivateUpgrade(UP_JETPACK, client->ps.stats);
        }
    }

    // set up for pmove
    oldEventSequence = client->ps.eventSequence;

    ::memset(&pm, 0, sizeof(pm));

    if(ent->flags & FL_FORCE_GESTURE) {
        ent->flags &= ~FL_FORCE_GESTURE;
        ent->client->pers.cmd.buttons |= BUTTON_GESTURE;
    }

    // clear fall velocity before every pmove
    client->pmext.fallVelocity = 0.f;

    pm.ps = &client->ps;
    pm.pmext = &client->pmext;
    pm.cmd = *ucmd;

    if(pm.ps->pm_type == PM_DEAD) {
        pm.tracemask = MASK_DEADSOLID;
    }

    if(pm.ps->stats[STAT_STATE] & SS_HOVELING) {
        pm.tracemask = MASK_DEADSOLID;
    } else {
        pm.tracemask = MASK_PLAYERSOLID;
    }

    pm.trace = trap_Trace;
    pm.pointcontents = trap_PointContents;
    pm.debugLevel = g_debugMove.integer;
    pm.noFootsteps = 0;

    pm.pmove_fixed = (bool)pmove_fixed.integer | client->pers.pmoveFixed;
    pm.pmove_msec = pmove_msec.integer;

    VectorCopy(client->ps.origin, client->oldOrigin);

    // moved from after Pmove -- potentially the cause of
    // future triggering bugs
    if(!ent->client->noclip) {
        TouchTriggers(ent);
    }

    bggame->Pmove(&pm);

    UnlaggedDetectCollisions(ent);

    // save results of pmove
    if(ent->client->ps.eventSequence != oldEventSequence) {
        ent->eventTime = level.time;
    }

    if(g_smoothClients.integer) {
        bggame->PlayerStateToEntityStateExtraPolate(&ent->client->ps, &ent->s,
                ent->client->ps.commandTime, true);
    } else {
        bggame->PlayerStateToEntityState(&ent->client->ps, &ent->s, true);
    }

    switch(client->ps.weapon) {
        case WP_ALEVEL0:
            if(!idSGameWeapons::CheckVenomAttack(ent)) {
                client->ps.weaponstate = WEAPON_READY;
            } else {
                client->ps.generic1 = WPM_PRIMARY;
                idSGameUtils::AddEvent(ent, EV_FIRE_WEAPON, 0);
            }

            break;

        case WP_ALEVEL1:
        case WP_ALEVEL1_UPG:
            idSGameWeapons::CheckGrabAttack(ent);
            break;

        case WP_ALEVEL3:
        case WP_ALEVEL3_UPG:
            if(!idSGameWeapons::CheckPounceAttack(ent)) {
                client->ps.weaponstate = WEAPON_READY;
            } else {
                client->ps.generic1 = WPM_SECONDARY;
                idSGameUtils::AddEvent(ent, EV_FIRE_WEAPON2, 0);
            }

            break;

        case WP_HBUILD:
            idSGameWeapons::CheckCkitRepair(ent);
            break;

        default:
            break;
    }

    SendPendingPredictableEvents(&ent->client->ps);

    if(!(ent->client->ps.eFlags & EF_FIRING)) {
        client->fireHeld = false;    // for grapple
    }

    if(!(ent->client->ps.eFlags & EF_FIRING2)) {
        client->fire2Held = false;
    }

    // use the snapped origin for linking so it matches client predicted versions
    VectorCopy(ent->s.pos.trBase, ent->r.currentOrigin);

    VectorCopy(pm.mins, ent->r.mins);
    VectorCopy(pm.maxs, ent->r.maxs);

    ent->waterlevel = pm.waterlevel;
    ent->watertype = pm.watertype;

    // touch other objects
    ClientImpacts(ent, &pm);

    CheckZap(ent);

    // execute client events
    ClientEvents(ent, oldEventSequence);

    // link entity now, after any personal teleporters have been used
    trap_LinkEntity(ent);

    // NOTE: now copy the exact origin over otherwise clients can be snapped into solid
    VectorCopy(ent->client->ps.origin, ent->r.currentOrigin);
    VectorCopy(ent->client->ps.origin, ent->s.origin);

    // save results of triggers and client events
    if(ent->client->ps.eventSequence != oldEventSequence) {
        ent->eventTime = level.time;
    }

    // Don't think anymore if dead
    if(client->ps.stats[STAT_HEALTH] <= 0) {
        return;
    }

    // swap and latch button actions
    client->oldbuttons = client->buttons;
    client->buttons = ucmd->buttons;
    client->latched_buttons |= client->buttons & ~client->oldbuttons;

    if((client->buttons & BUTTON_USE_EVOLVE) &&
            !(client->oldbuttons & BUTTON_USE_EVOLVE) &&
            client->ps.stats[ STAT_HEALTH ] > 0) {
        trace_t trace;
        vec3_t view, point;
        gentity_t *traceEnt;

        if(client->ps.stats[ STAT_STATE ] & SS_HOVELING) {
            gentity_t *hovel = client->hovel;

            //only let the player out if there is room
            if(!idSGameBuildable::AHovel_Blocked(hovel, ent, true)) {
                //prevent lerping
                client->ps.eFlags ^= EF_TELEPORT_BIT;
                client->ps.eFlags &= ~EF_NODRAW;
                UnlaggedClear(ent);

                //client leaves hovel
                client->ps.stats[ STAT_STATE ] &= ~SS_HOVELING;

                //hovel is empty
                idSGameBuildable::SetBuildableAnim(hovel, BANIM_ATTACK2, false);
                hovel->active = false;
            } else {
                //exit is blocked
                idSGameUtils::TriggerMenu(ent->client->ps.clientNum, MN_A_HOVEL_BLOCKED);
            }
        } else {
#define USE_OBJECT_RANGE 64

            sint entityList[ MAX_GENTITIES ];
            vec3_t range = { USE_OBJECT_RANGE, USE_OBJECT_RANGE, USE_OBJECT_RANGE };
            vec3_t mins, maxs;
            sint i, num;

            // look for object infront of player
            AngleVectors(client->ps.viewangles, view, nullptr, nullptr);
            VectorMA(client->ps.origin, USE_OBJECT_RANGE, view, point);
            trap_Trace(&trace, client->ps.origin, nullptr, nullptr, point,
                       ent->s.number, MASK_SHOT);

            traceEnt = &g_entities[ trace.entityNum ];

            if(traceEnt && traceEnt->buildableTeam == client->ps.stats[STAT_TEAM] &&
                    traceEnt->use) {
                traceEnt->use(traceEnt, ent,
                              ent);   //other and activator are the same in this context
            } else {
                //no entity in front of player - do a small area search

                VectorAdd(client->ps.origin, range, maxs);
                VectorSubtract(client->ps.origin, range, mins);

                num = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

                for(i = 0; i < num; i++) {
                    traceEnt = &g_entities[ entityList[ i ] ];

                    if(traceEnt && traceEnt->buildableTeam == client->ps.stats[ STAT_TEAM ] &&
                            traceEnt->use) {
                        traceEnt->use(traceEnt, ent,
                                      ent);   //other and activator are the same in this context
                        break;
                    }
                }

                if(i == num && client->ps.stats[ STAT_TEAM ] == TEAM_ALIENS) {
                    if(bggame->AlienCanEvolve((class_t)client->ps.stats[ STAT_CLASS ],
                                              client->ps.persistant[ PERS_CREDIT ], g_alienStage.integer)) {
                        //no nearby objects and alien - show class menu
                        idSGameUtils::TriggerMenu(ent->client->ps.clientNum, MN_A_INFEST);
                    } else {
                        //flash frags
                        idSGameUtils::AddEvent(ent, EV_ALIEN_EVOLVE_FAILED, 0);
                    }
                }
            }
        }
    }

    // Give clients some credit periodically
    if(ent->client->lastKillTime + FREEKILL_PERIOD < level.time) {
        if(idSGameMain::TimeTilSuddenDeath() <= 0) {
            //gotta love logic like this eh?
        } else if(ent->client->ps.stats[STAT_TEAM] == TEAM_ALIENS) {
            idSGameClient::AddCreditToClient(ent->client, FREEKILL_ALIEN, true);
        } else if(ent->client->ps.stats[STAT_TEAM] == TEAM_HUMANS) {
            idSGameClient::AddCreditToClient(ent->client, FREEKILL_HUMAN, true);
        }

        ent->client->lastKillTime = level.time;
    }

    // perform once-a-second actions
    ClientTimerActions(ent, msec);

    if(ent->suicideTime > 0 && ent->suicideTime < level.time) {
        ent->flags &= ~FL_GODMODE;
        ent->client->ps.stats[ STAT_HEALTH ] = ent->health = 0;
        idSGameCombat::player_die(ent, ent, ent, 100000, MOD_SUICIDE);

        ent->suicideTime = 0;
    }
}

/*
==================
idSGameLocal::ClientThink

A new command has arrived from the client
==================
*/
void idSGameLocal::ClientThink(sint clientNum) {
    gentity_t *ent;

    ent = g_entities + clientNum;
    trap_GetUsercmd(clientNum, &ent->client->pers.cmd);

    // mark the time we got info, so we can display the
    // phone jack if they don't get any for a while
    ent->client->lastCmdTime = level.time;

    if(!g_synchronousClients.integer) {
        idSGameActive::ClientThink_real(ent);
    }
}

/*
==================
idSGameActive::RunClient
==================
*/
void idSGameActive::RunClient(gentity_t *ent) {
    if(!g_synchronousClients.integer) {
        return;
    }

    ent->client->pers.cmd.serverTime = level.time;
    ClientThink_real(ent);
}


/*
==================
idSGameActive::SpectatorClientEndFrame
==================
*/
void idSGameActive::SpectatorClientEndFrame(gentity_t *ent) {
    gclient_t *cl;
    sint clientNum, flags, score, ping;

    // if we are doing a chase cam or a remote view, grab the latest info
    if(ent->client->sess.spectatorState == SPECTATOR_FOLLOW) {
        clientNum = ent->client->sess.spectatorClient;

        if(clientNum >= 0) {
            cl = &level.clients[ clientNum ];

            if(cl->pers.connected == CON_CONNECTED || cl->pers.demoClient) {
                flags = (cl->ps.eFlags & ~(EF_VOTED | EF_TEAMVOTED)) |
                        (ent->client->ps.eFlags & (EF_VOTED | EF_TEAMVOTED));
                score = ent->client->ps.persistant[ PERS_SCORE ];
                ping = ent->client->ps.ping;
                ent->client->ps = cl->ps;
                ent->client->ps.persistant[ PERS_SCORE ] = score;
                ent->client->ps.ping = ping;
                ent->client->ps.pm_flags |= PMF_FOLLOW;
                ent->client->ps.pm_flags &= ~PMF_QUEUED;
                ent->client->ps.eFlags = flags;
            }
        }
    }
}

/*
==============
idSGameActive::ClientEndFrame

Called at the end of each server frame for each connected client
A fast client will have multiple ClientThink for each ClientEdFrame,
while a slow client may have multiple v::ClientEndFrame between idSGameActive::ClientThink.
==============
*/
void idSGameActive::ClientEndFrame(gentity_t *ent) {
    clientPersistant_t  *pers;

    if(ent->client->sess.spectatorState != SPECTATOR_NOT) {
        SpectatorClientEndFrame(ent);
        return;
    }

    pers = &ent->client->pers;

    //
    // If the end of unit layout is displayed, don't give
    // the player any normal movement attributes
    //
    if(level.intermissiontime) {
        return;
    }

    // burn from lava, etc
    WorldEffects(ent);

    // apply all the damage taken this frame
    DamageFeedback(ent);

    // add the EF_CONNECTION flag if we haven't gotten commands recently
    if(level.time - ent->client->lastCmdTime > 1000) {
        ent->s.eFlags |= EF_CONNECTION;
    } else {
        ent->s.eFlags &= ~EF_CONNECTION;
    }

    ent->client->ps.stats[ STAT_HEALTH ] =
        ent->health; // FIXME: get rid of ent->health...

    // respawn if dead
    if(ent->client->ps.stats[STAT_HEALTH] <= 0 &&
            level.time >= ent->client->respawnTime) {
        idSGameClient::respawn(ent);
    }

    SetClientSound(ent);

    idSGameWeapons::UpdateZaps(ent);

    // set the latest infor
    if(g_smoothClients.integer) {
        bggame->PlayerStateToEntityStateExtraPolate(&ent->client->ps, &ent->s,
                ent->client->ps.commandTime, true);
    } else {
        bggame->PlayerStateToEntityState(&ent->client->ps, &ent->s, true);
    }

    SendPendingPredictableEvents(&ent->client->ps);
}


