/* Item Asylum inspired modes, compatible with standard DDNet clients. */
#ifndef GAME_SERVER_GAMEMODES_HUNTERN_H
#define GAME_SERVER_GAMEMODES_HUNTERN_H

#include <game/server/gamecontroller.h>
#include <game/server/asylum_time.h>
#include <game/server/gojo_state.h>

class CGameControllerHunterN : public IGameController
{
public:
	enum EMode { MODE_FFA, MODE_TDM, MODE_GG, MODE_ELIM, MODE_ZS, MODE_JGN, MODE_TOUR };
	CGameControllerHunterN(int Mode = MODE_FFA);
	~CGameControllerHunterN() override;
	void OnSnap(int SnappingClient) override;
	int MapMusicOffsetSeconds() const override { return m_TourBoss && m_BossPhase == 2 ? 3600 : 0; }
	bool IntersectCombatNpc(vec2 From, vec2 To, float Radius, vec2 *pHit) override;
	int DamageCombatNpc(int From, int WeaponID, int Damage) override;
	int DamageCombatNpcBox(int From, int WeaponID, int Damage, vec2 Origin, vec2 Aim, float Reach, float HalfWidth) override;
	void ExplosionCombatNpc(vec2 Pos, int From, int WeaponID, int Damage) override;
	int CombatNpcWeaponDamage(int From, int WeaponID) override;
	// Queued: a reroll must never delete the weapon inside its own Fire callback.
	void RerollLoadout(class CCharacter *pChr);
	// Utility-slot item consumption is queued because a weapon must not delete
	// itself while its Fire callback is still unwinding.
	void ConsumeSingleUse(class CCharacter *pChr, int Slot);
	// Apply a kill-unlocked upgrade module from slot 4 to the ranged slot.
	void ConsumeUpgradeModule(class CCharacter *pChr, int Slot);
	bool ConsumeMantleShield(int CID);
	bool HasMantleShield(int CID) const { return CID >= 0 && CID < MAX_CLIENTS && m_aMantleShield[CID]; }
	bool IsMantleInvulnerable(int CID) const;
	bool CanCombatInteract(int From, int To) const;
	bool CanWeaponInteract(int From, int To, int WeaponID) const override;
	// Replaces the status broadcast of one player for a moment.
	void ShowScreenText(int CID, const char *pText, float Seconds);
	void ShowJumpscare(int CID, float Seconds);
	bool ActivateTheWorld(int CID);
	int TheWorldCooldown(int CID) const;
	CGojoState *GetGojoState(int CID) override;
	const CGojoState *GetGojoState(int CID) const override;
	int MapAnimationStartTick(int SnappingClient, int DefaultStartTick) const override;
	bool IsRagdollImmune(int CID) const { return m_Mode == MODE_JGN && CID == m_Juggernaut; }
	void OnGameStart(bool IsRound) override;
	void OnWorldReset() override;
	void OnCharacterSpawn(class CCharacter *pChr) override;
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	int OnCharacterTakeDamage(class CCharacter *pChr, vec2 &Force, int &Dmg, int From, int WeaponType, int WeaponID, bool IsExplosion) override;
	int CombatDamageUnit() const override { return 10; }
	float BaseWalkspeedTiles() const override { return 16.0f; }
	void OnCharacterDamageApplied(class CCharacter *pChr, int From, int WeaponID, int HealthLoss, int ArmorLoss) override;
	void ActivateAsylumSkill(int CID, bool Ultimate) override;
	void OnPlayerJoin(class CPlayer *pPlayer) override;
	void OnPlayerLeave(class CPlayer *pPlayer) override;
	void OnPlayerChangeTeam(class CPlayer *pPlayer, int FromTeam, int ToTeam) override;
	bool OnPlayerTryRespawn(class CPlayer *pPlayer, vec2 Pos) override;
	void OnPostTick() override;
	bool IsAdminInvincible(int CID) const override { return CID >= 0 && CID < MAX_CLIENTS && m_aAdminGod[CID]; }
	void DoWincheckMatch() override;
	bool OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number = 0) override;

