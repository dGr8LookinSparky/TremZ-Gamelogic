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
// File name:   sgame_target.cpp
// Created:
// Compilers:   Microsoft (R) C/C++ Optimizing Compiler Version 19.26.28806 for x64,
//              gcc (Ubuntu 9.3.0-10ubuntu2) 9.3.0
// Description:
// -------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////

#include <sgame/sgame_precompiled.hpp>

/*
===============
idSGameTarget::idSGameTarget
===============
*/
idSGameTarget::idSGameTarget(void) {
}

/*
===============
idSGameTarget::~idSGameTarget
===============
*/
idSGameTarget::~idSGameTarget(void) {
}

//==========================================================

/*QUAKED target_delay (1 0 0) (-8 -8 -8) (8 8 8)
"wait" seconds to pause before firing targets.
"random" delay variance, total delay = delay +/- random seconds
*/
void idSGameTarget::Think_Target_Delay(gentity_t *ent) {
    idSGameUtils::UseTargets(ent, ent->activator);
}

void idSGameTarget::Use_Target_Delay(gentity_t *ent, gentity_t *other,
                                     gentity_t *activator) {
    ent->nextthink = level.time + (ent->wait + ent->random * crandom()) * 1000;
    ent->think = Think_Target_Delay;
    ent->activator = activator;
}

void idSGameTarget::SP_target_delay(gentity_t *ent) {
    // check delay for backwards compatability
    if(!idSGameSpawn::SpawnFloat("delay", "0", &ent->wait)) {
        idSGameSpawn::SpawnFloat("wait", "1", &ent->wait);
    }

    if(!ent->wait) {
        ent->wait = 1;
    }

    ent->use = Use_Target_Delay;
}


//==========================================================

/*QUAKED target_score (1 0 0) (-8 -8 -8) (8 8 8)
"count" number of points to add, default 1

The activator is given this many points.
*/
void idSGameTarget::Use_Target_Score(gentity_t *ent, gentity_t *other,
                                     gentity_t *activator) {
    idSGameCombat::AddScore(activator, ent->count);
}

void idSGameTarget::SP_target_score(gentity_t *ent) {
    if(!ent->count) {
        ent->count = 1;
    }

    ent->use = Use_Target_Score;
}


//==========================================================

/*QUAKED target_print (1 0 0) (-8 -8 -8) (8 8 8) humanteam alienteam private
"message" text to print
If "private", only the activator gets the message.  If no checks, all clients get the message.
*/
void idSGameTarget::Use_Target_Print(gentity_t *ent, gentity_t *other,
                                     gentity_t *activator) {
    if(activator->client && (ent->spawnflags & 4)) {
        trap_SendServerCommand(activator - g_entities, va("cp \"%s\"",
                               ent->message));
        return;
    }

    if(ent->spawnflags & 3) {
        if(ent->spawnflags & 1) {
            idSGameUtils::TeamCommand(TEAM_HUMANS, va("cp \"%s\"", ent->message));
        }

        if(ent->spawnflags & 2) {
            idSGameUtils::TeamCommand(TEAM_ALIENS, va("cp \"%s\"", ent->message));
        }

        return;
    }

    trap_SendServerCommand(-1, va("cp \"%s\"", ent->message));
}

void idSGameTarget::SP_target_print(gentity_t *ent) {
    ent->use = Use_Target_Print;
}


//==========================================================


/*QUAKED target_speaker (1 0 0) (-8 -8 -8) (8 8 8) looped-on looped-off global activator
"noise"   wav file to play

A global sound will play full volume throughout the level.
Activator sounds will play on the player that activated the target.
Global and activator sounds can't be combined with looping.
Normal sounds play each time the target is used.
Looped sounds will be toggled by use functions.
Multiple identical looping sounds will just increase volume without any speed cost.
"wait" : Seconds between auto triggerings, 0 = don't auto trigger
"random"  wait variance, default is 0
*/
void idSGameTarget::Use_Target_Speaker(gentity_t *ent, gentity_t *other,
                                       gentity_t *activator) {
    if(ent->spawnflags & 3) {
        // looping sound toggles
        if(ent->s.loopSound) {
            ent->s.loopSound = 0; // turn it off
        } else {
            ent->s.loopSound = ent->noise_index;  // start it
        }
    } else {
        // normal sound
        if(ent->spawnflags & 8) {
            idSGameUtils::AddEvent(activator, EV_GENERAL_SOUND, ent->noise_index);
        } else if(ent->spawnflags & 4) {
            idSGameUtils::AddEvent(ent, EV_GLOBAL_SOUND, ent->noise_index);
        } else {
            idSGameUtils::AddEvent(ent, EV_GENERAL_SOUND, ent->noise_index);
        }
    }
}

