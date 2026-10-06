#include "huntern.h"

#include <game/server/entities/character.h>
#include <game/server/entities/gojo.h>
#include <game/server/player.h>
#include <game/server/weapons.h>

CGojoState *CGameControllerHunterN::GetGojoState(int CID)
{
	return CID >= 0 && CID < MAX_CLIENTS && GetPlayerIfInRoom(CID) ? &m_aGojo[CID] : nullptr;
}

const CGojoState *CGameControllerHunterN::GetGojoState(int CID) const
{
	return CID >= 0 && CID < MAX_CLIENTS && GetPlayerIfInRoom(CID) ? &m_aGojo[CID] : nullptr;
}

void CGameControllerHunterN::ConGojo(IConsole::IResult *pResult, void *pUserData)
{
	auto *pSelf = static_cast<CGameControllerHunterN *>(pUserData);
	if(!pSelf->RequireAdmin(pResult)) return;
	const int CID = pResult->GetInteger(0), Enabled = pResult->GetInteger(1);
	CPlayer *pPlayer = CID >= 0 && CID < MAX_CLIENTS ? pSelf->GetPlayerIfInRoom(CID) : nullptr;
	if(!pPlayer || (Enabled != 0 && Enabled != 1))
	{
		pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "gojo", "Need a connected player in this room; enabled must be 0 or 1");
		return;
	}
	if(Enabled && (pSelf->m_TourLobby || pSelf->m_Mode == MODE_GG ||
		(pSelf->m_Mode == MODE_ZS && pSelf->m_aInfected[CID]) || (pSelf->m_Mode == MODE_JGN && CID == pSelf->m_Juggernaut)))
	{
		pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "gojo", "Gojo needs an armed map and a non-fixed role; use FFA or a combat Tour map");
		return;
	}
	CGojoState &State = pSelf->m_aGojo[CID];
	if(State.m_Enabled == (Enabled != 0))
	{
		pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "gojo", "Identity already has that state; cooldowns were not reset");
		return;
	}
	GojoClearEntities(pSelf->GameWorld(), CID);
	State.m_Enabled = Enabled != 0;
	State.m_LastActionTick = State.m_LastHealTick = pSelf->Server()->Tick();
	State.m_Infinity = 100;
	for(int &Item : pSelf->m_aLoadouts[CID]) Item = -1;
	pSelf->ClearAsylumProgress(CID);
	CCharacter *pChr = pPlayer->GetCharacter();
	if(pChr && pChr->IsAlive()) pSelf->GiveLoadout(pChr, true);
	pSelf->GameServer()->SendChatTarget(CID, Enabled ? "已变身五条悟：1苍拳 / 2苍 / 3赫 / 4茈 / 5无量空处。按住蓄力，松开释放。" : "已退出五条悟身份，恢复普通装备。");
	if(Enabled)
	{
		char aAnnouncement[256];
		str_format(aAnnouncement, sizeof(aAnnouncement), "那双眼睛，毋庸置疑——%s 变成了五条悟。", pSelf->Server()->ClientName(CID));
		// Explicitly server-wide, not only the player's current Asylum room.
		pSelf->GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aAnnouncement);
	}
	char aBuf[96]; str_format(aBuf, sizeof(aBuf), "gojo cid=%d enabled=%d", CID, Enabled);
	pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "gojo", aBuf);
}

void CGameControllerHunterN::ConGojoStatus(IConsole::IResult *pResult, void *pUserData)
{
	auto *pSelf = static_cast<CGameControllerHunterN *>(pUserData);
	if(!pSelf->RequireAdmin(pResult)) return;
	const int CID = pResult->GetInteger(0);
	CGojoState *pState = pSelf->GetGojoState(CID);
	if(!pState) { pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "gojo", "Need a connected player in this room"); return; }
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "gojo cid=%d enabled=%d inf_resources=%d energy=%d/%d infinity=%d domain_ticks=%d brain_ticks=%d carry_ticks=%d cd=%d,%d,%d,%d,%d", CID,
		pState->m_Enabled, pSelf->GetPlayerIfInRoom(CID)->m_AsylumInfCursedEnergy, round_to_int(pState->m_CursedEnergy), GOJO_CURSED_ENERGY_MAX, round_to_int(pState->m_Infinity), maximum(0, pState->m_DomainUntil - pSelf->Server()->Tick()),
		maximum(0, pState->m_BrainUntil - pSelf->Server()->Tick()), maximum(0, pState->m_CarryUntil - pSelf->Server()->Tick()),
		maximum(0, pState->m_NextCast[0] - pSelf->Server()->Tick()), maximum(0, pState->m_NextCast[1] - pSelf->Server()->Tick()),
		maximum(0, pState->m_NextCast[2] - pSelf->Server()->Tick()), maximum(0, pState->m_NextCast[3] - pSelf->Server()->Tick()),
		maximum(0, pState->m_NextCast[4] - pSelf->Server()->Tick()));
	pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "gojo", aBuf);
}

