/* Item Asylum inspired three-slot combat and six distinct game modes. */
#include "huntern.h"

#include <game/mapitems.h>
#include <game/server/entities/asylum_fx.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/weapons.h>

namespace
{
const char *gs_apModeNames[] = {"Tee Asylum FFA", "Tee Asylum TDM", "Tee Asylum GG", "Tee Asylum ELIM", "Tee Asylum ZS", "Tee Asylum JGN"};
const char *gs_apRules[] = {
	"自由混战：每次出生重抽近战、远程、特殊道具，击杀计分。",
	"团队乱斗：红蓝两队，队友免伤，团队击杀分决定胜负。",
	"武器进阶：用槽位1击杀升级，共17阶段；最终黄金勺击杀获胜。槽位2/3为急救包和冲刺。",
	"三命淘汰：每人3条命，击杀增加20点生命上限（最高200）；超时安全区收缩，最后存活者获胜。",
	"感染生存：随机初始感染者，生还者死亡后转为感染者；撑到倒计时结束获胜。",
	"巨人讨伐：一人化为高生命巨人，其余挑战者各3条命；击败巨人或坚持到超时即可获胜。"};
const int gs_aModeSeconds[] = {480, 360, 480, 180, 240, 240};
const int gs_aProgressItems[] = {
	ASYLUM_REVOLVER, ASYLUM_SMG, ASYLUM_SHOTGUN, ASYLUM_CROSSBOW,
	ASYLUM_FREEZERAY, ASYLUM_LAUNCHER, ASYLUM_BAZOOKA, ASYLUM_AMERICA,
	ASYLUM_RAILGUN, ASYLUM_PAN, ASYLUM_BAT, ASYLUM_ENERGYSWORD,
	ASYLUM_DARKHEART, ASYLUM_DYINGPAN, ASYLUM_KNIFE, ASYLUM_SPOON,
	ASYLUM_SPOON}; // Last stage is a golden spoon with bonus damage.
const int NUM_PROGRESS_STAGES = sizeof(gs_aProgressItems) / sizeof(gs_aProgressItems[0]);
}

