/* Item Asylum inspired three-slot combat and six distinct game modes. */
#include "huntern.h"
#include <engine/storage.h>

#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/weapons.h>

namespace
{
const char *gs_apModeNames[] = {"Tee Asylum FFA", "Tee Asylum TDM", "Tee Asylum GG", "Tee Asylum ELIM", "Tee Asylum ZS", "Tee Asylum JGN", "Tee Asylum Tour"};
const char *gs_apRules[] = {
	"自由混战：每次出生重抽近战、远程、特殊道具，击杀计分。",
	"团队乱斗：红蓝两队，队友免伤，团队击杀分决定胜负。",
	"武器进阶：用槽位1击杀升级，共17阶段；最终黄金勺击杀获胜。槽位2/3为急救包和冲刺。",
	"三命淘汰：每人3条命，击杀增加20点生命上限（最高200）；超时安全区收缩，最后存活者获胜。",
	"感染生存：随机初始感染者，生还者死亡后转为感染者；撑到倒计时结束获胜。",
	"巨人讨伐：一人化为高生命巨人，其余挑战者各3条命；击败巨人或坚持到超时即可获胜。",
	"大厅巡回：触碰三块选图板投票，下一局随机武器FFA或合作BOSS；结束后返回无武器大厅。"};
const int gs_aModeSeconds[] = {480, 360, 480, 180, 240, 240, 480};
const int gs_aProgressItems[] = {
	ASYLUM_REVOLVER, ASYLUM_SMG, ASYLUM_SHOTGUN, ASYLUM_CROSSBOW,
	ASYLUM_FREEZERAY, ASYLUM_LAUNCHER, ASYLUM_BAZOOKA, ASYLUM_M1911,
	ASYLUM_RAILGUN, ASYLUM_PAN, ASYLUM_BAT, ASYLUM_ENERGYSWORD,
	ASYLUM_DARKHEART, ASYLUM_DYINGPAN, ASYLUM_KNIFE, ASYLUM_SPOON,
	ASYLUM_SPOON}; // Last stage is a golden spoon with bonus damage.
const int NUM_PROGRESS_STAGES = sizeof(gs_aProgressItems) / sizeof(gs_aProgressItems[0]);
}

