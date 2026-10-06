#ifndef GAME_SERVER_WEAPONS_ASYLUM_H
#define GAME_SERVER_WEAPONS_ASYLUM_H

#include <game/server/weapon.h>

enum EAsylumItem
{
	ASYLUM_PAN, ASYLUM_KNIFE, ASYLUM_BAT,
	ASYLUM_REVOLVER, ASYLUM_SMG, ASYLUM_SHOTGUN, ASYLUM_RAILGUN, ASYLUM_LAUNCHER,
	ASYLUM_MEDKIT, ASYLUM_DASH, ASYLUM_SHOCKWAVE, ASYLUM_CLUSTER,
	// Item Asylum inspired additions. Keep these after the original IDs so old
	// loadouts and demos remain stable.
	ASYLUM_SPOON, ASYLUM_DARKHEART, ASYLUM_ENERGYSWORD, ASYLUM_DYINGPAN,
	ASYLUM_CROSSBOW, ASYLUM_FREEZERAY, ASYLUM_BAZOOKA, ASYLUM_AMERICA,
	ASYLUM_MANTLE, ASYLUM_REROLL, ASYLUM_PARASOL, ASYLUM_CLEAVE,
	ASYLUM_LILYNETTE, ASYLUM_CHICAGO, ASYLUM_M1911, ASYLUM_BOW,
	ASYLUM_PIXELGUN, ASYLUM_VAMPIREKNIVES, ASYLUM_TASER, ASYLUM_HYPERLASER,
	// Kill-earned upgrade modules and their resulting weapons. Modules are
	// deliberately outside the three random pools and live in slot 4.
	ASYLUM_UPGRADE_M1911, ASYLUM_UPGRADE_PIXELGUN,
	ASYLUM_M1911_UPG, ASYLUM_PIXELGUN_UPG,
	ASYLUM_9MM, ASYLUM_SUPPRESSED_PISTOL, ASYLUM_SSG08, ASYLUM_BLASTER,
	ASYLUM_UPGRADE_9MM, ASYLUM_UPGRADE_SUPPRESSED, ASYLUM_UPGRADE_SSG08,
	ASYLUM_MICROSMG, ASYLUM_SUPPRESSED_MAC10, ASYLUM_AWP,
	// ★ God-tier items: rare per-slot rolls (asylum_god_chance), loud on purpose.
	ASYLUM_BANHAMMER, ASYLUM_BIRCHTREE, ASYLUM_ZENITH,
	ASYLUM_TSARBOMB, ASYLUM_BLACKHOLE, ASYLUM_JUDGE,
	ASYLUM_TRAIN, ASYLUM_JUMPSCARE, ASYLUM_MOYAI,
	ASYLUM_MICROPHONE,
	ASYLUM_MASTERSPARK,
	ASYLUM_THEWORLD,
	NUM_ASYLUM_ITEMS
};

enum EAsylumCategory
{
	ASYLUM_CATEGORY_MELEE,
	ASYLUM_CATEGORY_RANGED,
	ASYLUM_CATEGORY_UTILITY,
	ASYLUM_CATEGORY_UPGRADE,
};

struct SAsylumItem
{
	const char *m_pName;
	const char *m_pDescription;
	int m_Type;
	int m_Damage;
	int m_DelayMs;
	float m_Force;
	int m_Category;
	// Left out (false) in ordinary rows. No "= false": GCC builds as C++11,
	// where a default member initializer stops the table being an aggregate.
	bool m_God;
};

const SAsylumItem &AsylumItem(int Item);
// Picks from the god-tier pool when God is set, otherwise from the ordinary one.
int AsylumRandomItem(int Category, bool God = false);
int AsylumWeaponID(int Item);
bool AsylumIsWeapon(int WeaponID);
int AsylumUpgradeKills(int Item);
int AsylumUpgradeBase(int Module);
int AsylumUpgradeModule(int Base);
int AsylumUpgradeResult(int Base);
int AsylumRangedDamage(int Item, float Range, bool Head = false, bool Limb = false);

