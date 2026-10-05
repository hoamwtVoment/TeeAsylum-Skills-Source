/* IA reference mechanics with explicit 2D adaptations; no client modification. */
#include "huntern.h"
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/generated/protocol7.h>
#include <game/version.h>
#include <game/server/entities/character.h>
#include <game/server/entities/asylum_visual_laser.h>
#include <game/server/player.h>
#include <game/server/weapons.h>

namespace
{
const char *gs_apMaps[] = {"asylum_rooftops", "asylum_crossroads", "asylum_10hourburstman",
	"huntern_msc", "mega_std_collection", "ctf3", "ctf7", "dm1", "dm9",
	"hunter1", "hunter2", "hunter3", "hunter4", "hunter5", "hunter6", "hunter8"};
constexpr int NUM_TOUR_MAPS = sizeof(gs_apMaps) / sizeof(gs_apMaps[0]);
constexpr float STUD = 32.0f / 1.875f;
}

const char *CGameControllerHunterN::TourMapChoice(int Choice) const
{
	return gs_apMaps[m_aMapChoices[clamp(Choice, 0, 2)]];
}

CGameControllerHunterN::~CGameControllerHunterN()
{
	ClearBossShots();
	ClearBossTee();
	for(int ID : m_aBossVisualIDs) if(ID >= 0) Server()->SnapFreeID(ID);
}

void CGameControllerHunterN::ClearBossTee()
{
	for(int CID = 0; CID < MAX_CLIENTS; ++CID)
	{
		const int Identity = m_aBossSnapCID[CID];
		if(Identity >= 0 && Server()->ClientIngame(CID) && Server()->IsSixup(CID) && !GameServer()->m_apPlayers[Identity])
		{
			protocol7::CNetMsg_Sv_ClientDrop Drop = {};
			Drop.m_ClientID = Identity; Drop.m_pReason = ""; Drop.m_Silent = true;
			Server()->SendPackMsg(&Drop, MSGFLAG_VITAL | MSGFLAG_NORECORD, CID);
		}
		m_aBossSnapCID[CID] = -1;
	}
}

void CGameControllerHunterN::ClearBossShots()
{
	for(const auto &Shot : m_BossShots) Server()->SnapFreeID(Shot.m_ID);
	m_BossShots.clear();
}

void CGameControllerHunterN::ResetTour()
{
	ClearBossShots();
	ClearBossTee();
	m_TourLobby = str_startswith(g_Config.m_SvMap, "asylum_lobby_");
	m_TourBoss = str_comp(g_Config.m_SvMap, "asylum_10hourburstman") == 0;
	m_TourChangingMap = false;
	m_LobbyStartTick = -1;
	if(m_TourLobby)
	{
		int Pool[NUM_TOUR_MAPS];
		for(int i = 0; i < NUM_TOUR_MAPS; ++i) Pool[i] = i;
		for(int i = 0; i < 3; ++i)
		{
			int Pick = i + secure_rand_below(NUM_TOUR_MAPS - i);
			int Tmp = Pool[i]; Pool[i] = Pool[Pick]; Pool[Pick] = Tmp;
			m_aMapChoices[i] = Pool[i];
		}
	}
	for(int &Vote : m_aMapVotes) Vote = -1;
	m_BossHealth = m_BossMaxHealth = 0;
	m_BossPhase = 1;
	m_BossDrinkUntil = m_BossNextTeleport = m_BossNextAttack = m_BossStunUntil = m_BossDashUntil = m_BossShadowUntil = 0;
	m_BossAttack = m_BossPreviousAttack = -1;
	m_BossAttackRepeat = 0;
	m_BossGrabCID = -1; m_BossGrabUntil = m_BossLastFireTick = 0;
	m_BossWindingUp = false;
	m_BossPos = vec2(BOSS_TRIGGER_X + 960, 1760);
	m_BossVel = vec2(0, 0);
	if(m_TourBoss)
		for(int &ID : m_aBossVisualIDs) if(ID < 0) ID = Server()->SnapNewID();
}

void CGameControllerHunterN::ChangeTourMap(const char *pMap)
{
	// A physical map belongs to the server, not to an independent room.
	// Only the deliberately selected single-world Tour controller can switch it.
	if(m_Mode != MODE_TOUR || GameWorld()->Team() != 0 || m_TourChangingMap) return;
	m_TourChangingMap = true;
	Server()->ChangeMap(pMap);
}