CGameControllerHunterN::CGameControllerHunterN(int Mode) : IGameController(),
	m_Mode(clamp(Mode, (int)MODE_FFA, (int)MODE_TOUR)), m_RoundActive(false), m_Juggernaut(-1),
	m_InfectionBonusSeconds(0), m_SafeZoneActive(false), m_SafeZoneCenter(0, 0), m_SafeZoneRadius(0.0f), m_NextZoneDamageTick(0)
{
	for(int &ID : m_aBossVisualIDs) ID = -1;
	for(int &ID : m_aBossSnapCID) ID = -1;
	for(int &Vote : m_aMapVotes) Vote = -1;
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
		ClearAsylumProgress(CID);
	}
	INSTANCE_CONFIG_INT(&m_RespawnDelay, "asylum_respawn_delay", 2, 1, 10, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Respawn delay in seconds");
	INSTANCE_CONFIG_INT(&m_SpawnProtection, "asylum_spawn_protection", 1, 0, 5, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Spawn shield in seconds, cancelled on firing");
	// Kept as a zero-only compatibility setting so old configuration files do
	// not accidentally reinstate the previous 20-point spawn armor.
	INSTANCE_CONFIG_INT(&m_SpawnArmor, "asylum_spawn_armor", 0, 0, 0, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Deprecated: Item Asylum spawns have no armor");
	INSTANCE_CONFIG_INT(&m_StartingLives, "asylum_lives", 3, 1, 10, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Lives in ELIM and for JGN challengers");
	INSTANCE_CONFIG_INT(&m_RoundSeconds, "asylum_round_seconds", gs_aModeSeconds[m_Mode], 10, 3600, CFGFLAG_CHAT | CFGFLAG_INSTANCE, "Round duration in seconds");
	INSTANCE_CONFIG_INT(&m_LobbySeconds, "asylum_lobby_seconds", 45, 5, 120, CFGFLAG_INSTANCE, "Touch-board voting duration in Tour lobbies");
	InstanceConsole()->Register("asylum_status", "", CFGFLAG_CHAT | CFGFLAG_INSTANCE | CFGFLAG_NO_CONSENT, ConStatus, this, "Show mode, timer, lives and equipment");
	InstanceConsole()->Register("asylum_items", "", CFGFLAG_CHAT | CFGFLAG_INSTANCE | CFGFLAG_NO_CONSENT, ConItems, this, "List random items, upgrade modules and resulting weapons");
	InstanceConsole()->Register("asylum_loadout", "i[cid] i[melee] i[ranged] i[utility]", CFGFLAG_INSTANCE, ConLoadout, this, "Administrator: equip valid category items on an existing character");
	InstanceConsole()->Register("asylum_give", "i[cid] i[item]", CFGFLAG_INSTANCE, ConGive, this, "Administrator: replace only the item's corresponding inventory slot");
	InstanceConsole()->Register("asylum_inspect", "i[cid]", CFGFLAG_INSTANCE, ConInspect, this, "Administrator: read-only exact health, inventory and ability diagnostics");
	InstanceConsole()->Register("asylum_god", "i[cid] i[enabled]", CFGFLAG_INSTANCE, ConGod, this, "Administrator: toggle combat/death-tile invulnerability (0 or 1)");
	InstanceConsole()->Register("asylum_map", "s[map]", CFGFLAG_INSTANCE, ConMap, this, "Administrator: load a physical .map in the main room");
}

void CGameControllerHunterN::ConStatus(IConsole::IResult *pResult, void *pUserData)
{
	CGameControllerHunterN *pSelf = (CGameControllerHunterN *)pUserData;
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s | 剩余 %ds | %s", pSelf->m_pGameType, pSelf->RemainingSeconds(), gs_apRules[pSelf->m_Mode]);
	pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", aBuf);
	if(pSelf->m_TourBoss)
	{
		str_format(aBuf, sizeof(aBuf), "boss hp=%d/%d phase=%d pos=%.0f,%.0f attack=%d uses=%d shots=%d", pSelf->m_BossHealth, pSelf->m_BossMaxHealth, pSelf->m_BossPhase, pSelf->m_BossPos.x, pSelf->m_BossPos.y, pSelf->m_BossAttack, pSelf->m_BossAttackUses, (int)pSelf->m_BossShots.size());
		pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", aBuf);
	}
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
	char aHeader[96];
	int RandomItems = 0, Modules = 0;
	for(int Item = 0; Item < NUM_ASYLUM_ITEMS; ++Item)
	{
		if(AsylumItem(Item).m_Category < ASYLUM_CATEGORY_UPGRADE)
			++RandomItems;
		else if(AsylumItem(Item).m_Category == ASYLUM_CATEGORY_UPGRADE)
			++Modules;
	}
	str_format(aHeader, sizeof(aHeader), "随机装备%d件，另含%d个击杀升级模块及%d件升级武器。", RandomItems, Modules, NUM_ASYLUM_ITEMS - RandomItems - Modules);
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

void CGameControllerHunterN::ConGive(IConsole::IResult *pResult, void *pUserData)
{
	auto *pSelf = (CGameControllerHunterN *)pUserData;
	if(!pSelf->RequireAdmin(pResult)) return;
	const int CID = pResult->GetInteger(0), Item = pResult->GetInteger(1);
	CPlayer *pPlayer = CID >= 0 && CID < MAX_CLIENTS ? pSelf->GetPlayerIfInRoom(CID) : nullptr;
	if(!pPlayer || !pPlayer->GetCharacter() || Item < 0 || Item >= NUM_ASYLUM_ITEMS || pSelf->m_TourLobby)
	{
		pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", "Need a live player, valid item ID and an armed combat map");
		return;
	}
	const int Category = AsylumItem(Item).m_Category;
	const int Slot = Category <= ASYLUM_CATEGORY_UPGRADE ? Category : 1;
	CCharacter *pChr = pPlayer->GetCharacter();
	if(Slot == 1)
	{
		for(int &Kills : pSelf->m_aUpgradeKills[CID]) Kills = 0;
		pChr->RemoveWeapon(3); pSelf->m_aLoadouts[CID][3] = -1;
	}
	pSelf->m_aPendingConsume[CID] &= ~(1 << Slot);
	pSelf->m_aPendingUpgrade[CID] = pSelf->m_aPendingReroll[CID] = -1;
	pChr->RemoveWeapon(Slot);
	pChr->GiveWeapon(Slot, AsylumWeaponID(Item), -1);
	pSelf->m_aLoadouts[CID][Slot] = Item;
	pSelf->SendLoadout(CID, true);
	char aBuf[128]; str_format(aBuf, sizeof(aBuf), "give cid=%d item=%d slot=%d", CID, Item, Slot + 1);
	pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", aBuf);
}

void CGameControllerHunterN::ConInspect(IConsole::IResult *pResult, void *pUserData)
{
	auto *pSelf = (CGameControllerHunterN *)pUserData;
	if(!pSelf->RequireAdmin(pResult)) return;
	const int CID = pResult->GetInteger(0);
	CPlayer *pPlayer = CID >= 0 && CID < MAX_CLIENTS ? pSelf->GetPlayerIfInRoom(CID) : nullptr;
	CCharacter *pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;
	if(!pChr) { pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", "Need a live player in this room"); return; }
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "inspect cid=%d hp=%d armor=%d god=%d items=%d,%d,%d,%d phase=%d", CID, pChr->GetHealth(), pChr->GetArmor(), pSelf->m_aAdminGod[CID], pSelf->m_aLoadouts[CID][0], pSelf->m_aLoadouts[CID][1], pSelf->m_aLoadouts[CID][2], pSelf->m_aLoadouts[CID][3], pChr->Core()->m_AsylumPhase);
	pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", aBuf);
	CWeapon *pHeld = pChr->CurrentWeapon();
	if(pHeld && AsylumIsWeapon(pHeld->GetWeaponID()))
	{
		((CAsylumWeapon *)pHeld)->AdminStatus(aBuf, sizeof(aBuf));
		pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", aBuf);
	}
}

void CGameControllerHunterN::ConGod(IConsole::IResult *pResult, void *pUserData)
{
	auto *pSelf = (CGameControllerHunterN *)pUserData;
	if(!pSelf->RequireAdmin(pResult)) return;
	const int CID = pResult->GetInteger(0), Enabled = pResult->GetInteger(1);
	if(CID < 0 || CID >= MAX_CLIENTS || !pSelf->GetPlayerIfInRoom(CID) || (Enabled != 0 && Enabled != 1))
	{
		pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", "Need a player in this room and enabled=0 or 1");
		return;
	}
	pSelf->m_aAdminGod[CID] = Enabled != 0;
	char aBuf[96]; str_format(aBuf, sizeof(aBuf), "god cid=%d enabled=%d (cleared on leave/map/restart)", CID, Enabled);
	pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", aBuf);
	pSelf->GameServer()->SendChatTarget(CID, Enabled ? "管理员已开启你的无敌。" : "管理员已关闭你的无敌。");
}

void CGameControllerHunterN::ConMap(IConsole::IResult *pResult, void *pUserData)
{
	auto *pSelf = (CGameControllerHunterN *)pUserData;
	if(!pSelf->RequireAdmin(pResult)) return;
	const char *pMap = pResult->GetString(0);
	bool Valid = pMap[0] && str_length(pMap) < 96;
	for(const char *p = pMap; *p; ++p)
		Valid &= (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-';
	char aPath[128]; str_format(aPath, sizeof(aPath), "maps/%s.map", pMap);
	IOHANDLE MapFile = Valid ? pSelf->GameServer()->Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_ALL) : nullptr;
	const bool Exists = MapFile != nullptr;
	if(MapFile) io_close(MapFile);
	if(!Valid || pSelf->GameWorld()->Team() != 0 || !Exists)
	{
		pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", "Need main room 0 and an existing map name without path/.map suffix");
		return;
	}
	pSelf->Server()->ChangeMap(pMap);
}

bool CGameControllerHunterN::RequireAdmin(IConsole::IResult *pResult)
{
	// Guard the callback too: a room setting vote or a delegated console
	// must never turn ordinary-player consent into administrator authority.
	const int Requester = pResult->m_ClientID;
	if(Requester < 0 || (Requester < MAX_CLIENTS && Server()->GetAuthedState(Requester) == AUTHED_ADMIN)) return true;
	InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "asylum", "Administrator authentication required");
	return false;
}

void CGameControllerHunterN::ClearAsylumProgress(int CID, bool PreserveMantle)
{
	for(int &Kills : m_aUpgradeKills[CID])
		Kills = 0;
	m_aPendingConsume[CID] = 0;
	m_aPendingUpgrade[CID] = -1;
	m_aLastCombatTick[CID] = 0;
	m_aRegenCarry[CID] = 0;
	if(!PreserveMantle)
	{
		m_aMantleShield[CID] = false;
		m_aMantleLives[CID] = 0;
		m_aMantleReadyTick[CID] = 0;
		m_aMantleCharge[CID] = 0;
		m_aMantleInvulnerableUntil[CID] = 0;
	}
	m_aLoadouts[CID][3] = -1;
}

int CGameControllerHunterN::RemainingSeconds() const
{
	return maximum(0, m_RoundSeconds + m_InfectionBonusSeconds - (Server()->Tick() - m_GameStartTick) / Server()->TickSpeed());
}

int CGameControllerHunterN::ProgressItem(int CID) const
{
	return gs_aProgressItems[clamp(m_aStages[CID], 0, NUM_PROGRESS_STAGES - 1)];
}

void CGameControllerHunterN::OnGameStart(bool IsRound)
{
	if(m_Mode == MODE_TOUR)
	{
		m_TourLobby = str_startswith(g_Config.m_SvMap, "asylum_lobby_");
		m_TourBoss = str_comp(g_Config.m_SvMap, "asylum_10hourburstman") == 0;
	}
	m_RoundActive = true;
	m_Juggernaut = -1;
	m_InfectionBonusSeconds = 0;
	m_SafeZoneActive = false;
	m_SafeZoneRadius = 0.0f;
	// The actual roster is initialized after ResetGame deletes the old world.
}

void CGameControllerHunterN::OnWorldReset()
{
	if(m_Mode == MODE_TOUR) ResetTour();
	int aActive[MAX_CLIENTS], NumActive = 0;
	for(int CID = 0; CID < MAX_CLIENTS; ++CID)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(CID);
		m_aParticipants[CID] = pPlayer && pPlayer->GetTeam() != TEAM_SPECTATORS;
		m_aAdminGod[CID] = false;
		m_aLives[CID] = m_aParticipants[CID] ? (m_TourBoss ? 6 : m_StartingLives) : 0;
		m_aStages[CID] = m_aKills[CID] = 0;
		m_aInfected[CID] = false;
		m_aPendingReroll[CID] = m_aLastDamageWeapon[CID] = m_aLastDamageFrom[CID] = m_aLastDamageTick[CID] = -1;
		m_aLastRerollTick[CID] = -1000000;
		ClearAsylumProgress(CID);
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
	if(LimitedLives() || m_Mode == MODE_ZS || (m_TourBoss && m_BossMaxHealth > 0))
	{
		m_aParticipants[CID] = false;
		m_aLives[CID] = 0;
	}
	m_aPendingReroll[CID] = -1;
	ClearAsylumProgress(CID);
}

void CGameControllerHunterN::OnPlayerJoin(CPlayer *pPlayer)
{
	const int CID = pPlayer->GetCID();
	m_aBossSnapCID[CID] = -1;
	m_aAdminGod[CID] = false;
	m_aMapVotes[CID] = -1;
	for(int &Item : m_aLoadouts[CID])
		Item = -1;
	m_aPendingReroll[CID] = -1;
	ClearAsylumProgress(CID);
	pPlayer->SetClass(CLASS_NONE);
	GameServer()->SendChatTarget(CID, gs_apRules[m_Mode]);
	GameServer()->SendChatTarget(CID, "按1/2/3切换装备，左键使用；有弹匣的枪会自动装弹。武器击杀达到要求后在4槽领取升级模块，选4并左键使用。");
	GameServer()->SendChatTarget(CID, "100基础生命、无出生护甲；脱离战斗后自动回血。一次性道具使用后对应槽位消失。");
	GameServer()->SendChatTarget(CID, "E/R技能兼容普通客户端：控制台输入 bind e \"say /asylum_e\" 和 bind r \"say /asylum_r\"；GG模式禁用附加技能。");
	if(m_RoundActive && !IsWarmup() && (LimitedLives() || m_Mode == MODE_ZS || (m_TourBoss && m_BossHealth > 0)))
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
	m_aAdminGod[pPlayer->GetCID()] = false;
	m_aMapVotes[pPlayer->GetCID()] = -1;
	Forfeit(pPlayer->GetCID());
	ClearAsylumProgress(pPlayer->GetCID());
}

void CGameControllerHunterN::OnPlayerChangeTeam(CPlayer *pPlayer, int FromTeam, int ToTeam)
{
	ClearAsylumProgress(pPlayer->GetCID(), m_Mode == MODE_ZS);
	if(m_RoundActive && !IsWarmup() && (LimitedLives() || m_Mode == MODE_ZS || (m_TourBoss && m_BossMaxHealth > 0)))
	{
		Forfeit(pPlayer->GetCID());
		if(ToTeam != TEAM_SPECTATORS)
			pPlayer->CancelSpawn();
	}
}

bool CGameControllerHunterN::OnPlayerTryRespawn(CPlayer *pPlayer, vec2 Pos)
{
	const int CID = pPlayer->GetCID();
	if(m_TourBoss && m_BossHealth > 0 && (!m_aParticipants[CID] || m_aLives[CID] <= 0))
	{
		pPlayer->CancelSpawn();
		return false;
	}
	if(m_RoundActive && !IsWarmup() &&
		((LimitedLives() && (!m_aParticipants[CID] || m_aLives[CID] <= 0)) ||
			(m_Mode == MODE_ZS && !m_aParticipants[CID])))
	{
		pPlayer->CancelSpawn();
		return false;
	}
	return true;
}

void CGameControllerHunterN::GiveLoadout(CCharacter *pChr, bool Randomize, bool PreserveMantle)
{
	const int CID = pChr->GetPlayer()->GetCID();
	// Every actual loadout replacement starts a new inventory. Reusing an
	// equal-ID object would retain ability charge and could refill its magazine.
	pChr->RemoveWeapons();
	ClearAsylumProgress(CID, PreserveMantle);
	if(m_TourLobby)
	{
		for(int &Item : m_aLoadouts[CID]) Item = -1;
		return;
	}
	m_aLastCombatTick[CID] = Server()->Tick();
	m_aRegenCarry[CID] = 0;
	if(Randomize || m_aLoadouts[CID][0] < 0)
		for(int Slot = 0; Slot < 3; ++Slot)
		{
			m_aLoadouts[CID][Slot] = AsylumRandomItem(Slot);
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
	pChr->SetArmor(0);
	pChr->SetWeaponTimerType(WEAPON_TIMER_INDIVIDUAL);
	pChr->Protect((float)m_SpawnProtection, true);
	m_aLastDamageFrom[CID] = m_aLastDamageWeapon[CID] = m_aLastDamageTick[CID] = -1;
	GiveLoadout(pChr, true, true);
}

void CGameControllerHunterN::ConsumeSingleUse(CCharacter *pChr, int Slot)
{
	if(!pChr || !pChr->IsAlive() || Slot < 0 || Slot >= 4)
		return;
	const int CID = pChr->GetPlayer()->GetCID();
	if(GetPlayerIfInRoom(CID) != pChr->GetPlayer() || !pChr->GetWeapon(Slot))
		return;
	if(pChr->GetWeapon(Slot)->GetWeaponID() == AsylumWeaponID(ASYLUM_MANTLE))
	{
		const bool FirstActivation = m_aMantleLives[CID] <= 0;
		m_aMantleLives[CID] += 3;
		if(FirstActivation)
		{
			m_aMantleShield[CID] = true;
			m_aMantleReadyTick[CID] = m_aMantleCharge[CID] = m_aMantleInvulnerableUntil[CID] = 0;
		}
	}
	m_aPendingConsume[CID] |= 1 << Slot;
}

bool CGameControllerHunterN::ConsumeMantleShield(int CID)
{
	if(CID < 0 || CID >= MAX_CLIENTS || !m_aMantleShield[CID])
		return false;
	m_aMantleShield[CID] = false;
	m_aMantleInvulnerableUntil[CID] = Server()->Tick() + maximum(1, Server()->TickSpeed() * 3 / 2);
	m_aMantleReadyTick[CID] = m_aMantleInvulnerableUntil[CID] + Server()->TickSpeed() * 26;
	m_aMantleCharge[CID] = 0;
	return true;
}

bool CGameControllerHunterN::IsMantleInvulnerable(int CID) const
{
	return CID >= 0 && CID < MAX_CLIENTS && m_aMantleInvulnerableUntil[CID] > Server()->Tick();
}

void CGameControllerHunterN::ConsumeUpgradeModule(CCharacter *pChr, int Slot)
{
	if(!pChr || !pChr->IsAlive() || m_Mode == MODE_GG || Slot != 3 || pChr->GetActiveWeapon() != 3)
		return;
	const int CID = pChr->GetPlayer()->GetCID();
	if(GetPlayerIfInRoom(CID) != pChr->GetPlayer() || m_aPendingUpgrade[CID] >= 0)
		return;
	CWeapon *pModule = pChr->GetWeapon(3);
	CWeapon *pRanged = pChr->GetWeapon(1);
	if(!pModule || !pRanged)
		return;
	const int Module = m_aLoadouts[CID][3];
	const int Base = AsylumUpgradeBase(Module);
	if(Base < 0 || pRanged->GetWeaponID() != AsylumWeaponID(Base) || pModule->GetWeaponID() != AsylumWeaponID(Module))
		return;
	m_aPendingUpgrade[CID] = AsylumUpgradeResult(Base);
}

void CGameControllerHunterN::ProcessPendingItems(int CID, CCharacter *pChr)
{
	// The controller ticks before its world. Requests made by Fire in the
	// preceding world tick are now outside Fire/HandleFire/Tick's call stack,
	// so removing or replacing even the active weapon is safe here.
	const bool AppliedModuleWasActive = m_aPendingUpgrade[CID] >= 0 && pChr->GetActiveWeapon() == 3;
	if(m_aPendingUpgrade[CID] >= 0)
	{
		const int Upgrade = m_aPendingUpgrade[CID];
		m_aPendingUpgrade[CID] = -1;
		m_aLoadouts[CID][1] = Upgrade;
		pChr->ForceSetWeapon(1, AsylumWeaponID(Upgrade), -1);
		m_aPendingConsume[CID] |= 1 << 3;
		GameServer()->SendChatTarget(CID, "升级模块已消耗；槽位2已替换为升级武器。");
	}
	for(int Slot = 0; Slot < 4; ++Slot)
		if(m_aPendingConsume[CID] & (1 << Slot))
		{
			pChr->RemoveWeapon(Slot);
			m_aLoadouts[CID][Slot] = -1;
		}
	if(AppliedModuleWasActive && m_aLoadouts[CID][1] >= 0)
		pChr->SetWeaponSlot(1, false);
	m_aPendingConsume[CID] = 0;
}

void CGameControllerHunterN::AwardUpgradeKill(int KillerCID, int WeaponID)
{
	if(m_Mode == MODE_GG || KillerCID < 0 || KillerCID >= MAX_CLIENTS)
		return;
	CPlayer *pPlayer = GetPlayerIfInRoom(KillerCID);
	CCharacter *pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;
	if(!pChr || !pChr->IsAlive() || m_aLoadouts[KillerCID][3] >= 0)
		return;
	CWeapon *pRanged = pChr->GetWeapon(1);
	if(!pRanged || pRanged->GetWeaponID() != WeaponID)
		return;
	const int aBaseItems[] = {ASYLUM_M1911, ASYLUM_PIXELGUN, ASYLUM_9MM, ASYLUM_SUPPRESSED_PISTOL, ASYLUM_SSG08};
	for(int Upgrade = 0; Upgrade < 5; ++Upgrade)
	{
		const int Base = aBaseItems[Upgrade];
		if(WeaponID != AsylumWeaponID(Base))
			continue;
		const int Required = maximum(1, AsylumUpgradeKills(Base));
		if(++m_aUpgradeKills[KillerCID][Upgrade] < Required)
			return;
		m_aLoadouts[KillerCID][3] = AsylumUpgradeModule(Base);
		pChr->ForceSetWeapon(3, AsylumWeaponID(m_aLoadouts[KillerCID][3]), -1);
		GameServer()->SendChatTarget(KillerCID, "击杀达标：按4选择升级模块并左键使用，升级槽会被消耗。");
		return;
	}
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
	if(Server()->Tick() - m_aLastRerollTick[CID] < Server()->TickSpeed() * 10)
	{
		GameServer()->SendChatTarget(CID, "重抽道具共享10秒冷却；再次抽到骰子也不会重置。");
		return;
	}
	m_aLastRerollTick[CID] = Server()->Tick();
	m_aPendingReroll[CID] = Server()->Tick();
}

bool CGameControllerHunterN::CanCombatInteract(int From, int To) const
{
	if(m_TourLobby || (m_TourBoss && From >= 0 && From != To)) return false;
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
	if(m_TourLobby)
	{
		int aCounts[3] = {};
		for(int i = 0; i < MAX_CLIENTS; ++i)
			if(GetPlayerIfInRoom(i) && m_aMapVotes[i] >= 0) ++aCounts[m_aMapVotes[i]];
		char aLobby[512];
		str_format(aLobby, sizeof(aLobby), "LOBBY | 无武器 / 触碰选图板\n1: %s [%d]\n2: %s [%d]\n3: %s [%d]\n你的选择: %d | 选图结束 %ds", TourMapChoice(0), aCounts[0], TourMapChoice(1), aCounts[1], TourMapChoice(2), aCounts[2], m_aMapVotes[CID] + 1,
			m_LobbyStartTick < 0 ? m_LobbySeconds : maximum(0, m_LobbySeconds - (Server()->Tick() - m_LobbyStartTick) / Server()->TickSpeed()));
		GameServer()->SendBroadcast(aLobby, CID, false);
		return;
	}
	aState[0] = 0; // The scoreboard already shows mode and kills.
	if(m_Mode == MODE_GG)
		str_format(aState, sizeof(aState), "GG | 阶段 %d/%d%s", minimum(m_aStages[CID] + 1, NUM_PROGRESS_STAGES), NUM_PROGRESS_STAGES,
			m_aStages[CID] == NUM_PROGRESS_STAGES - 1 ? " 黄金勺" : "");
	if(LimitedLives())
		str_format(aState, sizeof(aState), "%s | %s | 剩余命数 %d", m_pGameType,
			m_Mode == MODE_ELIM ? "淘汰赛" : (CID == m_Juggernaut ? "巨人" : "挑战者"), m_aLives[CID]);
	if(m_Mode == MODE_ELIM && m_SafeZoneActive)
	{
		CCharacter *pChr = pPlayer->GetCharacter();
		str_format(aState, sizeof(aState), "ELIM | 命数 %d | 收缩安全区 %.0f | %s", m_aLives[CID], m_SafeZoneRadius,
			pChr && distance(pChr->m_Pos, m_SafeZoneCenter) > m_SafeZoneRadius ? "圈外：向中心返回" : "圈内");
	}
	if(m_Mode == MODE_ZS)
		str_format(aState, sizeof(aState), "ZS | %s", m_aInfected[CID] ? "感染者" : "生还者");
	if(!pPlayer->GetCharacter())
	{
		if(m_TourBoss)
		{
			char aBoss[160], aBuf[256]; BossStatus(aBoss, sizeof(aBoss));
			str_format(aBuf, sizeof(aBuf), "%s\n剩余命数 %d | %s", aBoss, m_aLives[CID], m_aLives[CID] > 0 ? "等待复活" : "已失踪 / 等待大厅");
			GameServer()->SendBroadcast(aBuf, CID, false);
			return;
		}
		if(m_RoundActive && (LimitedLives() || m_Mode == MODE_ZS) && (!m_aParticipants[CID] || m_aLives[CID] <= 0))
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "%s\n已淘汰 / 等待下一局", aState);
			GameServer()->SendBroadcast(aBuf, CID, false);
		}
		return;
	}
	if(Detailed)
		for(int Slot = 0; Slot < 4; ++Slot)
		{
			if(m_aLoadouts[CID][Slot] < 0)
				continue;
			const SAsylumItem &Item = AsylumItem(m_aLoadouts[CID][Slot]);
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "[%d] %s — %s", Slot + 1, Item.m_pName, Item.m_pDescription);
			GameServer()->SendChatTarget(CID, aBuf);
		}
	CCharacter *pChr = pPlayer->GetCharacter();
	CWeapon *pUtility = pChr->GetWeapon(2);
	float Cooldown = 0.0f;
	int MantleCharges = m_aMantleShield[CID] ? 1 : -1;
	if(pUtility && AsylumIsWeapon(pUtility->GetWeaponID()))
	{
		CAsylumWeapon *pItem = (CAsylumWeapon *)pUtility;
		Cooldown = pItem->CooldownTicks() / (float)Server()->TickSpeed();
		if(pItem->Item() == ASYLUM_MANTLE)
			MantleCharges = pItem->MantleCharges();
	}
	char aExtra[160];
	str_format(aExtra, sizeof(aExtra), "冷却 %.1fs", Cooldown);
	if(MantleCharges >= 0)
		str_format(aExtra, sizeof(aExtra), "护盾余量 %d | 冷却 %.1fs", MantleCharges, Cooldown);
	if(m_aMantleLives[CID] > 0)
		str_format(aExtra, sizeof(aExtra), "Mantle %s | 余%d命 | %.1fs | 充能%d/170", m_aMantleShield[CID] ? "就绪" : "恢复中",
			m_aMantleLives[CID], maximum(0, m_aMantleReadyTick[CID] - Server()->Tick()) / (float)Server()->TickSpeed(), m_aMantleCharge[CID]);
	char aSkills[192];
	str_copy(aSkills, m_Mode == MODE_GG ? "GG：附加技能禁用" : "E / R：当前装备无附加技能", sizeof(aSkills));
	CWeapon *pActive = pChr->GetActiveWeapon() >= 0 && pChr->GetActiveWeapon() < NUM_WEAPON_SLOTS ? pChr->GetWeapon(pChr->GetActiveWeapon()) : nullptr;
	if(m_Mode != MODE_GG && pActive && AsylumIsWeapon(pActive->GetWeaponID()))
		((CAsylumWeapon *)pActive)->SkillStatus(aSkills, sizeof(aSkills));
	char aAmmo[192];
	aAmmo[0] = 0;
	if(pActive && AsylumIsWeapon(pActive->GetWeaponID()))
		((CAsylumWeapon *)pActive)->AmmoStatus(aAmmo, sizeof(aAmmo));
	char aBuf[1024];
	str_format(aBuf, sizeof(aBuf), "HP %d/%d | 护甲 %d/%d", maximum(0, pChr->GetHealth()), pChr->m_MaxHealth, pChr->GetArmor(), pChr->m_MaxArmor);
	if(aState[0]) { str_append(aBuf, "\n", sizeof(aBuf)); str_append(aBuf, aState, sizeof(aBuf)); }
	if(m_TourBoss)
	{
		char aBoss[160]; BossStatus(aBoss, sizeof(aBoss));
		str_append(aBuf, "\n", sizeof(aBuf)); str_append(aBuf, aBoss, sizeof(aBuf));
		char aLives[48]; str_format(aLives, sizeof(aLives), " | 命数 %d", m_aLives[CID]); str_append(aBuf, aLives, sizeof(aBuf));
	}
	for(int Slot = 0; Slot < 4; ++Slot)
	{
		if(m_aLoadouts[CID][Slot] < 0 || !pChr->GetWeapon(Slot))
			continue;
		char aLine[192];
		str_format(aLine, sizeof(aLine), "\n[%d] %s%s%s", Slot + 1, AsylumItem(m_aLoadouts[CID][Slot]).m_pName,
			Slot == 2 ? " | " : "", Slot == 2 ? aExtra : "");
		str_append(aBuf, aLine, sizeof(aBuf));
	}
	// Vanilla DDNet has one centered broadcast, not independent HUD anchors.
	// Pad only the lower block. No consumed item or unearned module placeholders.
	if(pActive && (aAmmo[0] || aSkills[0]))
	{
		// Broadcast starts near the top on a 300-unit stock-client canvas.
		// Fix the lower block's row regardless of consumed slots/Boss rows.
		int Rows = 1;
		for(const char *p = aBuf; *p; ++p) if(*p == '\n') ++Rows;
		while(Rows++ < 19) str_append(aBuf, "\n", sizeof(aBuf));
		if(aAmmo[0]) str_append(aBuf, aAmmo, sizeof(aBuf));
		if(aSkills[0])
		{
			if(aAmmo[0]) str_append(aBuf, "\n", sizeof(aBuf));
			str_append(aBuf, aSkills, sizeof(aBuf));
		}
	}
	GameServer()->SendBroadcast(aBuf, CID, false);
}

int CGameControllerHunterN::OnCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, int Weapon)
{
	CPlayer *pPlayer = pVictim->GetPlayer();
	const int CID = pPlayer->GetCID();
	m_aPendingReroll[CID] = -1;
	if(m_TourBoss && m_BossHealth > 0 && Weapon != WEAPON_GAME)
	{
		m_aLives[CID] = maximum(0, m_aLives[CID] - 1);
		if(!m_aLives[CID]) pPlayer->CancelSpawn();
	}
	if(m_aMantleLives[CID] > 0)
	{
		--m_aMantleLives[CID];
		if(m_aMantleLives[CID] <= 0)
		{
			m_aMantleShield[CID] = false;
			m_aMantleReadyTick[CID] = m_aMantleCharge[CID] = m_aMantleInvulnerableUntil[CID] = 0;
		}
	}
	ClearAsylumProgress(CID, true);
	pPlayer->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() * m_RespawnDelay;
	if(!IsGameRunning() || !m_RoundActive || Weapon == WEAPON_GAME)
		return DEATH_NO_SUICIDE_PANATY | DEATH_SKIP_SCORE;
	const bool ValidKill = pKiller && pKiller != pPlayer && GetPlayerIfInRoom(pKiller->GetCID()) == pKiller;
	// Only a genuine last hit by this same weapon unlocks its module; world,
	// suicide, blocked, teammate and another weapon's kills never count.
	if(ValidKill && m_aLastDamageTick[CID] == Server()->Tick() && m_aLastDamageFrom[CID] == pKiller->GetCID() &&
		!IsFriendlyFire(CID, pKiller->GetCID()))
		AwardUpgradeKill(pKiller->GetCID(), m_aLastDamageWeapon[CID]);
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
	if(m_aAdminGod[CID] || pChr->IsProtected() || IsMantleInvulnerable(CID) || !CanCombatInteract(From, CID))
		return DAMAGE_SKIP;
	if(m_Mode == MODE_GG && From >= 0 && From < MAX_CLIENTS && From != CID)
	{
		// A single melee swing or volley cannot finish two consecutive stages.
		if(m_aPendingReroll[From] >= 0 || WeaponID != AsylumWeaponID(ProgressItem(From)))
			return DAMAGE_SKIP;
		if(m_aStages[From] == NUM_PROGRESS_STAGES - 1)
			Dmg = maximum(Dmg, 100);
	}
	if(Dmg > 0)
	{
		if((From >= 0 || (m_TourBoss && From == -2)) && From != CID && ConsumeMantleShield(CID))
		{
			GameWorld()->CreatePlayerSpawn(pChr->m_Pos);
			GameWorld()->CreateSound(pChr->m_Pos, SOUND_PICKUP_ARMOR);
			GameServer()->SendChatTarget(CID, "Holy Mantle 抵挡了一次伤害。");
			return DAMAGE_SKIP;
		}
		CWeapon *pActive = pChr->GetActiveWeapon() >= 0 && pChr->GetActiveWeapon() < NUM_WEAPON_SLOTS ? pChr->GetWeapon(pChr->GetActiveWeapon()) : nullptr;
		if(pActive && AsylumIsWeapon(pActive->GetWeaponID()) && ((CAsylumWeapon *)pActive)->HandleIncomingDamage(Force, Dmg, From))
			return DAMAGE_SKIP;
		// One shared path for direct hits, projectiles, melee, AoE and skills.
		// Defense adds in percentage points (IA caps it at 99%), attack
		// bonuses multiply. Round once, after both modifiers are applied.
		float Attack = m_Mode == MODE_JGN && From == m_Juggernaut && From != CID ? 1.5f : 1.0f;
		CCharacter *pOwner = From >= 0 ? GameServer()->GetPlayerChar(From) : nullptr;
		if(pOwner && pOwner->GameWorld() == GameWorld() && pOwner->CurrentWeapon())
			Attack *= maximum(0.0f, pOwner->CurrentWeapon()->AttackMultiplier());
		const float Defense = pActive ? minimum(99.0f, pActive->DefensePercent()) : 0.0f;
		Dmg = Attack <= 0.0f ? 0 : maximum(1, round_to_int(Dmg * Attack * (1.0f - Defense / 100.0f)));
		m_aLastDamageWeapon[CID] = WeaponID;
		m_aLastDamageFrom[CID] = From;
		m_aLastDamageTick[CID] = Server()->Tick();
	}
	return m_Mode == MODE_JGN && CID == m_Juggernaut ? DAMAGE_NO_KNOCKBACK : DAMAGE_NORMAL;
}