void CGameControllerHunterN::GiveGojoLoadout(CCharacter *pChr)
{
	const int CID = pChr->GetPlayer()->GetCID();
	for(int &Item : m_aLoadouts[CID]) Item = -1;
	for(int Slot = 0; Slot < NUM_GOJO_SKILLS; ++Slot) pChr->GiveWeapon(Slot, GojoWeaponID(Slot), -1);
	pChr->SetWeaponSlot(0, false);
	m_aPendingReroll[CID] = -1;
	SendGojoLoadout(CID, true);
}

void CGameControllerHunterN::ConGojoEnergy(IConsole::IResult *pResult, void *pUserData)
{
	auto *pSelf = static_cast<CGameControllerHunterN *>(pUserData);
	if(!pSelf->RequireAdmin(pResult)) return;
	CGojoState *pState = pSelf->GetGojoState(pResult->GetInteger(0));
	const int Amount = pResult->GetInteger(1);
	if(!pState || !pState->m_Enabled || Amount < 0 || Amount > GOJO_CURSED_ENERGY_MAX)
	{
		pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "gojo", "Need an enabled Gojo and energy between 0 and 200");
		return;
	}
	pState->m_CursedEnergy = (float)Amount;
	pSelf->SendGojoLoadout(pResult->GetInteger(0), false);
}

void CGameControllerHunterN::ConInfCursedEnergy(IConsole::IResult *pResult, void *pUserData)
{
	auto *pSelf = static_cast<CGameControllerHunterN *>(pUserData);
	if(!pSelf->RequireAdmin(pResult)) return;
	const int CID = pResult->GetInteger(0), Enabled = pResult->GetInteger(1);
	CPlayer *pPlayer = CID >= 0 && CID < MAX_CLIENTS ? pSelf->GetPlayerIfInRoom(CID) : nullptr;
	if(!pPlayer || (Enabled != 0 && Enabled != 1))
	{
		pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "gojo", "Need a connected player; enabled must be 0 or 1");
		return;
	}
	pPlayer->m_AsylumInfCursedEnergy = Enabled != 0;
	if(Enabled)
	{
		pSelf->m_aGojo[CID].m_CursedEnergy = GOJO_CURSED_ENERGY_MAX;
		pSelf->m_aGojo[CID].m_Infinity = 100;
	}
	char aBuf[160]; str_format(aBuf, sizeof(aBuf), "inf_cursedenergy cid=%d enabled=%d (cursed energy + Infinity)", CID, Enabled);
	pSelf->InstanceConsole()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "gojo", aBuf);
	pSelf->GameServer()->SendChatTarget(CID, Enabled ? "[测试] 无限咒力和无限“无限”已开启；冷却、最低蓄力和状态限制仍保留。" : "[测试] 已恢复正常咒力和无限屏障消耗。");
	if(pSelf->m_aGojo[CID].m_Enabled) pSelf->SendGojoLoadout(CID, false);
}