class CAsylumWeapon : public CWeapon
{
	int m_Item;
	int m_MantleCharges;
	int m_Charge;
	int m_ENextTick;
	int m_RNextTick;
	int m_CounterUntil;
	int m_CounterPhaseEnd = 0;
	vec2 m_CounterDirection = vec2(1, 0);
	int m_CeroFacing = 1;
	bool m_CeroAimLocked = false;
	int m_BlasterChargeEnd = 0;
	bool m_BlasterReady = false;
	int m_BurstRemaining = 0;
	int m_BurstNextTick = 0;
	vec2 m_BurstDirection = vec2(1, 0);
	void FireBlaster(vec2 Direction, bool Charged);
	static bool BlasterHit(class CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife);
	static bool BlasterChargedHit(class CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife);
	int m_UltimateStartTick;
	int m_UltimateShots;
	int m_SlamTick;
	int m_LastQuoteTick;
	int m_Note;
	int m_aMicrophoneIDs[6];
	int m_MagazineSize;
	int m_MagazineReloadMs;
	int m_MagazineReloadEnd;
	bool m_ShellReload;
	int m_DarkheartSpinEnd;
	int m_DarkheartNextHit;
	int m_DashEnd;
	vec2 m_DashDirection;
	vec2 m_DashLastPos;
	bool m_aDashHit[MAX_CLIENTS];
	bool m_DashNpcHit = false;
	void EndDash();
	void TickDash();
	int m_SpeedBoostEnd;
	int m_SpeedBoostType;
	void BeginMagazineReload();
	void DarkheartAttack(vec2 Direction, int Damage, float Radius, float Forward);
	void Fire(vec2 Direction) override;
	void FireUltimateArea();
	void EndCounterPhase();
	static bool BulletHit(class CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife);
	static bool GrenadeHit(class CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife);
	static bool LaserHit(class CLaser *pLaser, vec2 Pos, CCharacter *pHit, bool EndOfLife);