void CGameControllerHunterN::TickTour()
{
	if(m_TourChangingMap || GameWorld()->Team() != 0) return;
	if(m_TourLobby)
	{
		bool HasPlayer = false;
		for(int CID = 0; CID < MAX_CLIENTS; ++CID)
		{
			CPlayer *pPlayer = GetPlayerIfInRoom(CID);
			CCharacter *pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;
			if(!pChr || pPlayer->GetTeam() == TEAM_SPECTATORS) continue;
			HasPlayer = true;
			// Actual world-space contact with each board, not a chat command.
			for(int Choice = 0; Choice < 3; ++Choice)
			{
				const vec2 Board(32.0f * (22 + Choice * 20), 32.0f * 32);
				if(fabs(pChr->m_Pos.x - Board.x) <= 70 && fabs(pChr->m_Pos.y - Board.y) <= 100 && m_aMapVotes[CID] != Choice)
				{
					m_aMapVotes[CID] = Choice;
					char aBuf[160]; str_format(aBuf, sizeof(aBuf), "选图已登记：%s；触碰另一块板子可以更改。", TourMapChoice(Choice));
					GameServer()->SendChatTarget(CID, aBuf);
					GameWorld()->CreateSound(pChr->m_Pos, SOUND_PICKUP_ARMOR);
				}
			}
		}
		if(!HasPlayer) { m_LobbyStartTick = -1; return; }
		if(m_LobbyStartTick < 0) m_LobbyStartTick = Server()->Tick();
		if(Server()->Tick() - m_LobbyStartTick < m_LobbySeconds * Server()->TickSpeed()) return;
		int aVotes[3] = {};
		for(int CID = 0; CID < MAX_CLIENTS; ++CID)
			if(GetPlayerIfInRoom(CID) && GetPlayerIfInRoom(CID)->GetTeam() != TEAM_SPECTATORS && m_aMapVotes[CID] >= 0)
				++aVotes[m_aMapVotes[CID]];
		int Best = maximum(aVotes[0], maximum(aVotes[1], aVotes[2]));
		int aTies[3], Count = 0;
		for(int i = 0; i < 3; ++i) if(aVotes[i] == Best) aTies[Count++] = i;
		ChangeTourMap(TourMapChoice(aTies[secure_rand_below(Count)]));
		return;
	}
	if(m_TourBoss) TickBoss();
}

bool CGameControllerHunterN::IntersectCombatNpc(vec2 From, vec2 To, float Radius, vec2 *pHit)
{
	if(!m_TourBoss || !m_RoundActive || m_BossHealth <= 0 || m_TourChangingMap) return false;
	vec2 Closest = From;
	closest_point_on_line(From, To, m_BossPos, Closest);
	if(distance(Closest, m_BossPos) > 28 + Radius) return false;
	if(GameServer()->Collision()->IntersectLine(From, Closest, nullptr, nullptr)) return false;
	if(pHit) *pHit = Closest;
	return true;
}

int CGameControllerHunterN::CombatNpcWeaponDamage(int From, int WeaponID)
{
	if(!AsylumIsWeapon(WeaponID)) return 0;
	int Item = WeaponID - AsylumWeaponID(0);
	if(Item < 0 || Item >= NUM_ASYLUM_ITEMS) return 0;
	CPlayer *pPlayer = GetPlayerIfInRoom(From);
	CCharacter *pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;
	float Range = pChr ? distance(pChr->m_Pos, m_BossPos) : 0;
	if(Item == ASYLUM_M1911) return round_to_int(24 - clamp((Range - STUD * 30) / (STUD * 40), 0.0f, 1.0f) * 14);
	if(Item == ASYLUM_PIXELGUN) return round_to_int(12 - clamp((Range - STUD * 60) / (STUD * 40), 0.0f, 1.0f) * 4);
	if(Item == ASYLUM_M1911_UPG) return round_to_int(14 - clamp((Range - STUD * 10) / (STUD * 20), 0.0f, 1.0f) * 4.7f);
	if(Item == ASYLUM_PIXELGUN_UPG) return round_to_int(14 - clamp((Range - STUD * 80) / (STUD * 40), 0.0f, 1.0f) * 4.5f);
	if(Item == ASYLUM_CROSSBOW) return round_to_int(82.3f + clamp((Range - STUD * 120) / (STUD * 40), 0.0f, 1.0f) * 67.7f);
	return AsylumRangedDamage(Item, Range);
}