CGameControllerHunterN::CGameControllerHunterN(int Mode) : IGameController(),
	m_Mode(clamp(Mode, (int)MODE_FFA, (int)MODE_JGN)), m_RoundActive(false), m_Juggernaut(-1),
	m_InfectionBonusSeconds(0), m_SafeZoneActive(false), m_SafeZoneCenter(0, 0), m_SafeZoneRadius(0.0f), m_NextZoneDamageTick(0)
{
	m_pGameType = gs_apModeNames[m_Mode];
	m_GameFlags = m_Mode == MODE_TDM ? IGF_TEAMS | IGF_SUDDENDEATH : IGF_SUDDENDEATH;
	if(LimitedLives())
		m_GameFlags = IGF_MARK_SURVIVAL;
	if(m_Mode == MODE_ZS)
		m_GameFlags = IGF_MARK_TEAMS;
	m_ResetScoreOnEndMatch = true;
	for(int CID = 0; CID < MAX_CLIENTS; ++CID)
	{
		for(int &Item : m_aLoadouts[CID])
			Item = -1;
		m_aLives[CID] = m_aKills[CID] = m_aStages[CID] = 0;
		m_aParticipants[CID] = m_aInfected[CID] = false;
		m_aPendingReroll[CID] = m_aLastDamageWeapon[CID] = m_aLastDamageFrom[CID] = m_aLastDamageTick[CID] = -1;
		m_aLastRerollTick[CID] = -1000000;
		m_aScreenTextUntil[CID] = 0;
		m_aJumpscareStart[CID] = m_aJumpscareUntil[CID] = 0;
	}
	INSTANCE_CONFIG_INT(&m_RespawnDelay, "asylum_respawn_delay", 2, 1, 10, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Respawn delay in seconds");
	INSTANCE_CONFIG_INT(&m_SpawnProtection, "asylum_spawn_protection", 1, 0, 5, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Spawn shield in seconds, cancelled on firing");
	INSTANCE_CONFIG_INT(&m_SpawnArmor, "asylum_spawn_armor", 20, 0, 100, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Armor granted on each spawn");
	INSTANCE_CONFIG_INT(&m_StartingLives, "asylum_lives", 3, 1, 10, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Lives in ELIM and for JGN challengers");
	INSTANCE_CONFIG_INT(&m_RoundSeconds, "asylum_round_seconds", gs_aModeSeconds[m_Mode], 10, 3600, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Round duration in seconds");
	INSTANCE_CONFIG_INT(&m_GodChance, "asylum_god_chance", 5, 0, 100, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Chance in percent per slot to roll a god-tier (★) item");
	INSTANCE_CONFIG_INT(&m_GodOnly, "asylum_god_only", 0, 0, 1, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Roll only god-tier (★) items");
	InstanceConsole()->Register("asylum_status", "", CFGFLAG_CHAT | CFGFLAG_INSTANCE | CFGFLAG_NO_CONSENT, ConStatus, this, "Show mode, timer, lives and equipment");
	InstanceConsole()->Register("asylum_items", "", CFGFLAG_CHAT | CFGFLAG_INSTANCE | CFGFLAG_NO_CONSENT, ConItems, this, "List all random items");
	InstanceConsole()->Register("asylum_loadout", "i[cid] i[melee] i[ranged] i[utility]", CFGFLAG_INSTANCE, ConLoadout, this, "Administrator: equip valid category items on an existing character");
	InstanceConsole()->Register("asylum_test_nocd", "i[cid] i[enabled]", CFGFLAG_INSTANCE, ConNoCooldown, this, "Administrator test: toggle no cooldowns for one player (0/1)");
	InstanceConsole()->Register("asylum_test_reset_cd", "i[cid]", CFGFLAG_INSTANCE, ConResetCooldown, this, "Administrator test: reset one player's weapon and The World cooldowns once");
	InstanceConsole()->Register("asylum_test_god", "i[cid] i[enabled]", CFGFLAG_INSTANCE, ConTestGod, this, "Administrator test: toggle invulnerability for one player (0/1)");
	InstanceConsole()->Register("asylum_test_weapon", "i[cid] i[item]", CFGFLAG_INSTANCE, ConTestWeapon, this, "Administrator test: replace and select one item in its automatic category slot");
	InstanceConsole()->Register("asylum_test_loadout", "i[cid] i[melee] i[ranged] i[utility]", CFGFLAG_INSTANCE, ConLoadout, this, "Administrator test: equip three category items; asylum_loadout remains a compatible alias");
	InstanceConsole()->Register("asylum_test_items", "", CFGFLAG_INSTANCE, ConItems, this, "Administrator test: list item IDs");
}

void CGameControllerHunterN::ConStatus(IConsole::IResult *pResult, void *pUserData)
{
	CGameControllerHunterN *pSelf = (CGameControllerHunterN *)pUserData;
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s | 剩余 %ds | %s", pSelf->m_pGameType, pSelf->RemainingSeconds(), gs_apRules[pSelf->m_Mode]);
	pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", aBuf);
	if(pResult->m_ClientID >= 0 && pResult->m_ClientID < MAX_CLIENTS)
	{
		// Instance-console output is not routed back to a player who invoked an
		// information vote. Mirror it to that player while retaining the console
		// line for administrators and logs.
		pSelf->GameServer()->SendChatTarget(pResult->m_ClientID, aBuf);
		pSelf->SendLoadout(pResult->m_ClientID, true);
	}
}

void CGameControllerHunterN::ConItems(IConsole::IResult *pResult, void *pUserData)
{
	CGameControllerHunterN *pSelf = (CGameControllerHunterN *)pUserData;
	int NumGod = 0;
	for(int Item = 0; Item < NUM_ASYLUM_ITEMS; ++Item)
		NumGod += AsylumItem(Item).m_God;
	char aGod[64];
	if(pSelf->m_GodOnly)
		str_copy(aGod, "当前只抽★大神武器", sizeof(aGod));
	else
		str_format(aGod, sizeof(aGod), "每个槽位%d%%概率抽到", pSelf->m_GodChance);
	char aHeader[192];
	str_format(aHeader, sizeof(aHeader), "装备池：共%d件近战、远程与特殊道具，其中★大神武器%d件，%s。", NUM_ASYLUM_ITEMS, NumGod, aGod);
	pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", aHeader);
	if(pResult->m_ClientID >= 0 && pResult->m_ClientID < MAX_CLIENTS)
		pSelf->GameServer()->SendChatTarget(pResult->m_ClientID, aHeader);
	for(int Item = 0; Item < NUM_ASYLUM_ITEMS; ++Item)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "%02d %s — %s", Item, AsylumItem(Item).m_pName, AsylumItem(Item).m_pDescription);
		pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", aBuf);
		if(pResult->m_ClientID >= 0 && pResult->m_ClientID < MAX_CLIENTS)
			pSelf->GameServer()->SendChatTarget(pResult->m_ClientID, aBuf);
	}
}

void CGameControllerHunterN::ConLoadout(IConsole::IResult *pResult, void *pUserData)
{
	CGameControllerHunterN *pSelf = (CGameControllerHunterN *)pUserData;
	const int CID = pResult->GetInteger(0);
	CPlayer *pPlayer = CID >= 0 && CID < MAX_CLIENTS ? pSelf->GetPlayerIfInRoom(CID) : nullptr;
	if(!pPlayer || !pPlayer->GetCharacter())
	{
		pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", "Player must have an active character in this room");
		return;
	}
	if(pSelf->m_Mode == MODE_GG || (pSelf->m_Mode == MODE_ZS && pSelf->m_aInfected[CID]) || (pSelf->m_Mode == MODE_JGN && CID == pSelf->m_Juggernaut))
	{
		pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", "This mode or role has a fixed loadout");
		return;
	}
	int aItems[3];
	for(int Slot = 0; Slot < 3; ++Slot)
	{
		aItems[Slot] = pResult->GetInteger(Slot + 1);
		if(aItems[Slot] < 0 || aItems[Slot] >= NUM_ASYLUM_ITEMS || AsylumItem(aItems[Slot]).m_Category != Slot)
		{
			pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", "Items must be valid melee, ranged, utility indices in that order");
			return;
		}
	}
	for(int Slot = 0; Slot < 3; ++Slot)
		pSelf->m_aLoadouts[CID][Slot] = aItems[Slot];
	pSelf->GiveLoadout(pPlayer->GetCharacter(), false);
}

int CGameControllerHunterN::RemainingSeconds() const
{
	return maximum(0, m_RoundSeconds + m_InfectionBonusSeconds - (Server()->Tick() - m_GameStartTick) / Server()->TickSpeed());
}

void CGameControllerHunterN::ResetPlayerCooldowns(CPlayer *pPlayer)
{
	pPlayer->m_TheWorldCooldown.Reset();
	m_aLastRerollTick[pPlayer->GetCID()] = -1000000;
	CCharacter *pChr = pPlayer->GetCharacter();
	if(!pChr)
		return;
	for(int Slot = 0; Slot < NUM_WEAPON_SLOTS; ++Slot)
	{
		CWeapon *apWeapons[] = {pChr->GetWeapon(Slot), pChr->GetOverrideWeapon(Slot)};
		for(CWeapon *pWeapon : apWeapons)
			if(pWeapon && AsylumIsWeapon(pWeapon->GetWeaponID()))
				static_cast<CAsylumWeapon *>(pWeapon)->ResetCooldowns();
	}
	CWeapon *pPowerup = pChr->GetPowerupWeapon();
	if(pPowerup && AsylumIsWeapon(pPowerup->GetWeaponID()))
		static_cast<CAsylumWeapon *>(pPowerup)->ResetCooldowns();
}

void CGameControllerHunterN::ConNoCooldown(IConsole::IResult *pResult, void *pUserData)
{
	CGameControllerHunterN *pSelf = static_cast<CGameControllerHunterN *>(pUserData);
	const int CID = pResult->GetInteger(0);
	const int Enabled = pResult->GetInteger(1);
	CPlayer *pPlayer = CID >= 0 && CID < MAX_CLIENTS ? pSelf->GetPlayerIfInRoom(CID) : nullptr;
	if(!pPlayer || (Enabled != 0 && Enabled != 1))
	{
		pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", "Player must be in this room; enabled must be 0 or 1");
		return;
	}
	pPlayer->m_AsylumNoCooldown = Enabled != 0;
	if(Enabled)
		pSelf->ResetPlayerCooldowns(pPlayer);
	char aBuf[160];
	str_format(aBuf, sizeof(aBuf), "CID %d: Asylum no-cooldown testing %s", CID, Enabled ? "enabled" : "disabled");
	pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", aBuf);
	pSelf->GameServer()->SendChatTarget(CID, Enabled ? "[测试] 已开启无冷却；前摇、持续时间、R充能和时停限制仍保留。" : "[测试] 已恢复正常冷却。");
}

void CGameControllerHunterN::ConResetCooldown(IConsole::IResult *pResult, void *pUserData)
{
	CGameControllerHunterN *pSelf = static_cast<CGameControllerHunterN *>(pUserData);
	const int CID = pResult->GetInteger(0);
	CPlayer *pPlayer = CID >= 0 && CID < MAX_CLIENTS ? pSelf->GetPlayerIfInRoom(CID) : nullptr;
	if(!pPlayer)
	{
		pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", "Player must be in this room");
		return;
	}
	pSelf->ResetPlayerCooldowns(pPlayer);
	char aBuf[96];
	str_format(aBuf, sizeof(aBuf), "CID %d: Asylum cooldowns reset", CID);
	pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", aBuf);
	pSelf->GameServer()->SendChatTarget(CID, "[测试] 已清空当前武器技能和 The World 冷却一次，不改变装备和血甲。");
}

void CGameControllerHunterN::ConTestGod(IConsole::IResult *pResult, void *pUserData)
{
	CGameControllerHunterN *pSelf = static_cast<CGameControllerHunterN *>(pUserData);
	const int CID = pResult->GetInteger(0);
	const int Enabled = pResult->GetInteger(1);
	CPlayer *pPlayer = CID >= 0 && CID < MAX_CLIENTS ? pSelf->GetPlayerIfInRoom(CID) : nullptr;
	if(!pPlayer || (Enabled != 0 && Enabled != 1))
	{
		pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", "Player must be in this room; enabled must be 0 or 1");
		return;
	}
	pPlayer->m_AsylumTestGod = Enabled != 0;
	CCharacter *pChr = pPlayer->GetCharacter();
	if(Enabled && pChr)
	{
		// An immortal test target should be hittable immediately, not hidden
		// behind its initial spawn shield. Existing freezes are left untouched.
		pChr->Protect(0.0f, false);
	}
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "CID %d: Asylum god-mode testing %s", CID, Enabled ? "enabled" : "disabled");
	pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", aBuf);
	pSelf->GameServer()->SendChatTarget(CID, Enabled ?
		"[测试] 已开启锁血无敌：可被击中，血甲不减少；命中音效、击退、冻结、跳脸与时停照常。" :
		"[测试] 已关闭无敌。");
}

void CGameControllerHunterN::ConTestWeapon(IConsole::IResult *pResult, void *pUserData)
{
	CGameControllerHunterN *pSelf = static_cast<CGameControllerHunterN *>(pUserData);
	const int CID = pResult->GetInteger(0);
	const int Item = pResult->GetInteger(1);
	CPlayer *pPlayer = CID >= 0 && CID < MAX_CLIENTS ? pSelf->GetPlayerIfInRoom(CID) : nullptr;
	CCharacter *pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;
	if(!pChr || !pChr->IsAlive() || Item < 0 || Item >= NUM_ASYLUM_ITEMS)
	{
		pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", "Player must have an active character; item ID must be valid");
		return;
	}
	if(pSelf->m_Mode == MODE_GG || (pSelf->m_Mode == MODE_ZS && pSelf->m_aInfected[CID]) ||
		(pSelf->m_Mode == MODE_JGN && CID == pSelf->m_Juggernaut))
	{
		pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", "Fixed loadout in this mode/role; use FFA to test arbitrary items");
		return;
	}
	const int Slot = AsylumItem(Item).m_Category;
	pSelf->m_aLoadouts[CID][Slot] = Item;
	pSelf->m_aPendingReroll[CID] = -1;
	// Only replace this slot; other weapons' cooldowns/charge and blood/armor stay.
	pChr->SetPowerUpWeapon(WEAPON_ID_NONE);
	pChr->SetOverrideWeapon(Slot, WEAPON_ID_NONE);
	pChr->ForceSetWeapon(Slot, AsylumWeaponID(Item), -1);
	pChr->SetWeaponSlot(Slot, false);
	pSelf->SendLoadout(CID, true);
	char aBuf[224];
	str_format(aBuf, sizeof(aBuf), "CID %d: slot %d -> %d %s", CID, Slot + 1, Item, AsylumItem(Item).m_pName);
	pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", aBuf);
}

int CGameControllerHunterN::ProgressItem(int CID) const
{
	return gs_aProgressItems[clamp(m_aStages[CID], 0, NUM_PROGRESS_STAGES - 1)];
}

void CGameControllerHunterN::OnGameStart(bool IsRound)
{
	m_RoundActive = true;
	m_Juggernaut = -1;
	m_InfectionBonusSeconds = 0;
	m_SafeZoneActive = false;
	m_SafeZoneRadius = 0.0f;
	// The actual roster is initialized after ResetGame deletes the old world.
}

void CGameControllerHunterN::OnWorldReset()
{
	int aActive[MAX_CLIENTS], NumActive = 0;
	for(int CID = 0; CID < MAX_CLIENTS; ++CID)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(CID);
		m_aParticipants[CID] = pPlayer && pPlayer->GetTeam() != TEAM_SPECTATORS;
		m_aLives[CID] = m_aParticipants[CID] ? m_StartingLives : 0;
		m_aStages[CID] = m_aKills[CID] = 0;
		m_aInfected[CID] = false;
		m_aPendingReroll[CID] = m_aLastDamageWeapon[CID] = m_aLastDamageFrom[CID] = m_aLastDamageTick[CID] = -1;
		m_aLastRerollTick[CID] = -1000000;
		m_aJumpscareStart[CID] = m_aJumpscareUntil[CID] = 0;
		for(int &Item : m_aLoadouts[CID])
			Item = -1;
		if(m_aParticipants[CID])
			aActive[NumActive++] = CID;
	}
	if(m_RoundActive && NumActive > 0)
	{
		if(m_Mode == MODE_ZS)
		{
			const int NumInfected = maximum(1, NumActive / 6);
			for(int i = 0; i < NumInfected; ++i)
			{
				const int Choice = i + secure_rand_below(NumActive - i);
				const int Temp = aActive[i];
				aActive[i] = aActive[Choice];
				aActive[Choice] = Temp;
				m_aInfected[aActive[i]] = true;
			}
		}
		if(m_Mode == MODE_JGN)
		{
			m_Juggernaut = aActive[secure_rand_below(NumActive)];
			m_aLives[m_Juggernaut] = 1;
			char aBuf[160];
			str_format(aBuf, sizeof(aBuf), "[JGN] %s 成为巨人！其余挑战者每人%d条命。", Server()->ClientName(m_Juggernaut), m_StartingLives);
			SendChatTarget(-1, aBuf);
		}
		SendChatTarget(-1, gs_apRules[m_Mode]);
	}
}

void CGameControllerHunterN::Forfeit(int CID)
{
	if(!m_RoundActive || IsWarmup())
		return;
	if(LimitedLives() || m_Mode == MODE_ZS)
	{
		m_aParticipants[CID] = false;
		m_aLives[CID] = 0;
	}
	m_aPendingReroll[CID] = -1;
}

void CGameControllerHunterN::OnPlayerJoin(CPlayer *pPlayer)
{
	const int CID = pPlayer->GetCID();
	for(int &Item : m_aLoadouts[CID])
		Item = -1;
	m_aPendingReroll[CID] = -1;
	m_aScreenTextUntil[CID] = 0;
	m_aJumpscareStart[CID] = m_aJumpscareUntil[CID] = 0;
	pPlayer->SetClass(CLASS_NONE);
	GameServer()->SendChatTarget(CID, gs_apRules[m_Mode]);
	GameServer()->SendChatTarget(CID, "按1/2/3切换装备，左键使用；弹药无限，技能有独立冷却。投票菜单可查看规则和装备列表。");
	GameServer()->SendChatTarget(CID, "E/R技能兼容普通客户端：控制台输入 bind e \"say /asylum_e\" 和 bind r \"say /asylum_r\"；GG模式禁用附加技能。");
	if(m_RoundActive && !IsWarmup() && (LimitedLives() || m_Mode == MODE_ZS))
	{
		m_aParticipants[CID] = false;
		m_aLives[CID] = 0;
		pPlayer->CancelSpawn();
		GameServer()->SendChatTarget(CID, "本局参赛名单已锁定；中途加入或离开重进需等待下一局。");
	}
	else
	{
		m_aParticipants[CID] = true;
		m_aStages[CID] = m_aKills[CID] = 0;
		m_aInfected[CID] = false;
		m_aLives[CID] = m_StartingLives;
	}
}

void CGameControllerHunterN::OnPlayerLeave(CPlayer *pPlayer)
{
	GameWorld()->EndTimeStopFor(pPlayer->GetCID());
	m_aJumpscareStart[pPlayer->GetCID()] = m_aJumpscareUntil[pPlayer->GetCID()] = 0;
	Forfeit(pPlayer->GetCID());
}

void CGameControllerHunterN::OnPlayerChangeTeam(CPlayer *pPlayer, int FromTeam, int ToTeam)
{
	if(ToTeam == TEAM_SPECTATORS)
		GameWorld()->EndTimeStopFor(pPlayer->GetCID());
	if(m_RoundActive && !IsWarmup() && (LimitedLives() || m_Mode == MODE_ZS))
	{
		Forfeit(pPlayer->GetCID());
		if(ToTeam != TEAM_SPECTATORS)
			pPlayer->CancelSpawn();
	}
}

bool CGameControllerHunterN::OnPlayerTryRespawn(CPlayer *pPlayer, vec2 Pos)
{
	const int CID = pPlayer->GetCID();
	if(m_RoundActive && !IsWarmup() &&
		((LimitedLives() && (!m_aParticipants[CID] || m_aLives[CID] <= 0)) ||
			(m_Mode == MODE_ZS && !m_aParticipants[CID])))
	{
		pPlayer->CancelSpawn();
		return false;
	}
	return true;
}

void CGameControllerHunterN::GiveLoadout(CCharacter *pChr, bool Randomize)
{
	const int CID = pChr->GetPlayer()->GetCID();
	// A lucky god-tier roll is announced; with asylum_god_only every roll is one, so stay quiet.
	bool Lucky = false;
	if(Randomize || m_aLoadouts[CID][0] < 0)
		for(int Slot = 0; Slot < 3; ++Slot)
		{
			const bool God = m_GodOnly || secure_rand_below(100) < m_GodChance;
			Lucky |= God && !m_GodOnly;
			m_aLoadouts[CID][Slot] = AsylumRandomItem(Slot, God);
			// The juggernaut's heavy arsenal is intentionally asymmetric.
			if(m_Mode == MODE_JGN && CID != m_Juggernaut)
				while(m_aLoadouts[CID][Slot] == ASYLUM_AMERICA || m_aLoadouts[CID][Slot] == ASYLUM_TASER)
					m_aLoadouts[CID][Slot] = AsylumRandomItem(Slot);
		}
	if(m_Mode == MODE_GG)
	{
		m_aLoadouts[CID][0] = ProgressItem(CID);
		m_aLoadouts[CID][1] = ASYLUM_MEDKIT;
		m_aLoadouts[CID][2] = ASYLUM_DASH;
	}
	if(m_Mode == MODE_ZS && m_aInfected[CID] && m_RoundActive)
	{
		m_aLoadouts[CID][0] = ASYLUM_KNIFE;
		m_aLoadouts[CID][1] = ASYLUM_DASH;
		m_aLoadouts[CID][2] = ASYLUM_CLEAVE;
	}
	if(m_Mode == MODE_JGN && CID == m_Juggernaut && m_RoundActive)
	{
		m_aLoadouts[CID][0] = ASYLUM_DYINGPAN;
		m_aLoadouts[CID][1] = ASYLUM_BAZOOKA;
		m_aLoadouts[CID][2] = ASYLUM_MANTLE;
	}
	for(int Slot = 0; Slot < 3; ++Slot)
		pChr->ForceSetWeapon(Slot, AsylumWeaponID(m_aLoadouts[CID][Slot]), -1);
	pChr->SetWeaponSlot(m_Mode == MODE_GG || (m_Mode == MODE_ZS && m_aInfected[CID]) ? 0 : 1, false);
	m_aPendingReroll[CID] = -1;
	SendLoadout(CID, true);
	if(Lucky)
	{
		// Mode overrides above may have replaced the rolled items.
		char aItems[192] = "";
		for(int Slot = 0; Slot < 3; ++Slot)
			if(AsylumItem(m_aLoadouts[CID][Slot]).m_God)
			{
				if(aItems[0])
					str_append(aItems, "、", sizeof(aItems));
				str_append(aItems, AsylumItem(m_aLoadouts[CID][Slot]).m_pName, sizeof(aItems));
			}
		if(aItems[0])
		{
			char aAnnouncement[256];
			str_format(aAnnouncement, sizeof(aAnnouncement), "★ 大神武器降临！%s 抽到了 %s", Server()->ClientName(CID), aItems);
			SendChatTarget(-1, aAnnouncement);
			AsylumPlayMeme(GameWorld(), ASYLUM_MEME_FANFARE, pChr->m_Pos, true, CmaskOne(CID));
			AsylumPlayMeme(GameWorld(), ASYLUM_MEME_FANFARE, pChr->m_Pos, false, ~CmaskOne(CID));
		}
	}
	char aBuf[96];
	str_format(aBuf, sizeof(aBuf), "loadout cid=%d mode=%d items=%d,%d,%d", CID, m_Mode,
		m_aLoadouts[CID][0], m_aLoadouts[CID][1], m_aLoadouts[CID][2]);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "asylum", aBuf);
}

void CGameControllerHunterN::OnCharacterSpawn(CCharacter *pChr)
{
	const int CID = pChr->GetPlayer()->GetCID();
	pChr->GetPlayer()->SetClass(CLASS_NONE);
	pChr->RemoveWeapons();
	pChr->m_MaxHealth = 100;
	if(m_Mode == MODE_ELIM)
		pChr->m_MaxHealth = minimum(200, 100 + m_aKills[CID] * 20);
	if(m_Mode == MODE_ZS && m_aInfected[CID])
		pChr->m_MaxHealth = 120;
	if(m_Mode == MODE_JGN && CID == m_Juggernaut)
		pChr->m_MaxHealth = clamp(300 + GetRealPlayerNum() * 150, 600, 2000);
	pChr->m_MaxArmor = 100;
	pChr->IncreaseHealth(pChr->m_MaxHealth);
	pChr->SetArmor(m_Mode == MODE_ZS && m_aInfected[CID] ? 0 : (m_Mode == MODE_JGN && CID == m_Juggernaut ? 100 : m_SpawnArmor));
	pChr->SetWeaponTimerType(WEAPON_TIMER_INDIVIDUAL);
	pChr->Protect(pChr->GetPlayer()->m_AsylumTestGod ? 0.0f : (float)m_SpawnProtection, true);
	m_aLastDamageFrom[CID] = m_aLastDamageWeapon[CID] = m_aLastDamageTick[CID] = -1;
	GiveLoadout(pChr, true);
}

void CGameControllerHunterN::RerollLoadout(CCharacter *pChr)
{
	if(!pChr || !pChr->IsAlive() || !GetPlayerIfInRoom(pChr->GetPlayer()->GetCID()))
		return;
	const int CID = pChr->GetPlayer()->GetCID();
	if(m_Mode == MODE_GG)
	{
		GameServer()->SendChatTarget(CID, "GG模式不允许重抽进阶武器。");
		return;
	}
	if(!pChr->GetPlayer()->m_AsylumNoCooldown && Server()->Tick() - m_aLastRerollTick[CID] < Server()->TickSpeed() * 10)
	{
		GameServer()->SendChatTarget(CID, "重抽道具共享10秒冷却；再次抽到骰子也不会重置。");
		return;
	}
	m_aLastRerollTick[CID] = Server()->Tick();
	m_aPendingReroll[CID] = Server()->Tick();
}

void CGameControllerHunterN::ShowScreenText(int CID, const char *pText, float Seconds)
{
	if(CID < 0 || CID >= MAX_CLIENTS || !GetPlayerIfInRoom(CID))
		return;
	m_aScreenTextUntil[CID] = Server()->Tick() + round_to_int(Seconds * Server()->TickSpeed());
	GameServer()->SendBroadcast(pText, CID, false);
}

void CGameControllerHunterN::ShowJumpscare(int CID, float Seconds)
{
	if(CID < 0 || CID >= MAX_CLIENTS || !GetPlayerIfInRoom(CID) || !AsylumHasJumpscareMap(GameWorld()))
		return;
	m_aJumpscareStart[CID] = Server()->Tick();
	m_aJumpscareUntil[CID] = Server()->Tick() + round_to_int(clamp(Seconds, 0.0f, ASYLUM_JUMPSCARE_FADE_MS / 1000.0f) * Server()->TickSpeed());
}

int CGameControllerHunterN::MapAnimationStartTick(int SnappingClient, int DefaultStartTick) const
{
	if(SnappingClient < 0 || SnappingClient >= MAX_CLIENTS || !GetPlayerIfInRoom(SnappingClient))
		return DefaultStartTick;
	if(Server()->Tick() >= m_aJumpscareUntil[SnappingClient])
	{
		const int VisualMillis = GameWorld()->TimeStopVisualMillis();
		if(VisualMillis >= 0 && AsylumHasTimeStopMap(GameWorld()))
		{
			const int Offset = (int)(((int64)ASYLUM_WORLD_GRAY_BASE_MS + VisualMillis) * Server()->TickSpeed() / 1000);
			return Server()->Tick() - Offset;
		}
		return DefaultStartTick;
	}
	// The synchronized color envelope only shows in the last ~3 seconds before
	// INT32_MAX milliseconds. Leave margins for old clients' float precision.
	// 64-bit multiplication avoids overflow; only the transmitted clock changes.
	const int Offset = (int)((int64)ASYLUM_JUMPSCARE_START_MS * Server()->TickSpeed() / 1000);
	return m_aJumpscareStart[SnappingClient] - Offset;
}

int CGameControllerHunterN::TheWorldCooldown(int CID) const
{
	CPlayer *pPlayer = CID >= 0 && CID < MAX_CLIENTS ? GetPlayerIfInRoom(CID) : nullptr;
	return pPlayer && !pPlayer->m_AsylumNoCooldown ? pPlayer->m_TheWorldCooldown.Remaining(Server()->Tick()) : 0;
}

bool CGameControllerHunterN::ActivateTheWorld(int CID)
{
	CPlayer *pPlayer = CID >= 0 && CID < MAX_CLIENTS ? GetPlayerIfInRoom(CID) : nullptr;
	CCharacter *pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;
	if(!pChr || !pChr->IsAlive() || pChr->IsFrozen() || pChr->IsDisabled() ||
		TheWorldCooldown(CID) > 0 || (!IsGameRunning() && !IsWarmup()) ||
		!GameWorld()->StartTimeStop(CID, ASYLUM_WORLD_SECONDS * Server()->TickSpeed()))
		return false;
	if(pPlayer->m_AsylumNoCooldown)
		pPlayer->m_TheWorldCooldown.Reset();
	else
		pPlayer->m_TheWorldCooldown.Activate(Server()->Tick(), Server()->TickSpeed());
	const vec2 Origin = pChr->m_Pos;
	const float Diagonal = length(vec2((float)GameServer()->Collision()->GetWidth(),
		(float)GameServer()->Collision()->GetHeight()) * 32.0f);
	for(int Listener = 0; Listener < MAX_CLIENTS; ++Listener)
	{
		CPlayer *pListener = GetPlayerIfInRoom(Listener);
		if(!pListener)
			continue;
		CCharacter *pCharacter = pListener->GetCharacter();
		const vec2 ListenerPos = pCharacter ? pCharacter->m_Pos : pListener->m_ViewPos;
		const int Level = AsylumWorldVolumeLevel(distance(Origin, ListenerPos), Diagonal);
		char aSample[32];
		str_format(aSample, sizeof(aSample), "the_world_%02d", Level);
		// Targeted global samples avoid the client's short default spatial cutoff.
		// Volume is baked into variants of the SAME user recording, never synthesized.
		if(!AsylumPlayMapSound(GameWorld(), aSample, Origin, true, CmaskOne(Listener)))
			GameWorld()->CreateSoundGlobal(SOUND_CTF_RETURN, CmaskOne(Listener));
	}
	SendChatTarget(-1, "ザ・ワールド！");
	return true;
}

bool CGameControllerHunterN::CanCombatInteract(int From, int To) const
{
	if(To < 0 || To >= MAX_CLIENTS || From >= MAX_CLIENTS)
		return false;
	CPlayer *pTarget = GetPlayerIfInRoom(To);
	if(!pTarget || pTarget->GetTeam() == TEAM_SPECTATORS)
		return false;
	if(From < 0)
		return true;
	CPlayer *pAttacker = GetPlayerIfInRoom(From);
	if(!pAttacker || pAttacker->GetTeam() == TEAM_SPECTATORS)
		return false;
	if(From == To)
		return true;
	if(IsFriendlyFire(To, From))
		return false;
	if(m_Mode == MODE_ZS && m_RoundActive && m_aInfected[From] == m_aInfected[To])
		return false;
	if(m_Mode == MODE_JGN && m_RoundActive && From != m_Juggernaut && To != m_Juggernaut)
		return false;
	return !pTarget->GetCharacter() || !pTarget->GetCharacter()->IsProtected();
}

bool CGameControllerHunterN::CanWeaponInteract(int From, int To, int WeaponID) const
{
	if(!CanCombatInteract(From, To))
		return false;
	return m_Mode != MODE_GG || From < 0 || From == To ||
		(m_aPendingReroll[From] < 0 && WeaponID == AsylumWeaponID(ProgressItem(From)));
}

void CGameControllerHunterN::SendLoadout(int CID, bool Detailed)
{
	CPlayer *pPlayer = GetPlayerIfInRoom(CID);
	if(!pPlayer)
		return;
	char aState[160];
	str_format(aState, sizeof(aState), "%s | 剩余 %ds | 击杀 %d", m_pGameType, RemainingSeconds(), pPlayer->m_Score);
	if(m_Mode == MODE_GG)
		str_format(aState, sizeof(aState), "GG | 阶段 %d/%d%s | 剩余 %ds", minimum(m_aStages[CID] + 1, NUM_PROGRESS_STAGES), NUM_PROGRESS_STAGES,
			m_aStages[CID] == NUM_PROGRESS_STAGES - 1 ? " 黄金勺" : "", RemainingSeconds());
	if(LimitedLives())
		str_format(aState, sizeof(aState), "%s | %s | 剩余命数 %d | 剩余 %ds", m_pGameType,
			m_Mode == MODE_ELIM ? "淘汰赛" : (CID == m_Juggernaut ? "巨人" : "挑战者"), m_aLives[CID], RemainingSeconds());
	if(m_Mode == MODE_ELIM && m_SafeZoneActive)
	{
		CCharacter *pChr = pPlayer->GetCharacter();
		str_format(aState, sizeof(aState), "ELIM | 命数 %d | 收缩安全区 %.0f | %s", m_aLives[CID], m_SafeZoneRadius,
			pChr && distance(pChr->m_Pos, m_SafeZoneCenter) > m_SafeZoneRadius ? "圈外：向中心返回" : "圈内");
	}
	if(m_Mode == MODE_ZS)
		str_format(aState, sizeof(aState), "ZS | %s | 剩余 %ds", m_aInfected[CID] ? "感染者" : "生还者", RemainingSeconds());
	if(!pPlayer->GetCharacter() || m_aLoadouts[CID][0] < 0)
	{
		if(m_RoundActive && (LimitedLives() || m_Mode == MODE_ZS) && (!m_aParticipants[CID] || m_aLives[CID] <= 0))
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "%s\n已淘汰 / 等待下一局", aState);
			GameServer()->SendBroadcast(aBuf, CID, false);
		}
		return;
	}
	if(Detailed)
		for(int Slot = 0; Slot < 3; ++Slot)
		{
			const SAsylumItem &Item = AsylumItem(m_aLoadouts[CID][Slot]);
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "[%d] %s — %s", Slot + 1, Item.m_pName, Item.m_pDescription);
			GameServer()->SendChatTarget(CID, aBuf);
		}
	CCharacter *pChr = pPlayer->GetCharacter();
	CWeapon *pUtility = pChr->GetWeapon(2);
	float Cooldown = 0.0f;
	int MantleCharges = -1;
	if(pUtility && AsylumIsWeapon(pUtility->GetWeaponID()))
	{
		CAsylumWeapon *pItem = (CAsylumWeapon *)pUtility;
		Cooldown = pItem->CooldownTicks() / (float)Server()->TickSpeed();
		if(pItem->Item() == ASYLUM_MANTLE)
			MantleCharges = pItem->MantleCharges();
	}
	char aExtra[80];
	str_format(aExtra, sizeof(aExtra), "冷却 %.1fs", Cooldown);
	if(MantleCharges >= 0)
		str_format(aExtra, sizeof(aExtra), "护盾余量 %d | 冷却 %.1fs", MantleCharges, Cooldown);
	if(pPlayer->m_AsylumNoCooldown)
		str_copy(aExtra, "测试模式：无冷却", sizeof(aExtra));
	if(pPlayer->m_AsylumTestGod)
		str_append(aExtra, " | 无敌", sizeof(aExtra));
	char aSkills[192];
	str_copy(aSkills, m_Mode == MODE_GG ? "GG：附加技能禁用" : "E / R：当前装备无附加技能", sizeof(aSkills));
	CWeapon *pActive = pChr->GetActiveWeapon() >= 0 && pChr->GetActiveWeapon() < NUM_WEAPON_SLOTS ? pChr->GetWeapon(pChr->GetActiveWeapon()) : nullptr;
	if(m_Mode != MODE_GG && pActive && AsylumIsWeapon(pActive->GetWeaponID()))
		((CAsylumWeapon *)pActive)->SkillStatus(aSkills, sizeof(aSkills));
	char aBuf[768];
	str_format(aBuf, sizeof(aBuf), "%s\nHP %d/%d | 护甲 %d/%d\n[1] %s | [2] %s\n[3] %s | %s\n%s", aState,
		maximum(0, pChr->GetHealth()), pChr->m_MaxHealth, pChr->GetArmor(), pChr->m_MaxArmor,
		AsylumItem(m_aLoadouts[CID][0]).m_pName, AsylumItem(m_aLoadouts[CID][1]).m_pName,
		AsylumItem(m_aLoadouts[CID][2]).m_pName, aExtra, aSkills);
	GameServer()->SendBroadcast(aBuf, CID, false);
}