void CGameControllerHunterN::OnCharacterDamageApplied(CCharacter *pChr, int From, int WeaponID, int HealthLoss, int ArmorLoss)
{
	if(!pChr)
		return;
	m_aLastCombatTick[pChr->GetPlayer()->GetCID()] = Server()->Tick();
	if(From >= 0 && From < MAX_CLIENTS)
		m_aLastCombatTick[From] = Server()->Tick();
	if(From < 0 || From >= MAX_CLIENTS || !AsylumIsWeapon(WeaponID))
		return;
	const int CID = pChr->GetPlayer()->GetCID();
	if(From == CID || GetPlayerIfInRoom(CID) != pChr->GetPlayer())
		return;
	CPlayer *pAttacker = GetPlayerIfInRoom(From);
	CCharacter *pOwner = pAttacker ? pAttacker->GetCharacter() : nullptr;
	const int AppliedDamage = maximum(0, HealthLoss) + maximum(0, ArmorLoss);
	if(!pOwner || !pOwner->IsAlive() || AppliedDamage <= 0)
		return;
	if(m_aMantleLives[From] > 0 && !m_aMantleShield[From] && Server()->Tick() >= m_aMantleInvulnerableUntil[From])
	{
		m_aMantleCharge[From] = minimum(170, m_aMantleCharge[From] + AppliedDamage);
		if(m_aMantleCharge[From] >= 170)
		{
			m_aMantleShield[From] = true;
			m_aMantleReadyTick[From] = m_aMantleCharge[From] = 0;
			GameServer()->SendChatTarget(From, "Holy Mantle：造成170实际伤害，护盾已恢复。");
		}
	}
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
	if(!pChr || !pChr->IsAlive() || pChr->IsFrozen() || pChr->IsDisabled())
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
	if(m_Mode == MODE_TOUR && !GameWorld()->m_Paused) TickTour();
	// The room controller ticks before the world even while paused. Maintain
	// only the new absolute-tick timers so pause time cannot regenerate a
	// shield or satisfy out-of-combat healing.
	if(GameWorld()->m_Paused)
	{
		if(m_Mode == MODE_TOUR)
		{
			if(m_LobbyStartTick >= 0) ++m_LobbyStartTick;
			for(int *pTimer : {&m_BossDrinkUntil, &m_BossNextTeleport, &m_BossNextAttack,
				&m_BossStunUntil, &m_BossDashUntil, &m_BossShadowUntil, &m_BossGrabUntil})
				if(*pTimer > 0) ++*pTimer;
		}
		for(int CID = 0; CID < MAX_CLIENTS; ++CID)
		{
			if(m_aMantleReadyTick[CID] > 0) ++m_aMantleReadyTick[CID];
			if(m_aMantleInvulnerableUntil[CID] > 0) ++m_aMantleInvulnerableUntil[CID];
			++m_aLastCombatTick[CID];
		}
		return;
	}
	if(!IsGameRunning() && !IsWarmup())
		return;
	if(IsGameRunning() && m_SafeZoneActive && m_RoundActive && Server()->Tick() >= m_NextZoneDamageTick)
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
		CCharacter *pChr = pPlayer->GetCharacter();
		if(pChr)
		{
			if(m_aMantleLives[CID] > 0 && !m_aMantleShield[CID] && m_aMantleReadyTick[CID] > 0 && Server()->Tick() >= m_aMantleReadyTick[CID])
			{
				m_aMantleShield[CID] = true;
				m_aMantleReadyTick[CID] = m_aMantleCharge[CID] = 0;
				GameServer()->SendChatTarget(CID, "Holy Mantle：护盾已恢复。");
			}
			ProcessPendingItems(CID, pChr);
			if(m_aPendingReroll[CID] >= 0 && m_aPendingReroll[CID] < Server()->Tick())
				GiveLoadout(pChr, true);
			CWeapon *pActive = pChr->GetActiveWeapon() >= 0 && pChr->GetActiveWeapon() < NUM_WEAPON_SLOTS ? pChr->GetWeapon(pChr->GetActiveWeapon()) : nullptr;
			if(pActive && AsylumIsWeapon(pActive->GetWeaponID()) && ((CAsylumWeapon *)pActive)->Item() == ASYLUM_PARASOL && pChr->Core()->m_Vel.y > 2.0f)
				pChr->Core()->m_Vel.y = 2.0f;
			// Item Asylum-style regeneration: after five seconds out of combat,
			// recover 2.5% of maximum health each second (fractional points are
			// carried between ticks, so 100 HP alternates 2 and 3 points).
			if(m_Mode != MODE_GG && pChr->GetHealth() > 0 && pChr->GetHealth() < pChr->m_MaxHealth &&
				Server()->Tick() - m_aLastCombatTick[CID] >= Server()->TickSpeed() * 5 &&
				Server()->Tick() % maximum(1, Server()->TickSpeed()) == 0)
			{
				m_aRegenCarry[CID] += pChr->m_MaxHealth * 25;
				const int Heal = m_aRegenCarry[CID] / 1000;
				m_aRegenCarry[CID] %= 1000;
				if(Heal > 0)
					pChr->IncreaseHealth(Heal);
			}
		}
		if(!pChr && !pPlayer->m_RespawnDisabled && Server()->Tick() >= pPlayer->m_RespawnTick)
			pPlayer->Respawn();
		if(Server()->Tick() % maximum(1, Server()->TickSpeed() / 2) == 0)
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
	if(m_Mode == MODE_TOUR)
	{
		const char *apLobbies[] = {"asylum_lobby_propaganda", "asylum_lobby_atrium", "asylum_lobby_arcade"};
		ChangeTourMap(apLobbies[secure_rand_below(3)]);
		return;
	}
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
	if(!m_RoundActive || !IsGameRunning())
		return;
	if(m_Mode == MODE_TOUR && (m_TourLobby || m_TourBoss || m_TourChangingMap)) return;
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
		else if(m_Mode == MODE_TOUR)
			FinishPlayer(-1, "时间到，本局平局。");
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