	// ★ God-tier items, implemented in asylum_god.cpp.
	bool FireGodItem(vec2 Direction);
	void TickGodItem();
	static bool TsarHit(class CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife);
	static bool JudgeHit(class CProjectile *pProj, vec2 Pos, CCharacter *pHit, bool EndOfLife);
	static bool BlackholeLaserHit(class CLaser *pLaser, vec2 Pos, CCharacter *pHit, bool EndOfLife);
	static bool LightningHit(class CLaser *pLaser, vec2 Pos, CCharacter *pHit, bool EndOfLife);

public:
	// Shared by the player weapon and the Boss's stateless AI attack adapter.
	static constexpr int DARKHEART_DASH_DAMAGE = 45;
	static constexpr int DARKHEART_SPIN_DAMAGE = 8;
	static constexpr int DARKHEART_SPIN_HITS = 11;
	static constexpr float DARKHEART_DASH_SECONDS = 0.3f;
	static constexpr float DARKHEART_SPIN_SECONDS = 3.0f;
	static constexpr float DARKHEART_SPIN_RADIUS = 100.0f;
	CAsylumWeapon(CCharacter *pOwner, int Item);
	~CAsylumWeapon() override;
	void Snap(int SnappingClient, int OtherMode) override;
	bool IgnoreCooldown() override;
	void ResetCooldowns();
	int GetType() override { return AsylumItem(m_Item).m_Type; }
	int NumAmmoIcons() override;
	int Item() const { return m_Item; }
	int CooldownTicks() const { return m_ReloadTimer; }
	bool ConsumeMantle() { if(m_MantleCharges <= 0) return false; --m_MantleCharges; return true; }
	int MantleCharges() const { return m_MantleCharges; }
	void Tick() override;
	void TickPaused() override;
	void OnGiven(bool IsAmmoFillUp) override;
	void AddCharge(int ActualDamage);
	void AmmoStatus(char *pBuf, int Size);
	bool IsUpgradeModule() const { return AsylumItem(m_Item).m_Category == ASYLUM_CATEGORY_UPGRADE; }
	bool ActivateSkill(bool Ultimate, vec2 Direction);
	bool HandleIncomingDamage(vec2 &Force, int &Damage, int From);
	void SkillStatus(char *pBuf, int Size);
	float WalkspeedBonusTiles() override;
	float FixedWalkspeedTiles() override { return m_Item == ASYLUM_LILYNETTE && m_UltimateStartTick >= 0 ? 3.0f : -1.0f; }
	void OnUnequip() override;
	bool IgnoreHookDrag() override { return m_DashEnd > 0 || m_CounterPhaseEnd > 0 || BlocksHook(); }
	bool BlocksHook() const override { return m_Item == ASYLUM_LILYNETTE && m_UltimateStartTick >= 0; }
	bool OverrideAim(vec2 &Direction) const override
	{
		if(!BlocksHook() || !m_CeroAimLocked) return false;
		Direction = vec2(m_CeroFacing, 0); return true;
	}
	bool CeroState(int &Start, int &Facing) const
	{
		Start = m_UltimateStartTick; Facing = m_CeroFacing; return BlocksHook();
	}
	void AdminStatus(char *pBuf, int Size) const;
	float DefensePercent() const override { return m_Item == ASYLUM_LILYNETTE && m_UltimateStartTick >= 0 ? 40.0f : 0.0f; }
	static void ApplyRagdoll(CCharacter *pTarget, float Seconds);
	bool CanUseWhileFrozen() const override { return m_Item == ASYLUM_MEDKIT || m_Item == ASYLUM_REROLL; }
	bool RagdollImmune() const { return m_Item == ASYLUM_LILYNETTE && m_UltimateStartTick >= 0; }
	// IA uses Roblox studs.  The server maps 1.875 studs to one DDNet tile
	// (32 world units), keeping hitboxes and their laser silhouettes aligned.
	static float IAStudToDDNet(float Studs) { return Studs * (32.0f / 1.875f); }
};

template<int ItemIndex>
class CAsylumItem : public CAsylumWeapon
{
public:
	CAsylumItem(CCharacter *pOwner) : CAsylumWeapon(pOwner, ItemIndex) {}
};