void idSGameTarget::SP_target_speaker(gentity_t *ent) {
    valueType buffer[ MAX_QPATH ];
    valueType *s;

    idSGameSpawn::SpawnFloat("wait", "0", &ent->wait);
    idSGameSpawn::SpawnFloat("random", "0", &ent->random);

    if(!idSGameSpawn::SpawnString("noise", "NOSOUND", &s)) {
        idSGameMain::Error("target_speaker without a noise key at %s",
                           idSGameUtils::vtos(ent->s.origin));
    }

    // force all client reletive sounds to be "activator" speakers that
    // play on the entity that activates it
    if(s[0] == '*') {
        ent->spawnflags |= 8;
    }

    if(!strstr(s, ".wav")) {
        Q_vsprintf_s(buffer, sizeof(buffer), sizeof(buffer), "%s.wav", s);
    } else {
        Q_strncpyz(buffer, s, sizeof(buffer));
    }

    ent->noise_index = idSGameUtils::SoundIndex(buffer);

    // a repeating speaker can be done completely client side
    ent->s.eType = ET_SPEAKER;
    ent->s.eventParm = ent->noise_index;
    ent->s.frame = ent->wait * 10;
    ent->s.clientNum = ent->random * 10;


    // check for prestarted looping sound
    if(ent->spawnflags & 1) {
        ent->s.loopSound = ent->noise_index;
    }

    ent->use = Use_Target_Speaker;

    if(ent->spawnflags & 4) {
        ent->r.svFlags |= SVF_BROADCAST;
    }

    VectorCopy(ent->s.origin, ent->s.pos.trBase);

    // must link the entity so we get areas and clusters so
    // the server can determine who to send updates to
    trap_LinkEntity(ent);
}

void idSGameTarget::target_teleporter_use(gentity_t *self,
        gentity_t *other, gentity_t *activator) {
    gentity_t *dest;

    if(!activator->client) {
        return;
    }

    dest = idSGameUtils::PickTarget(self->target);

    if(!dest) {
        idSGameMain::Printf("Couldn't find teleporter destination\n");
        return;
    }

    idSGameMisc::TeleportPlayer(activator, dest->s.origin, dest->s.angles);
}

/*QUAKED target_teleporter (1 0 0) (-8 -8 -8) (8 8 8)
The activator will be teleported away.
*/
void idSGameTarget::SP_target_teleporter(gentity_t *self) {
    if(!self->targetname) {
        idSGameMain::Printf("untargeted %s at %s\n", self->classname,
                            idSGameUtils::vtos(self->s.origin));
    }

    self->use = target_teleporter_use;
}

/*QUAKED target_relay (.5 .5 .5) (-8 -8 -8) (8 8 8) RED_ONLY BLUE_ONLY RANDOM
This doesn't perform any actions except fire its targets.
The activator can be forced to be from a certain team.
if RANDOM is checked, only one of the targets will be fired, not all of them
*/
void idSGameTarget::target_relay_use(gentity_t *self, gentity_t *other,
                                     gentity_t *activator) {
    if((self->spawnflags & 1) && activator->client &&
            activator->client->ps.stats[STAT_TEAM] != TEAM_HUMANS) {
        return;
    }

    if((self->spawnflags & 2) && activator->client &&
            activator->client->ps.stats[STAT_TEAM] != TEAM_ALIENS) {
        return;
    }

    if(self->spawnflags & 4) {
        gentity_t *ent;

        ent = idSGameUtils::PickTarget(self->target);

        if(ent && ent->use) {
            ent->use(ent, self, activator);
        }

        return;
    }

    idSGameUtils::UseTargets(self, activator);
}

void idSGameTarget::SP_target_relay(gentity_t *self) {
    self->use = target_relay_use;
}

/*QUAKED target_kill (.5 .5 .5) (-8 -8 -8) (8 8 8)
Kills the activator.
*/
void idSGameTarget::target_kill_use(gentity_t *self, gentity_t *other,
                                    gentity_t *activator) {
    idSGameCombat::Damage(activator, nullptr, nullptr, nullptr, nullptr,
                          100000, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);
}

void idSGameTarget::SP_target_kill(gentity_t *self) {
    self->use = target_kill_use;
}

/*QUAKED target_position (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for in-game calculation, like jumppad targets.
*/
void idSGameTarget::SP_target_position(gentity_t *self) {
    idSGameUtils::SetOrigin(self, self->s.origin);
}