int CGameControllerHunterN::OnCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, int Weapon)
{
	CPlayer *pPlayer = pVictim->GetPlayer();
	const int CID = pPlayer->GetCID();
	GameWorld()->EndTimeStopFor(CID);
	m_aPendingReroll[CID] = -1;
	pPlayer->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() * m_RespawnDelay;
	if(!IsGameRunning() || !m_RoundActive || Weapon == WEAPON_GAME)
		return DEATH_NO_SUICIDE_PANATY | DEATH_SKIP_SCORE;
	const bool ValidKill = pKiller && pKiller != pPlayer && GetPlayerIfInRoom(pKiller->GetCID()) == pKiller;
	if(ValidKill && CanCombatInteract(pKiller->GetCID(), CID) &&
		(m_Mode != MODE_TDM || pKiller->GetTeam() != pPlayer->GetTeam()))
	{
		const bool Deferred = GameWorld()->IsTimeStopActive();
		const int Reduction = pKiller->m_TheWorldCooldown.AwardKill(Server()->Tick(), Server()->TickSpeed(), Deferred);
		if(Reduction > 0)
			GameServer()->SendChatTarget(pKiller->GetCID(), Deferred ?
				"[The World] 有效击杀：时停结束后返还2秒冷却（本次最多10秒）。" :
				"[The World] 有效击杀：冷却减少2秒（本次最多10秒）。");
	}
	if(m_Mode == MODE_TDM && ValidKill && pKiller->GetTeam() != pPlayer->GetTeam())
		++m_aTeamscore[pKiller->GetTeam() & 1];
	if(m_Mode == MODE_GG)
	{
		if(ValidKill && m_aLastDamageTick[CID] == Server()->Tick() && m_aLastDamageFrom[CID] == pKiller->GetCID() &&
			m_aLastDamageWeapon[CID] == AsylumWeaponID(ProgressItem(pKiller->GetCID())))
		{
			const int Killer = pKiller->GetCID();
			++m_aStages[Killer];
			pKiller->m_Score = m_aStages[Killer];
			if(m_aStages[Killer] >= NUM_PROGRESS_STAGES)
				FinishPlayer(Killer, "完成黄金勺最终击杀");
			else
				m_aPendingReroll[Killer] = Server()->Tick();
		}
		return DEATH_NO_SUICIDE_PANATY | DEATH_SKIP_SCORE;
	}
	if(m_Mode == MODE_ELIM || m_Mode == MODE_JGN)
	{
		if(m_aParticipants[CID])
			m_aLives[CID] = maximum(0, m_aLives[CID] - 1);
		if(m_aLives[CID] <= 0)
		{
			pPlayer->CancelSpawn();
			GameServer()->SendChatTarget(CID, "命数耗尽，已淘汰；下一局自动重新参赛。");
		}
		if(ValidKill)
		{
			++m_aKills[pKiller->GetCID()];
			if(m_Mode == MODE_ELIM && pKiller->GetCharacter())
			{
				CCharacter *pChr = pKiller->GetCharacter();
				pChr->m_MaxHealth = minimum(200, 100 + m_aKills[pKiller->GetCID()] * 20);
				pChr->IncreaseHealth(20);
			}
		}
		return DEATH_NO_SUICIDE_PANATY | DEATH_SKIP_SCORE;
	}
	if(m_Mode == MODE_ZS)
	{
		if(m_aParticipants[CID] && !m_aInfected[CID])
		{
			m_aInfected[CID] = true;
			m_InfectionBonusSeconds += 15;
			char aBuf[160];
			str_format(aBuf, sizeof(aBuf), "[ZS] %s 已感染！复活后转为感染者；倒计时延长15秒。", Server()->ClientName(CID));
			SendChatTarget(-1, aBuf);
		}
		return DEATH_NO_SUICIDE_PANATY | DEATH_SKIP_SCORE;
	}
	return DEATH_NO_SUICIDE_PANATY | DEATH_NORMAL;
}