int CGameControllerHunterN::DamageCombatNpc(int From, int WeaponID, int Damage)
{
	CPlayer *pPlayer = GetPlayerIfInRoom(From);
	CCharacter *pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;
	if(!m_TourBoss || !m_RoundActive || m_BossHealth <= 0 || Damage <= 0 || !pChr || !m_aParticipants[From] || m_TourChangingMap || m_BossDrinkUntil > 0) return 0;
	float Attack = pChr->CurrentWeapon() ? pChr->CurrentWeapon()->AttackMultiplier() : 1.0f;
	const int Applied = minimum(m_BossHealth, maximum(0, round_to_int(Damage * Attack * (m_BossPhase == 2 ? 2 : 1))));
	m_BossHealth -= Applied;
	m_aLastCombatTick[From] = Server()->Tick();
	if(m_aMantleLives[From] > 0 && !m_aMantleShield[From] && Server()->Tick() >= m_aMantleInvulnerableUntil[From])
	{
		m_aMantleCharge[From] = minimum(170, m_aMantleCharge[From] + Applied);
		if(m_aMantleCharge[From] >= 170)
		{
			m_aMantleShield[From] = true;
			m_aMantleReadyTick[From] = m_aMantleCharge[From] = 0;
		}
	}
	for(int Slot = 0; Slot < NUM_WEAPON_SLOTS; ++Slot)
	{
		CWeapon *pWeapon = pChr->GetWeapon(Slot);
		if(pWeapon && pWeapon->GetWeaponID() == WeaponID && AsylumIsWeapon(WeaponID))
		{
			((CAsylumWeapon *)pWeapon)->AddCharge(Applied);
			if(WeaponID == AsylumWeaponID(ASYLUM_DARKHEART)) pChr->IncreaseHealth(Applied / 2);
			if(WeaponID == AsylumWeaponID(ASYLUM_VAMPIREKNIVES)) pChr->IncreaseHealth(Applied / 2);
			break;
		}
	}
	if(Applied) GameWorld()->CreateSound(pChr->m_Pos, SOUND_HIT, CmaskOne(From));
	// Evaluate on the hit, so a later pellet in the same tick cannot skip phase 2.
	if(m_BossPhase == 1 && m_BossHealth <= m_BossMaxHealth / 2)
	{
		m_BossHealth = maximum(1, m_BossHealth);
		m_BossDrinkUntil = Server()->Tick() + Server()->TickSpeed() * 3;
		m_BossDashUntil = 0;
		if(m_BossGrabCID >= 0)
		{
			CCharacter *pGrabbed = GameServer()->GetPlayerChar(m_BossGrabCID);
			if(pGrabbed) pGrabbed->UnFreeze();
		}
		m_BossGrabCID = -1; m_BossGrabUntil = 0;
		ClearBossShots();
		SendChatTarget(-1, "10 Hour Burst Man 正在饮用10 hour burst！进入二阶段后恢复全部生命，承受双倍伤害。");
	}
	else if(m_BossHealth <= 0)
		FinishPlayer(-1, "挑战者获胜！10 Hour Burst Man 已被击败。");
	return Applied;
}

int CGameControllerHunterN::DamageCombatNpcBox(int From, int WeaponID, int Damage, vec2 Origin, vec2 Aim, float Reach, float HalfWidth)
{
	const vec2 Delta = m_BossPos - Origin;
	const float Along = dot(Delta, Aim), Across = fabs(dot(Delta, vec2(-Aim.y, Aim.x)));
	if(Along < -28 || Along > Reach + 28 || Across > HalfWidth + 28) return 0;
	return DamageCombatNpc(From, WeaponID, Damage); // This ability explicitly pierces walls.
}

void CGameControllerHunterN::ExplosionCombatNpc(vec2 Pos, int From, int WeaponID, int Damage)
{
	const float Range = distance(Pos, m_BossPos);
	if(Range < 135 && !GameServer()->Collision()->IntersectLine(Pos, m_BossPos, nullptr, nullptr))
		DamageCombatNpc(From, WeaponID, round_to_int(Damage * (1 - clamp((Range - 48) / 87, 0.0f, 1.0f))));
}

bool CGameControllerHunterN::BossHitPlayer(CCharacter *pChr, int Damage, vec2 Force, float Freeze, bool Lifesteal)
{
	if(!pChr || !pChr->IsAlive()) return false;
	const int Before = maximum(0, pChr->GetHealth()) + pChr->GetArmor();
	pChr->TakeDamage(Force, Damage, -2, WEAPON_HAMMER, WEAPON_ID_WORLD, false);
	const int Lost = Before - maximum(0, pChr->GetHealth()) - pChr->GetArmor();
	if(Lost > 0 && Freeze > 0 && pChr->IsAlive())
	{
		CWeapon *pWeapon = pChr->CurrentWeapon();
		if(!pWeapon || !AsylumIsWeapon(pWeapon->GetWeaponID()) || !((CAsylumWeapon *)pWeapon)->RagdollImmune()) pChr->Freeze(Freeze, true);
	}
	if(Lost > 0 && Lifesteal) m_BossHealth = minimum(m_BossMaxHealth, m_BossHealth + Lost);
	return Lost > 0;
}

void CGameControllerHunterN::SelectBossAttack()
{
	const int Choices = m_BossPhase == 1 ? 5 : 7;
	int Next;
	do { Next = secure_rand_below(Choices); } while(Next == m_BossPreviousAttack && m_BossAttackRepeat >= 2);
	m_BossAttackRepeat = Next == m_BossPreviousAttack ? m_BossAttackRepeat + 1 : 1;
	m_BossPreviousAttack = m_BossAttack = Next;
	m_BossAttackUses = 0;
	m_BossWindingUp = false;
	m_BossNextAttack = Server()->Tick() + Server()->TickSpeed();
	// Attack cues are animation/sound only, not global chat announcements.
	if(Next != 1 && Next != 6) // Wiki: Twilight and Strong Left have no equip cue.
		GameWorld()->CreateSound(m_BossPos, Next == 3 ? SOUND_NINJA_FIRE : SOUND_WEAPON_SWITCH);
}