private:
	static constexpr float BOSS_TRIGGER_X = 3200.0f;
	static constexpr float BOSS_ARENA_END = 19200.0f;
	int m_aMapChoices[3] = {0, 1, 2};
	const char *TourMapChoice(int Choice) const;
	bool m_TourLobby = false;
	bool m_TourBoss = false;
	bool m_TourChangingMap = false;
	int m_LobbySeconds = 45;
	int m_LobbyStartTick = -1;
	int m_aMapVotes[MAX_CLIENTS];
	int m_BossHealth = 0;
	int m_BossMaxHealth = 0;
	int m_BossPhase = 1;
	int m_BossDrinkUntil = 0;
	int m_BossNextTeleport = 0;
	int m_BossNextAttack = 0;
	int m_BossAttack = -1;
	int m_BossPreviousAttack = -1;
	int m_BossAttackRepeat = 0;
	int m_BossAttackUses = 0;
	int m_BossDashUntil = 0;
	int m_BossStunUntil = 0;
	int m_BossShadowUntil = 0;
	int m_BossGrabCID = -1;
	int m_BossGrabUntil = 0;
	vec2 m_BossGrabStart = vec2(0, 0);
	int m_BossLastFireTick = 0;
	int m_aBossSnapCID[MAX_CLIENTS];
	bool m_BossWindingUp = false;
	vec2 m_BossAttackAim = vec2(1, 0);
	uint64 m_BossDashHits = 0;
	vec2 m_BossPos = vec2(BOSS_TRIGGER_X + 960, 1760);
	vec2 m_BossVel = vec2(0, 0);
	vec2 m_BossAim = vec2(1, 0);
	int m_aBossVisualIDs[20];
	struct SBossShot { vec2 m_Pos, m_Vel; int m_Life, m_Damage, m_ID; bool m_Pierce; uint64 m_Hits; };
	std::vector<SBossShot> m_BossShots;
	void ResetTour();
	void TickTour();
	void TickBoss();
	void ChangeTourMap(const char *pMap);
	void ClearBossShots();
	void ClearBossTee();
	void SelectBossAttack();
	bool BossHitPlayer(class CCharacter *pChr, int Damage, vec2 Force, float Freeze = 0, bool Lifesteal = false);
	void BossStatus(char *pBuf, int Size) const;
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
	int m_aLoadouts[MAX_CLIENTS][4];
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
	CGojoState m_aGojo[MAX_CLIENTS];
	void GiveGojoLoadout(class CCharacter *pChr);
	void SendGojoLoadout(int CID, bool Detailed);
	void TickGojo(int CID, bool Advance);
	bool GojoInfinityBlocks(class CCharacter *pChr, int From, int WeaponID, int Damage);
	static void ConGojo(IConsole::IResult *pResult, void *pUserData);
	static void ConGojoStatus(IConsole::IResult *pResult, void *pUserData);
	static void ConGojoEnergy(IConsole::IResult *pResult, void *pUserData);
	static void ConInfCursedEnergy(IConsole::IResult *pResult, void *pUserData);
	int m_aJumpscareStart[MAX_CLIENTS];
	int m_aJumpscareUntil[MAX_CLIENTS];
	int m_aUpgradeKills[MAX_CLIENTS][5];
	int m_aPendingConsume[MAX_CLIENTS];
	int m_aPendingUpgrade[MAX_CLIENTS];
	int m_aLastCombatTick[MAX_CLIENTS];
	int m_aRegenCarry[MAX_CLIENTS];
	bool m_aMantleShield[MAX_CLIENTS];
	int m_aMantleLives[MAX_CLIENTS];
	int m_aMantleReadyTick[MAX_CLIENTS];
	int m_aMantleCharge[MAX_CLIENTS];
	int m_aMantleInvulnerableUntil[MAX_CLIENTS];
	bool m_aAdminGod[MAX_CLIENTS] = {};
	void AwardUpgradeKill(int KillerCID, int WeaponID);
	bool LimitedLives() const { return m_Mode == MODE_ELIM || m_Mode == MODE_JGN; }
	int RemainingSeconds() const;
	int ProgressItem(int CID) const;
	void GiveLoadout(class CCharacter *pChr, bool Randomize, bool PreserveMantle = false);
	void ClearAsylumProgress(int CID, bool PreserveMantle = false);
	void ProcessPendingItems(int CID, class CCharacter *pChr);
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
	static void ConGive(IConsole::IResult *pResult, void *pUserData);
	static void ConInspect(IConsole::IResult *pResult, void *pUserData);
	bool RequireAdmin(IConsole::IResult *pResult);
	static void ConGod(IConsole::IResult *pResult, void *pUserData);
	static void ConMap(IConsole::IResult *pResult, void *pUserData);
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
using CGameControllerAsylumTour = CGameControllerAsylumMode<CGameControllerHunterN::MODE_TOUR>;

#endif