int CGameControllerHunterN::OnCharacterTakeDamage(CCharacter *pChr, vec2 &Force, int &Dmg,
	int From, int WeaponType, int WeaponID, bool IsExplosion)
{
	const int CID = pChr->GetPlayer()->GetCID();
	if(pChr->IsProtected() || !CanCombatInteract(From, CID))
		return DAMAGE_SKIP;
	if(m_Mode == MODE_GG && From >= 0 && From < MAX_CLIENTS && From != CID)
	{
		// A single melee swing or volley cannot finish two consecutive stages.
		if(m_aPendingReroll[From] >= 0 || WeaponID != AsylumWeaponID(ProgressItem(From)))
			return DAMAGE_SKIP;
		if(m_aStages[From] == NUM_PROGRESS_STAGES - 1)
			Dmg = maximum(Dmg, 100);
	}
	if(m_Mode == MODE_JGN && From >= 0 && From == m_Juggernaut && From != CID && Dmg > 0)
		Dmg = maximum(1, Dmg * 3 / 2);
	if(Dmg > 0)
	{
		CWeapon *pUtility = pChr->GetWeapon(2);
		if(pUtility && AsylumIsWeapon(pUtility->GetWeaponID()) && ((CAsylumWeapon *)pUtility)->Item() == ASYLUM_MANTLE &&
			((CAsylumWeapon *)pUtility)->ConsumeMantle())
		{
			pChr->Protect(0.25f, false);
			GameWorld()->CreatePlayerSpawn(pChr->m_Pos);
			GameWorld()->CreateSound(pChr->m_Pos, SOUND_PICKUP_ARMOR);
			GameServer()->SendChatTarget(CID, "Holy Mantle 抵挡了一次伤害。");
			return DAMAGE_SKIP;
		}
		CWeapon *pActive = pChr->GetActiveWeapon() >= 0 && pChr->GetActiveWeapon() < NUM_WEAPON_SLOTS ? pChr->GetWeapon(pChr->GetActiveWeapon()) : nullptr;
		if(pActive && AsylumIsWeapon(pActive->GetWeaponID()) && ((CAsylumWeapon *)pActive)->HandleIncomingDamage(Force, Dmg, From))
			return DAMAGE_SKIP;
		m_aLastDamageWeapon[CID] = WeaponID;
		m_aLastDamageFrom[CID] = From;
		m_aLastDamageTick[CID] = Server()->Tick();
	}
	return m_Mode == MODE_JGN && CID == m_Juggernaut ? DAMAGE_NO_KNOCKBACK : DAMAGE_NORMAL;
}