void idSGameTarget::target_location_linkup(gentity_t *ent) {
    sint i, n;

    if(level.locationLinked) {
        return;
    }

    level.locationLinked = true;

    level.locationHead = nullptr;

    trap_SetConfigstring(CS_LOCATIONS, "unknown");

    for(i = 0, ent = g_entities, n = 1; i < level.num_entities; i++, ent++) {
        if(ent->s.eType == ET_LOCATION) {
            // lets overload some variables!
            ent->s.generic1 = n; // use for location marking

            trap_SetConfigstring(CS_LOCATIONS + n, ent->message);

            n++;

            ent->nextTrain = level.locationHead;
            level.locationHead = ent;
        }
    }

    // All linked together now
}

/*QUAKED target_location (0 0.5 0) (-8 -8 -8) (8 8 8)
Set "message" to the name of this location.
Set "count" to 0-7 for color.
0:white 1:red 2:green 3:yellow 4:blue 5:cyan 6:magenta 7:white

Closest target_location in sight used for the location, if none
in site, closest in distance
*/
void idSGameTarget::SP_target_location(gentity_t *self) {
    self->think = target_location_linkup;
    self->nextthink = level.time + 200;  // Let them all spawn first
    self->s.eType = ET_LOCATION;
    self->r.svFlags = SVF_BROADCAST;
    trap_LinkEntity(self);   // make the server send them to the clients
    idSGameUtils::SetOrigin(self, self->s.origin);
}

/*
===============
target_rumble_think
===============
*/
void idSGameTarget::target_rumble_think(gentity_t *self) {
    sint i;
    gentity_t *ent;

    if(self->last_move_time < level.time) {
        self->last_move_time = level.time + 0.5;
    }

    for(i = 0, ent = g_entities + i; i < level.num_entities; i++, ent++) {
        if(!ent->inuse) {
            continue;
        }

        if(!ent->client) {
            continue;
        }

        if(ent->client->ps.groundEntityNum == ENTITYNUM_NONE) {
            continue;
        }

        ent->client->ps.groundEntityNum = ENTITYNUM_NONE;
        ent->client->ps.velocity[ 0 ] += crandom() * 150;
        ent->client->ps.velocity[ 1 ] += crandom() * 150;
        ent->client->ps.velocity[ 2 ] = self->speed;
    }

    if(level.time < self->timestamp) {
        self->nextthink = level.time + FRAMETIME;
    }
}

/*
===============
idSGameTarget::target_rumble_use
===============
*/
void idSGameTarget::target_rumble_use(gentity_t *self, gentity_t *other,
                                      gentity_t *activator) {
    self->timestamp = level.time + (self->count * FRAMETIME);
    self->nextthink = level.time + FRAMETIME;
    self->activator = activator;
    self->last_move_time = 0;
}

/*
===============
SP_target_rumble
===============
*/
void idSGameTarget::SP_target_rumble(gentity_t *self) {
    if(!self->targetname) {
        idSGameMain::Printf(S_COLOR_YELLOW "WARNING: untargeted %s at %s\n",
                            self->classname, idSGameUtils::vtos(self->s.origin));
    }

    if(!self->count) {
        self->count = 10;
    }

    if(!self->speed) {
        self->speed = 100;
    }

    self->think = target_rumble_think;
    self->use = target_rumble_use;
}

/*
===============
target_alien_win_use
===============
*/
void idSGameTarget::target_alien_win_use(gentity_t *self, gentity_t *other,
        gentity_t *activator) {
    level.uncondAlienWin = true;
}

/*
===============
SP_target_alien_win
===============
*/
void idSGameTarget::SP_target_alien_win(gentity_t *self) {
    self->use = target_alien_win_use;
}

/*
===============
target_human_win_use
===============
*/
void idSGameTarget::target_human_win_use(gentity_t *self, gentity_t *other,
        gentity_t *activator) {
    level.uncondHumanWin = true;
}

/*
===============
SP_target_human_win
===============
*/
void idSGameTarget::SP_target_human_win(gentity_t *self) {
    self->use = target_human_win_use;
}

/*
===============
target_hurt_use
===============
*/
void idSGameTarget::target_hurt_use(gentity_t *self, gentity_t *other,
                                    gentity_t *activator) {
    // hurt the activator
    if(!activator->takedamage) {
        return;
    }

    idSGameCombat::Damage(activator, self, self, nullptr, nullptr,
                          self->damage, 0, MOD_TRIGGER_HURT);
}

/*
===============
SP_target_hurt
===============
*/
void idSGameTarget::SP_target_hurt(gentity_t *self) {
    if(!self->targetname) {
        idSGameMain::Printf(S_COLOR_YELLOW "WARNING: untargeted %s at %s\n",
                            self->classname, idSGameUtils::vtos(self->s.origin));
    }

    if(!self->damage) {
        self->damage = 5;
    }

    self->use = target_hurt_use;
}