void CGameControllerHunterN::SendGojoLoadout(int CID, bool Detailed)
{
	CPlayer *pPlayer = GetPlayerIfInRoom(CID);
	if(!pPlayer) return;
	CCharacter *pChr = pPlayer->GetCharacter();
	if(!pChr) return;
	char aHeld[128] = "";
	CWeapon *pWeapon = pChr->CurrentWeapon();
	if(pWeapon && GojoIsWeapon(pWeapon->GetWeaponID()))
	{
		auto *pSkill = static_cast<CGojoWeapon *>(pWeapon);
		const int Skill = pSkill->Skill();
		const float Cooldown = pPlayer->m_AsylumNoCooldown ? 0 : maximum(0, m_aGojo[CID].m_NextCast[Skill] - Server()->Tick()) / (float)Server()->TickSpeed();
		if(pSkill->Charging()) str_format(aHeld, sizeof(aHeld), "%s | 蓄力 %d%%%s", GojoSkillName(Skill), round_to_int(pSkill->Charge() * 100),
			!pSkill->ReadyToRelease() ? " | 未达到最低蓄力，点按不释放" : Skill == GOJO_BLUE ? (pSkill->Charge() >= 0.99f ? " | 最大输出苍就绪" : " | 松开发出普通苍") : " | 松开可释放");
		else str_format(aHeld, sizeof(aHeld), "%s | 冷却 %.1fs%s", GojoSkillName(Skill), Cooldown,
			Skill == GOJO_BLUE && GojoHasBlue(GameWorld(), CID) ? " | 按住左键引导苍" : "");
	}
	char aEnergyBar[96] = "";
	const int Filled = clamp(round_to_int(m_aGojo[CID].m_CursedEnergy * 20 / GOJO_CURSED_ENERGY_MAX), 0, 20);
	for(int i = 0; i < 20; ++i) str_append(aEnergyBar, i < Filled ? "█" : "░", sizeof(aEnergyBar));
	char aBuf[640];
	str_format(aBuf, sizeof(aBuf), "五条悟 | HP %d/%d | 无限 %d/100%s\n咒力 [%s] %d/%d\n[1]苍拳 [2]苍 [3]赫 [4]茈 [5]无量空处\n%s%s", pChr->GetHealth(), pChr->m_MaxHealth,
		round_to_int(m_aGojo[CID].m_Infinity), pPlayer->m_AsylumNoCooldown ? " | 无CD" : "",
		aEnergyBar, round_to_int(m_aGojo[CID].m_CursedEnergy), GOJO_CURSED_ENERGY_MAX,
		aHeld,
		m_aGojo[CID].BrainDamaged(Server()->Tick()) ? " | 脑损伤：移速降低、咏唱变慢" : "");
	if(pPlayer->m_AsylumInfCursedEnergy) str_append(aBuf, "\n[测试] 无限咒力 / 无限无限", sizeof(aBuf));
	GameServer()->SendBroadcast(aBuf, CID, false);
	if(Detailed) GameServer()->SendChatTarget(CID, "五条悟技能只属于该身份，不在任何随机装备池；按1–5切换，技能2–4按住蓄力后松开。");
}

void CGameControllerHunterN::TickGojo(int CID, bool Advance)
{
	CGojoState *pState = GetGojoState(CID);
	if(!pState) return;
	if(!Advance) { pState->Pause(); return; }
	CPlayer *pPlayer = GetPlayerIfInRoom(CID);
	CCharacter *pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;
	if(pPlayer && pPlayer->m_AsylumInfCursedEnergy)
	{
		pState->m_CursedEnergy = GOJO_CURSED_ENERGY_MAX;
		pState->m_Infinity = 100;
	}
	if(!pChr || !pState->m_Enabled || pState->Immobilized(Server()->Tick())) return;
	if(Server()->Tick() - pState->m_LastEnergySpendTick >= Server()->TickSpeed() * 2)
		pState->m_CursedEnergy = minimum((float)GOJO_CURSED_ENERGY_MAX, pState->m_CursedEnergy + 6.0f / Server()->TickSpeed());
	if(Server()->Tick() - pState->m_LastActionTick >= Server()->TickSpeed() * 3)
		pState->m_Infinity = minimum(100.0f, pState->m_Infinity + 5.0f / Server()->TickSpeed());
	if(Server()->Tick() - pState->m_LastActionTick >= Server()->TickSpeed() * 4 && Server()->Tick() - pState->m_LastHealTick >= Server()->TickSpeed())
	{
		pState->m_LastHealTick = Server()->Tick();
		pChr->IncreaseHealth(2);
	}
}

bool CGameControllerHunterN::GojoInfinityBlocks(CCharacter *pChr, int From, int WeaponID, int Damage)
{
	const int CID = pChr->GetPlayer()->GetCID();
	CGojoState &State = m_aGojo[CID];
	if(!State.m_Enabled || pChr->GetPlayer()->m_AsylumTestGod || Damage <= 0 || From == CID || From == -1 || WeaponID == GojoWeaponID(GOJO_DOMAIN) ||
		State.Immobilized(Server()->Tick()) || Server()->Tick() - State.m_LastActionTick < Server()->TickSpeed() * 2) return false;
	if(State.m_InfinityBlockUntil > Server()->Tick()) return true;
	const float Cost = clamp(Damage * 0.5f, 15.0f, 40.0f);
	if(!pChr->GetPlayer()->m_AsylumInfCursedEnergy && State.m_Infinity < Cost) return false;
	if(!pChr->GetPlayer()->m_AsylumInfCursedEnergy) State.m_Infinity -= Cost;
	else State.m_Infinity = 100;
	State.m_InfinityBlockUntil = Server()->Tick() + maximum(1, Server()->TickSpeed() / 10);
	GameWorld()->CreatePlayerSpawn(pChr->m_Pos);
	GameWorld()->CreateSound(pChr->m_Pos, SOUND_LASER_BOUNCE);
	return true;
}