void CGameControllerHunterN::OnCharacterDamageApplied(CCharacter *pChr, int From, int WeaponID, int HealthLoss, int ArmorLoss)
{
	if(!pChr || From < 0 || From >= MAX_CLIENTS || !AsylumIsWeapon(WeaponID))
		return;
	const int CID = pChr->GetPlayer()->GetCID();
	if(From == CID || GetPlayerIfInRoom(CID) != pChr->GetPlayer())
		return;
	CPlayer *pAttacker = GetPlayerIfInRoom(From);
	CCharacter *pOwner = pAttacker ? pAttacker->GetCharacter() : nullptr;
	const int AppliedDamage = maximum(0, HealthLoss) + maximum(0, ArmorLoss);
	if(!pOwner || !pOwner->IsAlive() || AppliedDamage <= 0)
		return;
	// Charge the weapon that dealt the damage, not a newly selected weapon;
	// blocked hits and overkill cannot grant charge because only actual loss is reported.
	for(int Slot = 0; Slot < NUM_WEAPON_SLOTS; ++Slot)
	{
		CWeapon *pWeapon = pOwner->GetWeapon(Slot);
		if(pWeapon && pWeapon->GetWeaponID() == WeaponID)
		{
			((CAsylumWeapon *)pWeapon)->AddCharge(AppliedDamage);
			break;
		}
	}
}