using CAsylumPan = CAsylumItem<ASYLUM_PAN>;
using CAsylumKnife = CAsylumItem<ASYLUM_KNIFE>;
using CAsylumBat = CAsylumItem<ASYLUM_BAT>;
using CAsylumRevolver = CAsylumItem<ASYLUM_REVOLVER>;
using CAsylumSMG = CAsylumItem<ASYLUM_SMG>;
using CAsylumShotgun = CAsylumItem<ASYLUM_SHOTGUN>;
using CAsylumRailgun = CAsylumItem<ASYLUM_RAILGUN>;
using CAsylumLauncher = CAsylumItem<ASYLUM_LAUNCHER>;
using CAsylumMedkit = CAsylumItem<ASYLUM_MEDKIT>;
using CAsylumDash = CAsylumItem<ASYLUM_DASH>;
using CAsylumShockwave = CAsylumItem<ASYLUM_SHOCKWAVE>;
using CAsylumCluster = CAsylumItem<ASYLUM_CLUSTER>;
using CAsylumSpoon = CAsylumItem<ASYLUM_SPOON>;
using CAsylumDarkheart = CAsylumItem<ASYLUM_DARKHEART>;
using CAsylumEnergySword = CAsylumItem<ASYLUM_ENERGYSWORD>;
using CAsylumDyingPan = CAsylumItem<ASYLUM_DYINGPAN>;
using CAsylumCrossbow = CAsylumItem<ASYLUM_CROSSBOW>;
using CAsylumFreezeRay = CAsylumItem<ASYLUM_FREEZERAY>;
using CAsylumBazooka = CAsylumItem<ASYLUM_BAZOOKA>;
using CAsylumAmerica = CAsylumItem<ASYLUM_AMERICA>;
using CAsylumMantle = CAsylumItem<ASYLUM_MANTLE>;
using CAsylumReroll = CAsylumItem<ASYLUM_REROLL>;
using CAsylumParasol = CAsylumItem<ASYLUM_PARASOL>;
using CAsylumCleave = CAsylumItem<ASYLUM_CLEAVE>;
using CAsylumLilynette = CAsylumItem<ASYLUM_LILYNETTE>;
using CAsylumChicago = CAsylumItem<ASYLUM_CHICAGO>;
using CAsylumM1911 = CAsylumItem<ASYLUM_M1911>;
using CAsylumBow = CAsylumItem<ASYLUM_BOW>;
using CAsylumPixelGun = CAsylumItem<ASYLUM_PIXELGUN>;
using CAsylumVampireKnives = CAsylumItem<ASYLUM_VAMPIREKNIVES>;
using CAsylumTaser = CAsylumItem<ASYLUM_TASER>;
using CAsylumHyperlaser = CAsylumItem<ASYLUM_HYPERLASER>;
using CAsylumUpgradeM1911 = CAsylumItem<ASYLUM_UPGRADE_M1911>;
using CAsylumUpgradePixelGun = CAsylumItem<ASYLUM_UPGRADE_PIXELGUN>;
using CAsylumM1911Upgrade = CAsylumItem<ASYLUM_M1911_UPG>;
using CAsylumPixelGunUpgrade = CAsylumItem<ASYLUM_PIXELGUN_UPG>;
using CAsylum9mm = CAsylumItem<ASYLUM_9MM>;
using CAsylumSuppressedPistol = CAsylumItem<ASYLUM_SUPPRESSED_PISTOL>;
using CAsylumSSG08 = CAsylumItem<ASYLUM_SSG08>;
using CAsylumBlaster = CAsylumItem<ASYLUM_BLASTER>;
using CAsylumUpgrade9mm = CAsylumItem<ASYLUM_UPGRADE_9MM>;
using CAsylumUpgradeSuppressed = CAsylumItem<ASYLUM_UPGRADE_SUPPRESSED>;
using CAsylumUpgradeSSG08 = CAsylumItem<ASYLUM_UPGRADE_SSG08>;
using CAsylumMicroSMG = CAsylumItem<ASYLUM_MICROSMG>;
using CAsylumSuppressedMAC10 = CAsylumItem<ASYLUM_SUPPRESSED_MAC10>;
using CAsylumAWP = CAsylumItem<ASYLUM_AWP>;
using CAsylumBanhammer = CAsylumItem<ASYLUM_BANHAMMER>;
using CAsylumBirchTree = CAsylumItem<ASYLUM_BIRCHTREE>;
using CAsylumZenith = CAsylumItem<ASYLUM_ZENITH>;
using CAsylumTsarBomb = CAsylumItem<ASYLUM_TSARBOMB>;
using CAsylumBlackhole = CAsylumItem<ASYLUM_BLACKHOLE>;
using CAsylumJudge = CAsylumItem<ASYLUM_JUDGE>;
using CAsylumTrain = CAsylumItem<ASYLUM_TRAIN>;
using CAsylumJumpscare = CAsylumItem<ASYLUM_JUMPSCARE>;
using CAsylumMoyai = CAsylumItem<ASYLUM_MOYAI>;
using CAsylumMicrophone = CAsylumItem<ASYLUM_MICROPHONE>;
using CAsylumMasterSpark = CAsylumItem<ASYLUM_MASTERSPARK>;
using CAsylumTheWorld = CAsylumItem<ASYLUM_THEWORLD>;

#endif