void CGameControllerHunterN::TickBoss()
{
	if(!m_RoundActive) return;
	CCharacter *pTarget = nullptr;
	float Best = 1e20f;
	int Count = 0, Remaining = 0;
	bool Trigger = false;
	for(int CID = 0; CID < MAX_CLIENTS; ++CID)
	{
		CPlayer *pPlayer = GetPlayerIfInRoom(CID);
		if(!pPlayer || pPlayer->GetTeam() == TEAM_SPECTATORS) continue;
		CCharacter *pChr = pPlayer->GetCharacter();
		++Count;
		if(m_aParticipants[CID] && m_aLives[CID] > 0) ++Remaining;
		if(!pChr || !m_aParticipants[CID]) continue;
		Trigger |= pChr->m_Pos.x >= BOSS_TRIGGER_X;
		float D = distance(pChr->m_Pos, m_BossPos);
		if(D < Best) { Best = D; pTarget = pChr; }
	}
	if(!m_BossMaxHealth)
	{
		if(!Trigger) return;
		m_BossHealth = m_BossMaxHealth = minimum(11990, 2000 + maximum(0, Count - 1) * 666);
		for(int CID = 0; CID < MAX_CLIENTS; ++CID)
			if(GetPlayerIfInRoom(CID) && GetPlayerIfInRoom(CID)->GetTeam() != TEAM_SPECTATORS) { m_aParticipants[CID] = true; m_aLives[CID] = 6; }
		m_GameStartTick = Server()->Tick();
		m_BossNextTeleport = Server()->Tick() + Server()->TickSpeed() * 8;
		SelectBossAttack();
		SendChatTarget(-1, "10 Hour Burst Man 已出现！每名挑战者6条命；远离森林黑暗边界。Boss免疫击退、冻结和倒地。");
	}
	if(Remaining == 0) { FinishPlayer(-1, "10 Hour Burst Man 获胜：所有挑战者已失踪。"); return; }
	if(RemainingSeconds() <= 0) { FinishPlayer(-1, "挑战失败：时间耗尽。"); return; }
	if(m_BossDrinkUntil)
	{
		if(Server()->Tick() < m_BossDrinkUntil) return;
		m_BossDrinkUntil = 0; m_BossPhase = 2; m_BossHealth = m_BossMaxHealth;
		m_GameStartTick = Server()->Tick();
		SelectBossAttack();
		SendChatTarget(-1, "二阶段！Boss速度28，承受双倍伤害；倒计时重新开始。");
	}
	// Bounded projectile pool: each bolt owns an independent one-hit-per-player mask.
	for(auto It = m_BossShots.begin(); It != m_BossShots.end();)
	{
		vec2 Previous = It->m_Pos;
		It->m_Pos += It->m_Vel;
		bool Remove = --It->m_Life <= 0;
		vec2 End = It->m_Pos;
		if(!It->m_Pierce && GameServer()->Collision()->IntersectLine(Previous, End, &End, nullptr)) Remove = true;
		for(int CID = 0; CID < MAX_CLIENTS; ++CID)
		{
			CPlayer *pPlayer = GetPlayerIfInRoom(CID);
			CCharacter *pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;
			if(!pChr || !m_aParticipants[CID] || (It->m_Hits & ((uint64)1 << CID))) continue;
			vec2 Closest = Previous; closest_point_on_line(Previous, End, pChr->m_Pos, Closest);
			if(distance(Closest, pChr->m_Pos) > 30) continue;
			It->m_Hits |= (uint64)1 << CID;
			BossHitPlayer(pChr, It->m_Damage, It->m_Pierce ? vec2(0, -5) : vec2(0, -1), It->m_Pierce ? 1.0f : 0);
			if(!It->m_Pierce) { Remove = true; break; }
		}
		if(Remove) { Server()->SnapFreeID(It->m_ID); It = m_BossShots.erase(It); } else ++It;
	}
	if((!pTarget || m_BossStunUntil > Server()->Tick()) && m_BossGrabCID < 0) return;
	if(m_BossGrabCID < 0 && Server()->Tick() >= m_BossNextTeleport)
	{
		GameWorld()->CreatePlayerSpawn(m_BossPos);
		const float X = secure_rand_below(12) == 0 ? (secure_rand_below(2) ? BOSS_TRIGGER_X + 160 : BOSS_ARENA_END - 320) :
			clamp(pTarget->m_Pos.x + (secure_rand_below(2) ? 1 : -1) * (256 + secure_rand_below(512)), BOSS_TRIGGER_X + 128, BOSS_ARENA_END - 256);
		m_BossPos = vec2(X, 1760);
		m_BossNextTeleport = Server()->Tick() + Server()->TickSpeed() * (6 + secure_rand_below(7));
		m_BossShadowUntil = Server()->Tick() + Server()->TickSpeed() * 3 / 5;
		GameWorld()->CreateSound(m_BossPos, SOUND_NINJA_FIRE);
	}
	vec2 Delta = pTarget ? pTarget->m_Pos - m_BossPos : vec2(0, 0);
	if(length(Delta) > 0) m_BossAim = normalize(Delta);
	if(m_BossDashUntil > Server()->Tick())
	{
		vec2 Previous = m_BossPos;
		GameServer()->Collision()->MoveBox(&m_BossPos, &m_BossVel, vec2(48, 64), 0);
		for(int CID = 0; CID < MAX_CLIENTS; ++CID)
		{
			CPlayer *pPlayer = GetPlayerIfInRoom(CID);
			CCharacter *pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;
			if(!pChr || !m_aParticipants[CID] || (m_BossDashHits & ((uint64)1 << CID))) continue;
			vec2 Closest = Previous; closest_point_on_line(Previous, m_BossPos, pChr->m_Pos, Closest);
			if(distance(Closest, pChr->m_Pos) < 52 && !GameServer()->Collision()->IntersectLine(Closest, pChr->m_Pos, nullptr, nullptr))
			{ m_BossDashHits |= (uint64)1 << CID; BossHitPlayer(pChr, CAsylumWeapon::DARKHEART_DASH_DAMAGE, m_BossAim * 5, 0.5f, true); }
		}
		return;
	}
	if(m_BossGrabCID >= 0)
	{
		CCharacter *pGrabbed = GameServer()->GetPlayerChar(m_BossGrabCID);
		if(pGrabbed && pGrabbed->IsAlive())
		{
			const float Side = m_BossGrabStart.x < (BOSS_TRIGGER_X + BOSS_ARENA_END) / 2 ? -1.0f : 1.0f;
			m_BossVel = vec2(Side * 5, 0);
			GameServer()->Collision()->MoveBox(&m_BossPos, &m_BossVel, vec2(48, 64), 0);
			pGrabbed->ResetHook(); pGrabbed->Core()->m_Vel = vec2(0, 0);
			pGrabbed->m_Pos = pGrabbed->Core()->m_Pos = m_BossPos + vec2(-Side * 38, 0);
			pGrabbed->Freeze(0.1f, true);
			if(Server()->Tick() < m_BossGrabUntil) return;
			BossHitPlayer(pGrabbed, 100000, vec2(0, 0));
		}
		m_BossGrabCID = -1; m_BossGrabUntil = 0;
		if(m_BossAttackUses >= (m_BossPhase == 1 ? 1 : 3)) SelectBossAttack();
	}
	if(m_BossAttack == 0 && m_BossAttackUses >= 3) SelectBossAttack();
	const float Base = m_BossPhase == 1 ? 22 : 28;
	float Speed = Base;
	if(m_BossAttack == 1 || m_BossAttack >= 5) Speed += 8;
	if(m_BossAttack == 2) Speed -= m_BossPhase == 1 ? 6 : 4;
	if(m_BossAttack == 3) Speed += m_BossPhase == 1 ? 4 : 6;
	if(m_BossAttack == 4) Speed -= m_BossPhase == 1 ? 8 : 6;
	const bool WindupAttack = m_BossAttack == 0 || m_BossAttack == 1 || m_BossAttack == 3 || m_BossAttack == 6;
	if(WindupAttack && !m_BossWindingUp && length(Delta) < STUD * (m_BossAttack == 0 ? 22 : (m_BossAttack == 1 ? 12 : (m_BossAttack == 6 ? 14 : 10))))
	{
		m_BossWindingUp = true;
		m_BossAttackAim = m_BossAim; // Telegraph a committed direction that can be dodged.
		m_BossNextAttack = Server()->Tick() + maximum(1, Server()->TickSpeed() * (m_BossAttack == 0 ? 3 : (m_BossAttack == 1 ? 7 : 5)) / 10);
		if(m_BossAttack == 1 || m_BossAttack == 6) GameWorld()->CreateHammerHit(m_BossPos + m_BossAim * 56);
	}
	bool Windup = m_BossWindingUp || (m_BossAttack == 5 && m_BossAttackUses > 0);
	m_BossVel.x = Windup ? 0 : (Delta.x >= 0 ? 1 : -1) * Speed * 32 / Server()->TickSpeed();
	m_BossVel.y = minimum(m_BossVel.y + 0.5f, 12.0f);
	if(Delta.y < -64 && GameServer()->Collision()->CheckPoint(m_BossPos + vec2(0, 34))) m_BossVel.y = -12;
	GameServer()->Collision()->MoveBox(&m_BossPos, &m_BossVel, vec2(48, 64), 0);
	if(WindupAttack && !m_BossWindingUp) return; // Approach before committing to a melee attack.
	if(Server()->Tick() < m_BossNextAttack) return;
	int MaxUses = 1;
	float Delay = 0.75f;
	bool Hit = false;
	m_BossLastFireTick = Server()->Tick();
	if(m_BossAttack == 0)
	{
		MaxUses = 3;
		m_BossDashUntil = Server()->Tick() + round_to_int(Server()->TickSpeed() * CAsylumWeapon::DARKHEART_DASH_SECONDS);
		m_BossDashHits = 0;
		m_BossVel = m_BossAttackAim * 28;
	}
	else if(m_BossAttack == 4 || m_BossAttack == 5)
	{
		MaxUses = 4; Delay = m_BossAttack == 5 ? 0.25f : (m_BossPhase == 1 ? 0.6f : 0.25f);
		int Pellets = m_BossAttack == 5 ? 1 : 8;
		for(int i = 0; i < Pellets && m_BossShots.size() < 64; ++i)
		{
			float Angle = angle(m_BossAim) + (i - (Pellets - 1) * 0.5f) * 0.10f;
			vec2 Dir(cosf(Angle), sinf(Angle));
			int Damage = m_BossAttack == 5 ? 35 : round_to_int(14 - clamp((length(Delta) - STUD * 10) / (STUD * 20), 0.0f, 1.0f) * 4.7f);
			m_BossShots.push_back({m_BossPos, Dir * (STUD * (m_BossAttack == 5 ? 175 : 500) / Server()->TickSpeed()), Server()->TickSpeed() * 3, Damage, Server()->SnapNewID(), m_BossAttack == 5, 0});
		}
		GameWorld()->CreateSound(m_BossPos, m_BossAttack == 5 ? SOUND_LASER_FIRE : SOUND_SHOTGUN_FIRE);
	}
	else
	{
		if(m_BossAttack == 2) { MaxUses = 6; Delay = 0.35f; }
		if(m_BossAttack == 3) MaxUses = m_BossPhase == 1 ? 1 : 3;
		for(int CID = 0; CID < MAX_CLIENTS; ++CID)
		{
			CPlayer *pPlayer = GetPlayerIfInRoom(CID);
			CCharacter *pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;
			if(!pChr || !m_aParticipants[CID] || GameServer()->Collision()->IntersectLine(m_BossPos, pChr->m_Pos, nullptr, nullptr)) continue;
			vec2 D = pChr->m_Pos - m_BossPos;
			const vec2 AttackAim = WindupAttack ? m_BossAttackAim : m_BossAim;
			float Along = dot(D, AttackAim), Across = fabs(dot(D, vec2(-AttackAim.y, AttackAim.x)));
			if(m_BossAttack == 1)
			{
				// Twilight: direct blade 85 plus grounded shockwave 20.
				if(Along >= -28 && Along < STUD * 5 + 28 && Across < STUD * 2.5f + 28) Hit |= BossHitPlayer(pChr, 85, vec2(0, -5), 1);
				if(fabs(D.x) < STUD * 15 && fabs(D.y) < 48 && GameServer()->Collision()->CheckPoint(pChr->m_Pos + vec2(0, 30))) Hit |= BossHitPlayer(pChr, 20, vec2(0, -10), 1);
			}
			else
			{
				float Reach = STUD * (m_BossAttack == 6 ? 12 : (m_BossAttack == 3 ? 8 : 5));
				float Width = STUD * (m_BossAttack == 6 ? 8 : 5) * 0.5f;
				if(Along >= 0 && Along <= Reach + 28 && Across <= Width + 28)
				{
					if(m_BossAttack == 3 && m_BossGrabCID < 0)
					{
						if(BossHitPlayer(pChr, 1, vec2(0, 0)) && pChr->IsAlive())
						{
							Hit = true; m_BossGrabCID = CID; m_BossGrabStart = pChr->m_Pos;
							m_BossGrabUntil = Server()->Tick() + Server()->TickSpeed() * 3 / 5;
							m_BossShadowUntil = m_BossGrabUntil;
							pChr->Freeze(0.7f, true); pChr->ResetHook();
						}
					}
					else if(m_BossAttack != 3)
						Hit |= BossHitPlayer(pChr, m_BossAttack == 6 ? 45 : 72, AttackAim * (m_BossAttack == 6 ? 24 : 6) + vec2(0, -4), 1);
				}
			}
		}
		GameWorld()->CreateHammerHit(m_BossPos + m_BossAim * 48);
		if(m_BossAttack == 1) GameWorld()->CreateExplosionParticle(m_BossPos);
	}
	++m_BossAttackUses;
	m_BossWindingUp = false;
	m_BossNextAttack = Server()->Tick() + maximum(1, round_to_int(Delay * Server()->TickSpeed()));
	if(m_BossAttackUses >= MaxUses)
	{
		int Completed = m_BossAttack;
		if(Completed == 0 || (Completed == 3 && m_BossGrabCID >= 0)) return;
		SelectBossAttack();
		if(Completed == 5 || (!Hit && (Completed == 1 || Completed == 3 || Completed == 6)))
			m_BossStunUntil = m_BossNextAttack = Server()->Tick() + Server()->TickSpeed() * 2;
		else m_BossNextAttack = Server()->Tick() + Server()->TickSpeed() / 2;
	}
}