void CGameControllerHunterN::ActivateAsylumSkill(int CID, bool Ultimate)
{
	if(CID < 0 || CID >= MAX_CLIENTS || (!IsGameRunning() && !IsWarmup()))
		return;
	CPlayer *pPlayer = GetPlayerIfInRoom(CID);
	CCharacter *pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;
	if(!pChr || !pChr->IsAlive() || pChr->IsFrozen() || pChr->IsDisabled() || GameWorld()->IsClientTimeStopped(CID))
		return;
	if(m_Mode == MODE_GG)
	{
		GameServer()->SendChatTarget(CID, "GG模式禁用附加技能，必须用当前进阶武器击杀。");
		return;
	}
	CWeapon *pWeapon = pChr->GetActiveWeapon() >= 0 && pChr->GetActiveWeapon() < NUM_WEAPON_SLOTS ? pChr->GetWeapon(pChr->GetActiveWeapon()) : nullptr;
	if(pWeapon && AsylumIsWeapon(pWeapon->GetWeaponID()))
		((CAsylumWeapon *)pWeapon)->ActivateSkill(Ultimate, pChr->GetAimDirection());
}

void CGameControllerHunterN::OnPostTick()
{
	const bool TimeStopActive = GameWorld()->IsTimeStopActive();
	for(int CID = 0; CID < MAX_CLIENTS; ++CID)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(CID);
		if(!pPlayer)
			continue;
		if(TimeStopActive || GameWorld()->m_Paused)
			pPlayer->m_TheWorldCooldown.Pause(Server()->Tick());
		else
			pPlayer->m_TheWorldCooldown.ApplyPending(Server()->Tick());
	}
	if(GameWorld()->IsTimeStopped() && !GameWorld()->m_Paused)
	{
		++m_GameStartTick;
		if(m_NextZoneDamageTick > 0)
			++m_NextZoneDamageTick;
		for(int CID = 0; CID < MAX_CLIENTS; ++CID)
		{
			if(!GameWorld()->IsClientTimeStopped(CID))
				continue;
			if(m_aPendingReroll[CID] >= 0) ++m_aPendingReroll[CID];
			if(m_aLastRerollTick[CID] > 0) ++m_aLastRerollTick[CID];
			if(m_aJumpscareUntil[CID] > Server()->Tick()) { ++m_aJumpscareStart[CID]; ++m_aJumpscareUntil[CID]; }
		}
	}
	if(!IsGameRunning() && !IsWarmup())
		return;
	if(!GameWorld()->IsTimeStopped() && IsGameRunning() && m_SafeZoneActive && m_RoundActive && Server()->Tick() >= m_NextZoneDamageTick)
	{
		m_NextZoneDamageTick = Server()->Tick() + Server()->TickSpeed();
		m_SafeZoneRadius = maximum(64.0f, m_SafeZoneRadius - 24.0f);
		GameWorld()->CreatePlayerSpawn(m_SafeZoneCenter);
		for(int i = 0; i < 16; ++i)
		{
			const float Angle = 2.0f * pi * i / 16.0f;
			GameWorld()->CreateHammerHit(m_SafeZoneCenter + vec2(cosf(Angle), sinf(Angle)) * m_SafeZoneRadius);
		}
		for(int CID = 0; CID < MAX_CLIENTS; ++CID)
		{
			CPlayer *pPlayer = GetPlayerIfInRoom(CID);
			CCharacter *pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;
			if(pChr && m_aParticipants[CID] && distance(pChr->m_Pos, m_SafeZoneCenter) > m_SafeZoneRadius)
				pChr->TakeDamage(vec2(0, 0), 10, -1, WEAPON_WORLD, WEAPON_ID_WORLD, false);
		}
	}
	for(int CID = 0; CID < MAX_CLIENTS; ++CID)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(CID);
		if(!pPlayer || pPlayer->GetTeam() == TEAM_SPECTATORS)
			continue;
		if(GameWorld()->IsClientTimeStopped(CID))
			continue;
		CCharacter *pChr = pPlayer->GetCharacter();
		if(pChr)
		{
			if(m_aPendingReroll[CID] >= 0 && m_aPendingReroll[CID] < Server()->Tick())
				GiveLoadout(pChr, true);
			CWeapon *pActive = pChr->GetActiveWeapon() >= 0 && pChr->GetActiveWeapon() < NUM_WEAPON_SLOTS ? pChr->GetWeapon(pChr->GetActiveWeapon()) : nullptr;
			if(pActive && AsylumIsWeapon(pActive->GetWeaponID()) && ((CAsylumWeapon *)pActive)->Item() == ASYLUM_PARASOL && pChr->Core()->m_Vel.y > 2.0f)
				pChr->Core()->m_Vel.y = 2.0f;
		}
		if(!pChr && !pPlayer->m_RespawnDisabled && Server()->Tick() >= pPlayer->m_RespawnTick)
			pPlayer->Respawn();
		if(Server()->Tick() % maximum(1, Server()->TickSpeed() / 2) == 0 && Server()->Tick() >= m_aScreenTextUntil[CID])
			SendLoadout(CID, false);
	}
}

