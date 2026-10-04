/* Tee Asylum god-tier (★) item effects: meme sound sequences and summoned entities. */
#ifndef GAME_SERVER_ENTITIES_ASYLUM_FX_H
#define GAME_SERVER_ENTITIES_ASYLUM_FX_H

#include <base/system.h>
#include <base/vmath.h>

class CCharacter;
class CEntity;
class CGameWorld;

// Standard DDNet clients can only play their built-in samples, so the
// "mysterious" sounds are timed sequences of stock sounds.
enum EAsylumMeme
{
	ASYLUM_MEME_BONK,
	ASYLUM_MEME_VINE_BOOM,
	ASYLUM_MEME_FANFARE,
	ASYLUM_MEME_DRUMROLL,
	ASYLUM_MEME_SAD,
	ASYLUM_MEME_BOING,
	ASYLUM_MEME_ERROR,
	ASYLUM_MEME_SCREAM,
	ASYLUM_MEME_CHOO_CHOO,
	ASYLUM_MEME_KACHING,
	ASYLUM_MEME_SHING,
	ASYLUM_MEME_RUSTLE,
	ASYLUM_MEME_WUB,
	ASYLUM_MEME_ALARM,
	ASYLUM_MEME_HEARTBEAT,
	ASYLUM_MEME_THUNDER,
	NUM_ASYLUM_MEMES
};

// Plays a meme at Pos. Global sounds play at full volume regardless of
// distance; Mask limits either kind to some clients of this world.
void AsylumPlayMeme(CGameWorld *pWorld, int Meme, vec2 Pos, bool Global = false, int64 Mask = -1LL);

// Writes ASCII text into the world with laser dots for a few seconds.
void AsylumShowText(CGameWorld *pWorld, vec2 Pos, const char *pText, float Seconds, int Gap = 7);

// Whether something owned by Owner may hurt or push pTarget under the mode's
// rules (teams, ZS sides, JGN roles, spawn protection). pSource is any entity
// in the same world.
bool AsylumCanAffect(CEntity *pSource, int Owner, CCharacter *pTarget, int WeaponID);

// Zenith: spinning spectral blades fly to Target and return to the owner.
void AsylumSpawnBladeStorm(CGameWorld *pWorld, int Owner, int WeaponID, vec2 Pos, vec2 Target, int Damage);
// Tsar bobm: beeps for a while, then detonates in three widening rings.
void AsylumSpawnTsarBomb(CGameWorld *pWorld, int Owner, int WeaponID, vec2 Pos);
// Blackhole raygun: pulls enemies in for three seconds, then collapses.
void AsylumSpawnBlackHole(CGameWorld *pWorld, int Owner, int WeaponID, vec2 Pos);
// Unstoppable train: a ghost train crossing the map horizontally (Direction is -1 or 1).
void AsylumSpawnTrain(CGameWorld *pWorld, int Owner, int WeaponID, vec2 Pos, int Direction, int Damage, float Force);

#endif
