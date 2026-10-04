/* Item Asylum inspired modes, compatible with standard DDNet clients. */
#ifndef GAME_SERVER_GAMEMODES_HUNTERN_H
#define GAME_SERVER_GAMEMODES_HUNTERN_H

#include <game/server/gamecontroller.h>
#include <game/server/asylum_time.h>

class CGameControllerHunterN : public IGameController
{
public:
	enum EMode { MODE_FFA, MODE_TDM, MODE_GG, MODE_ELIM, MODE_ZS, MODE_JGN };
	CGameControllerHunterN(int Mode = MODE_FFA);
	// Queued: a reroll must never delete the weapon inside its own Fire callback.
	void RerollLoadout(class CCharacter *pChr);
	bool CanCombatInteract(int From, int To) const;
	bool CanWeaponInteract(int From, int To, int WeaponID) const;
	// Replaces the status broadcast of one player for a moment.
	void ShowScreenText(int CID, const char *pText, float Seconds);
	void ShowJumpscare(int CID, float Seconds);
	bool ActivateTheWorld(int CID);
	int TheWorldCooldown(int CID) const;
	int MapAnimationStartTick(int SnappingClient, int DefaultStartTick) const override;
	void OnGameStart(bool IsRound) override;
	void OnWorldReset() override;
	void OnCharacterSpawn(class CCharacter *pChr) override;
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	int OnCharacterTakeDamage(class CCharacter *pChr, vec2 &Force, int &Dmg, int From, int WeaponType, int WeaponID, bool IsExplosion) override;
	int CombatDamageUnit() const override { return 10; }
	void OnCharacterDamageApplied(class CCharacter *pChr, int From, int WeaponID, int HealthLoss, int ArmorLoss) override;
	void ActivateAsylumSkill(int CID, bool Ultimate) override;
	void OnPlayerJoin(class CPlayer *pPlayer) override;
	void OnPlayerLeave(class CPlayer *pPlayer) override;
	void OnPlayerChangeTeam(class CPlayer *pPlayer, int FromTeam, int ToTeam) override;
	bool OnPlayerTryRespawn(class CPlayer *pPlayer, vec2 Pos) override;
	void OnPostTick() override;
	void DoWincheckMatch() override;
	bool OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number = 0) override;

private:
	int m_Mode;
	int m_RespawnDelay;
	int m_SpawnProtection;
	int m_SpawnArmor;
	int m_StartingLives;
	int m_RoundSeconds;
	int m_GodChance;
	int m_GodOnly;
	bool m_RoundActive;
	int m_Juggernaut;
	int m_InfectionBonusSeconds;
	bool m_SafeZoneActive;
	vec2 m_SafeZoneCenter;
	float m_SafeZoneRadius;
	int m_NextZoneDamageTick;
	int m_aLoadouts[MAX_CLIENTS][3];
	int m_aLives[MAX_CLIENTS];
	int m_aKills[MAX_CLIENTS];
	int m_aStages[MAX_CLIENTS];
	bool m_aParticipants[MAX_CLIENTS];
	bool m_aInfected[MAX_CLIENTS];
	int m_aPendingReroll[MAX_CLIENTS];
	int m_aLastRerollTick[MAX_CLIENTS];
	int m_aLastDamageWeapon[MAX_CLIENTS];
	int m_aLastDamageFrom[MAX_CLIENTS];
	int m_aLastDamageTick[MAX_CLIENTS];
	int m_aScreenTextUntil[MAX_CLIENTS];
	int m_aJumpscareStart[MAX_CLIENTS];
	int m_aJumpscareUntil[MAX_CLIENTS];
	bool LimitedLives() const { return m_Mode == MODE_ELIM || m_Mode == MODE_JGN; }
	int RemainingSeconds() const;
	int ProgressItem(int CID) const;
	void GiveLoadout(class CCharacter *pChr, bool Randomize);
	void SendLoadout(int CID, bool Detailed);
	void FinishPlayer(int CID, const char *pReason);
	void FinishSide(bool InfectedWon, const char *pReason);
	void Forfeit(int CID);
	void ResetPlayerCooldowns(class CPlayer *pPlayer);
	static void ConStatus(IConsole::IResult *pResult, void *pUserData);
	static void ConItems(IConsole::IResult *pResult, void *pUserData);
	static void ConLoadout(IConsole::IResult *pResult, void *pUserData);
	static void ConNoCooldown(IConsole::IResult *pResult, void *pUserData);
	static void ConResetCooldown(IConsole::IResult *pResult, void *pUserData);
	static void ConTestGod(IConsole::IResult *pResult, void *pUserData);
	static void ConTestWeapon(IConsole::IResult *pResult, void *pUserData);
};

template<int Mode>
class CGameControllerAsylumMode : public CGameControllerHunterN
{
public:
	CGameControllerAsylumMode() : CGameControllerHunterN(Mode) {}
};

using CGameControllerAsylumFFA = CGameControllerAsylumMode<CGameControllerHunterN::MODE_FFA>;
using CGameControllerAsylumTDM = CGameControllerAsylumMode<CGameControllerHunterN::MODE_TDM>;
using CGameControllerAsylumGG = CGameControllerAsylumMode<CGameControllerHunterN::MODE_GG>;
using CGameControllerAsylumELIM = CGameControllerAsylumMode<CGameControllerHunterN::MODE_ELIM>;
using CGameControllerAsylumZS = CGameControllerAsylumMode<CGameControllerHunterN::MODE_ZS>;
using CGameControllerAsylumJGN = CGameControllerAsylumMode<CGameControllerHunterN::MODE_JGN>;

#endif