void CGameControllerHunterN::FinishPlayer(int CID, const char *pReason)
{
	if(!m_RoundActive)
		return;
	m_RoundActive = false;
	char aBuf[256];
	if(CID >= 0)
	{
		str_format(aBuf, sizeof(aBuf), "[%s] %s 获胜！%s", m_pGameType, Server()->ClientName(CID), pReason);
		CPlayer *pPlayer = GetPlayerIfInRoom(CID);
		if(pPlayer && LimitedLives())
			++pPlayer->m_Score;
	}
	else
		str_format(aBuf, sizeof(aBuf), "[%s] %s", m_pGameType, pReason);
	SendChatTarget(-1, aBuf);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "asylum", aBuf);
	EndMatch();
}

void CGameControllerHunterN::FinishSide(bool InfectedWon, const char *pReason)
{
	if(!m_RoundActive)
		return;
	for(int CID = 0; CID < MAX_CLIENTS; ++CID)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(CID);
		if(pPlayer && m_aParticipants[CID] && m_aInfected[CID] == InfectedWon)
			++pPlayer->m_Score;
	}
	char aBuf[192];
	str_format(aBuf, sizeof(aBuf), "%s获胜！%s", InfectedWon ? "感染者" : "生还者", pReason);
	FinishPlayer(-1, aBuf);
}