void CGameControllerHunterN::BossStatus(char *pBuf, int Size) const
{
	if(!m_BossMaxHealth) { str_copy(pBuf, "BOSS | 向右穿过灯柱开始战斗 | 6条命", Size); return; }
	str_format(pBuf, Size, "10 Hour Burst Man | HP %d/%d | P%d%s", m_BossHealth, m_BossMaxHealth, m_BossPhase,
		m_BossDrinkUntil ? " | 恢复中" : "");
}

void CGameControllerHunterN::OnSnap(int SnappingClient)
{
	if(!m_TourBoss || m_BossHealth <= 0 || m_TourChangingMap) return;
	auto Line = [&](int ID, vec2 A, vec2 B) {
		if(ID < 0) return;
		auto *pObj = (CNetObj_Laser *)Server()->SnapNewItem(NETOBJTYPE_LASER, ID, sizeof(CNetObj_Laser));
		if(!pObj) return;
		pObj->m_FromX = round_to_int(A.x); pObj->m_FromY = round_to_int(A.y);
		pObj->m_X = round_to_int(B.x); pObj->m_Y = round_to_int(B.y); pObj->m_StartTick = Server()->Tick();
	};
	// Synthetic wire identity only: never construct a CPlayer, accept input,
	// or index player arrays with a snapshot-pool ID. The default Tour reserves
	// one slot; custom/full servers omit the Tee rather than alias a real CID.
	bool aUsed[MAX_CLIENTS] = {};
	const bool Legacy = SnappingClient >= 0 && !Server()->IsSixup(SnappingClient) &&
		GameServer()->GetClientVersion(SnappingClient) < VERSION_DDNET_OLD;
	const int Slots = Legacy ? VANILLA_MAX_CLIENTS - 1 : MAX_CLIENTS;
	for(int CID = 0; CID < MAX_CLIENTS; ++CID)
	{
		if(!GameServer()->m_apPlayers[CID] && !Server()->ClientIngame(CID)) continue;
		int Mapped = CID;
		if(SnappingClient >= 0 && !Server()->Translate(Mapped, SnappingClient)) continue;
		if(Mapped >= 0 && Mapped < Slots) aUsed[Mapped] = true;
	}
	int BossCID = -1;
	for(int CID = Slots - 1; CID >= 0; --CID)
		if(!aUsed[CID]) { BossCID = CID; break; }
	if(BossCID >= 0)
	{
		const bool Sixup = SnappingClient >= 0 && Server()->IsSixup(SnappingClient);
		if(!Sixup)
		{
			auto *pInfo = (CNetObj_ClientInfo *)Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, BossCID, sizeof(CNetObj_ClientInfo));
			if(pInfo)
			{
				StrToInts(&pInfo->m_Name0, 4, "10H Burst Man"); StrToInts(&pInfo->m_Clan0, 3, "AI BOSS");
				StrToInts(&pInfo->m_Skin0, 6, "blacktee");
				// Client-side supplied atlas has black eyes; x_ names are filtered
				// by stock DDNet when the server does not advertise allow-X-skins.
				pInfo->m_UseCustomColor = 0; pInfo->m_ColorBody = pInfo->m_ColorFeet = 0; pInfo->m_Country = -1;
			}
			auto *pPlayer = (CNetObj_PlayerInfo *)Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, BossCID, sizeof(CNetObj_PlayerInfo));
			if(pPlayer) { pPlayer->m_Local = 0; pPlayer->m_ClientID = BossCID; pPlayer->m_Team = TEAM_RED; pPlayer->m_Score = 0; pPlayer->m_Latency = 0; }
		}
		else
		{
			if(m_aBossSnapCID[SnappingClient] != BossCID)
			{
				int Old = m_aBossSnapCID[SnappingClient];
				if(Old >= 0 && !GameServer()->m_apPlayers[Old])
				{
					protocol7::CNetMsg_Sv_ClientDrop Drop = {};
					Drop.m_ClientID = Old; Drop.m_pReason = ""; Drop.m_Silent = true;
					Server()->SendPackMsg(&Drop, MSGFLAG_VITAL | MSGFLAG_NORECORD, SnappingClient);
				}
				protocol7::CNetMsg_Sv_ClientInfo Info = {};
				Info.m_ClientID = BossCID; Info.m_pName = "10H Burst Man"; Info.m_pClan = "AI BOSS";
				Info.m_Country = -1; Info.m_Team = TEAM_RED; Info.m_Silent = true;
			const char *apParts[] = {"standard", "", "", "standard", "standard", "standard"};
				for(int Part = 0; Part < 6; ++Part) { Info.m_apSkinPartNames[Part] = apParts[Part]; Info.m_aUseCustomColors[Part] = 1; Info.m_aSkinPartColors[Part] = 0; }
				Server()->SendPackMsg(&Info, MSGFLAG_VITAL | MSGFLAG_NORECORD, SnappingClient);
				m_aBossSnapCID[SnappingClient] = BossCID;
			}
			auto *pPlayer = (protocol7::CNetObj_PlayerInfo *)Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, BossCID, sizeof(protocol7::CNetObj_PlayerInfo));
			if(pPlayer) { pPlayer->m_PlayerFlags = 0; pPlayer->m_Score = 0; pPlayer->m_Latency = 0; }
		}
		CNetObj_Character Core = {};
		Core.m_Tick = 0; // Always render the authoritative AI position, no dead reckoning.
		Core.m_X = round_to_int(m_BossPos.x); Core.m_Y = round_to_int(m_BossPos.y);
		Core.m_VelX = round_to_int(m_BossVel.x * 256); Core.m_VelY = round_to_int(m_BossVel.y * 256);
		Core.m_Angle = round_to_int(angle(m_BossAim) * 256); Core.m_Direction = m_BossVel.x > 0 ? 1 : (m_BossVel.x < 0 ? -1 : 0);
		Core.m_HookedPlayer = -1; Core.m_HookX = Core.m_X; Core.m_HookY = Core.m_Y;
		Core.m_Weapon = m_BossAttack == 4 ? WEAPON_SHOTGUN : (m_BossAttack == 5 ? WEAPON_LASER : WEAPON_HAMMER);
		Core.m_Emote = EMOTE_ANGRY; Core.m_AttackTick = m_BossLastFireTick;
		if(m_BossShadowUntil <= Server()->Tick())
		{
			if(!Sixup)
			{
				auto *pChr = (CNetObj_Character *)Server()->SnapNewItem(NETOBJTYPE_CHARACTER, BossCID, sizeof(CNetObj_Character));
				if(pChr) *pChr = Core;
			}
			else
			{
				auto *pChr = (protocol7::CNetObj_Character *)Server()->SnapNewItem(NETOBJTYPE_CHARACTER, BossCID, sizeof(protocol7::CNetObj_Character));
				if(pChr)
				{
					mem_copy(pChr, &Core, sizeof(CNetObj_CharacterCore));
					pChr->m_Health = pChr->m_Armor = pChr->m_AmmoCount = pChr->m_TriggeredEvents = 0;
					pChr->m_Weapon = Core.m_Weapon; pChr->m_Emote = Core.m_Emote; pChr->m_AttackTick = Core.m_AttackTick;
				}
			}
			// The Tee is a visual NPC, not an extra colliding predicted player.
			auto *pDDNet = (CNetObj_DDNetCharacter *)Server()->SnapNewItem(NETOBJTYPE_DDNETCHARACTER, BossCID, sizeof(CNetObj_DDNetCharacter));
			if(pDDNet) { pDDNet->m_Flags = CHARACTERFLAG_NO_COLLISION | CHARACTERFLAG_NO_HOOK; pDDNet->m_FreezeEnd = 0; pDDNet->m_Jumps = 0; pDDNet->m_TeleCheckpoint = 0; pDDNet->m_StrongWeakID = BossCID; }
		}
	}
	if(m_BossShadowUntil <= Server()->Tick())
	{
		// Darkheart's outline is literally the same geometry as the player weapon.
		if(m_BossAttack == 0)
		{
			CAsylumHeldLaserShape::SSegment aShape[CAsylumHeldLaserShape::MAX_SEGMENTS];
			const int Count = AsylumBuildHeldShape(ASYLUM_DARKHEART, aShape);
			vec2 Aim = m_BossAim;
			const vec2 Side(-Aim.y, Aim.x);
			for(int i = 0; i < Count; ++i)
				Line(m_aBossVisualIDs[i], m_BossPos + Aim * aShape[i].m_From.x + Side * aShape[i].m_From.y,
					m_BossPos + Aim * aShape[i].m_To.x + Side * aShape[i].m_To.y);
		}
		Line(m_aBossVisualIDs[19], m_BossPos + vec2(-40, -52), m_BossPos + vec2(-40 + 80.0f * m_BossHealth / m_BossMaxHealth, -52));
	}
	for(const auto &Shot : m_BossShots)
	{
		if(Shot.m_Pierce) { Line(Shot.m_ID, Shot.m_Pos - normalize(Shot.m_Vel) * 64, Shot.m_Pos); continue; }
		auto *pShot = (CNetObj_Projectile *)Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, Shot.m_ID, sizeof(CNetObj_Projectile));
		if(!pShot) continue;
		const float Speed = maximum(1.0f, (float)GameServer()->Tuning()->m_ShotgunSpeed);
		pShot->m_X = round_to_int(Shot.m_Pos.x); pShot->m_Y = round_to_int(Shot.m_Pos.y);
		pShot->m_VelX = round_to_int(Shot.m_Vel.x * Server()->TickSpeed() * 100 / Speed);
		pShot->m_VelY = round_to_int(Shot.m_Vel.y * Server()->TickSpeed() * 100 / Speed);
		pShot->m_StartTick = Server()->Tick(); pShot->m_Type = WEAPON_SHOTGUN;
	}
}
