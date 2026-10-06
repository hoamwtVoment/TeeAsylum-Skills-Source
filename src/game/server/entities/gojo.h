#ifndef GAME_SERVER_ENTITIES_GOJO_H
#define GAME_SERVER_ENTITIES_GOJO_H

#include <game/server/entity.h>
#include <game/server/gojo_state.h>

void GojoSnapLine(class IServer *pServer, int ID, vec2 From, vec2 To, int Skill, int Owner);
bool GojoHasBlue(CGameWorld *pWorld, int Owner);
bool GojoHasOrb(CGameWorld *pWorld, int Owner, int Skill);
bool GojoHasDomain(CGameWorld *pWorld, int Owner);
bool GojoHasDomainMap(CGameWorld *pWorld);
void GojoClearEntities(CGameWorld *pWorld, int Owner);
void GojoConstrainMovement(CCharacter *pCharacter, vec2 Previous);

class CGojoOrb : public CEntity
{
	int m_Owner, m_WeaponID, m_Skill, m_Age = 0;
	float m_Charge, m_Radius;
	vec2 m_Velocity;
	bool m_aHit[MAX_CLIENTS] = {};
	bool m_aCarried[MAX_CLIENTS] = {};
	vec2 m_aCarryOffset[MAX_CLIENTS];
	int m_aNextRedHit[MAX_CLIENTS] = {};
	bool m_NpcHit = false;
	int m_LastNpcHitAge = -1000000;
	int m_aIDs[48];
	void Pull();
	void Hit(vec2 Previous);
	void ClearCarry();

public:
	CGojoOrb(CGameWorld *pWorld, int Owner, int Skill, vec2 Pos, vec2 Direction, float Charge);
	~CGojoOrb() override;
	void Tick() override;
	void TickPaused() override {}
	void Reset() override;
	void Snap(int SnappingClient, int OtherMode) override;
	bool NetworkClipped(int SnappingClient) override;
	int Owner() const { return m_Owner; }
	int Skill() const { return m_Skill; }
	float Radius() const { return m_Radius; }
	float Charge() const { return m_Charge; }
	vec2 Velocity() const { return m_Velocity; }
	bool Active() const { return !m_MarkedForDestroy; }
};

class CGojoDomain : public CEntity
{
	int m_Owner, m_Age = 0;
	bool m_aAffected[MAX_CLIENTS] = {};
	bool m_aAdmitted[MAX_CLIENTS] = {};
	int m_aLastBarrierHit[MAX_CLIENTS] = {};
	int m_aIDs[64];
	void Release(int CID);

public:
	CGojoDomain(CGameWorld *pWorld, int Owner, vec2 Pos);
	~CGojoDomain() override;
	void Tick() override;
	void TickPaused() override;
	void Reset() override;
	void Snap(int SnappingClient, int OtherMode) override;
	bool NetworkClipped(int SnappingClient) override;
	int Owner() const { return m_Owner; }
	bool Active() const { return !m_MarkedForDestroy; }
	void ConstrainMovement(CCharacter *pCharacter, vec2 Previous);
};

#endif