void CGameControllerHunterN::DoWincheckMatch()
{
	if(!m_RoundActive || !IsGameRunning() || GameWorld()->IsTimeStopped())
		return;
	const bool TimeUp = RemainingSeconds() <= 0 ||
		(m_GameInfo.m_TimeLimit > 0 && Server()->Tick() - m_GameStartTick >= m_GameInfo.m_TimeLimit * 60 * Server()->TickSpeed());
	if(m_Mode == MODE_ZS)
	{
		int Survivors = 0, Infected = 0;
		for(int CID = 0; CID < MAX_CLIENTS; ++CID)
			if(GetPlayerIfInRoom(CID) && m_aParticipants[CID])
				(m_aInfected[CID] ? Infected : Survivors)++;
		if(Survivors == 0)
			FinishSide(true, "所有生还者均已感染或退出");
		else if(Infected == 0 || TimeUp)
			FinishSide(false, Infected == 0 ? "感染者退出比赛" : "成功坚持到倒计时结束");
		return;
	}
	if(m_Mode == MODE_JGN)
	{
		int Challengers = 0;
		for(int CID = 0; CID < MAX_CLIENTS; ++CID)
			if(CID != m_Juggernaut && GetPlayerIfInRoom(CID) && m_aParticipants[CID] && m_aLives[CID] > 0)
				++Challengers;
		if(m_Juggernaut < 0 || !GetPlayerIfInRoom(m_Juggernaut) || !m_aParticipants[m_Juggernaut] || m_aLives[m_Juggernaut] <= 0)
			FinishPlayer(-1, "挑战者获胜！巨人已被击败或退出。");
		else if(Challengers == 0)
			FinishPlayer(m_Juggernaut, "所有挑战者均已淘汰");
		else if(TimeUp)
			FinishPlayer(-1, "挑战者获胜！成功坚持到倒计时结束。");
		return;
	}
	if(m_Mode == MODE_ELIM)
	{
		int Alive = 0, Winner = -1;
		for(int CID = 0; CID < MAX_CLIENTS; ++CID)
		{
			if(!GetPlayerIfInRoom(CID) || !m_aParticipants[CID] || m_aLives[CID] <= 0)
				continue;
			++Alive;
			Winner = CID;
		}
		if(Alive <= 1)
			FinishPlayer(Alive == 1 ? Winner : -1, Alive == 1 ? "最后存活者" : "全部淘汰，本局平局。");
		else if(TimeUp && !m_SafeZoneActive)
		{
			vec2 Center(0, 0);
			int Characters = 0;
			for(int CID = 0; CID < MAX_CLIENTS; ++CID)
			{
				CPlayer *pPlayer = GetPlayerIfInRoom(CID);
				CCharacter *pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;
				if(pChr && m_aParticipants[CID] && m_aLives[CID] > 0)
				{
					Center += pChr->m_Pos;
					++Characters;
				}
			}
			// Delay until there is a real spawn position; large maps need a local center.
			if(Characters > 0)
			{
				m_SafeZoneCenter = Center / (float)Characters;
				// A mean inside a wall is inaccessible; use the nearest living tee.
				if(GameServer()->Collision()->CheckPoint(m_SafeZoneCenter))
				{
					float Closest = 1e20f;
					vec2 Fallback = m_SafeZoneCenter;
					for(int CID = 0; CID < MAX_CLIENTS; ++CID)
					{
						CPlayer *pPlayer = GetPlayerIfInRoom(CID);
						CCharacter *pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;
						if(pChr && m_aParticipants[CID] && m_aLives[CID] > 0 && distance(pChr->m_Pos, m_SafeZoneCenter) < Closest)
						{
							Closest = distance(pChr->m_Pos, m_SafeZoneCenter);
							Fallback = pChr->m_Pos;
						}
					}
					m_SafeZoneCenter = Fallback;
				}
				m_SafeZoneRadius = 256.0f;
				for(int CID = 0; CID < MAX_CLIENTS; ++CID)
				{
					CPlayer *pPlayer = GetPlayerIfInRoom(CID);
					CCharacter *pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;
					if(pChr && m_aParticipants[CID] && m_aLives[CID] > 0)
						m_SafeZoneRadius = maximum(m_SafeZoneRadius, distance(pChr->m_Pos, m_SafeZoneCenter) + 128.0f);
				}
				m_SafeZoneActive = true;
				m_NextZoneDamageTick = Server()->Tick() + Server()->TickSpeed();
				SendChatTarget(-1, "[ELIM] 时间到！安全区开始收缩，圈外每秒受到10点伤害；最后存活者获胜。");
			}
		}
		return;
	}
	if(m_Mode == MODE_TDM)
	{
		const bool ScoreUp = m_GameInfo.m_ScoreLimit > 0 && maximum(m_aTeamscore[0], m_aTeamscore[1]) >= m_GameInfo.m_ScoreLimit;
		if((TimeUp || ScoreUp) && m_aTeamscore[0] != m_aTeamscore[1])
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "%s获胜！团队比分 %d:%d", m_aTeamscore[0] > m_aTeamscore[1] ? "红队" : "蓝队", m_aTeamscore[0], m_aTeamscore[1]);
			FinishPlayer(-1, aBuf);
		}
		else if(TimeUp || ScoreUp)
			m_SuddenDeath = 1;
		return;
	}
	int Best = -100000, Ties = 0, Winner = -1;
	for(int CID = 0; CID < MAX_CLIENTS; ++CID)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(CID);
		if(!pPlayer || pPlayer->GetTeam() == TEAM_SPECTATORS)
			continue;
		const int Score = m_Mode == MODE_GG ? m_aStages[CID] : pPlayer->m_Score;
		if(Score > Best)
		{
			Best = Score;
			Ties = 1;
			Winner = CID;
		}
		else if(Score == Best)
			++Ties;
	}
	if(TimeUp || (m_Mode == MODE_FFA && m_GameInfo.m_ScoreLimit > 0 && Best >= m_GameInfo.m_ScoreLimit))
	{
		if(Ties == 1)
			FinishPlayer(Winner, m_Mode == MODE_GG ? "时间到，武器进度领先" : "击杀分领先");
		else
			m_SuddenDeath = 1;
	}
}

bool CGameControllerHunterN::OnEntity(int Index, vec2 Pos, int Layer, int Flags, int Number)
{
	// Fixed pickups would bypass the randomized or mode-specific loadouts.
	return Index == ENTITY_ARMOR_1 || Index == ENTITY_HEALTH_1 ||
		Index == ENTITY_WEAPON_SHOTGUN || Index == ENTITY_WEAPON_GRENADE ||
		Index == ENTITY_WEAPON_LASER || Index == ENTITY_POWERUP_NINJA;
}
