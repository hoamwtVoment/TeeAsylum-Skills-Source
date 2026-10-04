/* A 2D adaptation of Marisa's charged, extremely thick Master Spark laser. */
#ifndef GAME_SERVER_ENTITIES_ASYLUM_SPARK_H
#define GAME_SERVER_ENTITIES_ASYLUM_SPARK_H

#include <game/server/entity.h>

class CMasterSpark : public CEntity
{
	enum
	{
		NUM_BEAM_LINES = 33,
		NUM_RING_LINES = 12,
		NUM_IDS = NUM_BEAM_LINES + NUM_RING_LINES,
	};
	int m_Owner;
	int m_WeaponID;
	int m_Damage;
	int m_Age;
	vec2 m_Direction;
	int m_aIDs[NUM_IDS];
	int ChargeTicks();
	float BeamRadius(float Along) const;
	void DamageBeam(CCharacter *pOwner);
	void SnapLine(int ID, vec2 From, vec2 To);

public:
	CMasterSpark(CGameWorld *pWorld, int Owner, int WeaponID, vec2 Pos, vec2 Direction, int Damage);
	~CMasterSpark() override;
	bool IsCasting(int Owner, int WeaponID) const { return !m_MarkedForDestroy && m_Owner == Owner && m_WeaponID == WeaponID; }
	void Reset() override;
	void Tick() override;
	bool NetworkClipped(int SnappingClient) override;
	void Snap(int SnappingClient, int OtherMode) override;
};

#endif
