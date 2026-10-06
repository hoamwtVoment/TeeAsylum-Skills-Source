/* Small deterministic time-stop/cooldown rules, independent of the server. */
#ifndef GAME_SERVER_ASYLUM_TIME_H
#define GAME_SERVER_ASYLUM_TIME_H

#include <algorithm>

enum
{
	ASYLUM_WORLD_SECONDS = 5,
	// Encoded recording is 4418.292ms. Round UP to the next 50Hz tick:
	// 1000ms normal lead + 3420ms slowdown, never truncate its final syllable.
	ASYLUM_WORLD_WINDUP_MS = 3420,
	ASYLUM_WORLD_AUDIO_LEAD_MS = 1000,
	ASYLUM_WORLD_COOLDOWN_SECONDS = 60,
	ASYLUM_WORLD_KILL_REDUCTION_SECONDS = 2,
	ASYLUM_WORLD_MAX_REDUCTION_SECONDS = 10,
	ASYLUM_WORLD_VOLUME_LEVELS = 12,
	ASYLUM_WORLD_GRAY_BASE_MS = 2147467264,
	ASYLUM_WORLD_GRAY_FADE_MS = 256,
	ASYLUM_JUMPSCARE_START_MS = 2147481087,
	ASYLUM_JUMPSCARE_FADE_MS = 1280,
};

class CAsylumTimeStop
{
	int m_Owner;
	int m_Until;
	int m_Start;
	int m_FadeStart;
	int m_FullStopStart;
	int m_SlowStart;
	int m_FadeOffset;
	float m_Accumulator;

public:
	CAsylumTimeStop() : m_Owner(-1), m_Until(0), m_Start(0), m_FadeStart(-1), m_FullStopStart(0), m_SlowStart(0), m_FadeOffset(0), m_Accumulator(0.0f) {}
	bool Active(int Tick) const { return m_Owner >= 0 && Tick < m_Until; }
	int Owner() const { return m_Owner; }
	bool Influencing(int Tick) const { return Active(Tick) && Tick >= m_SlowStart; }
	bool StopsClient(int CID, int Tick) const { return CID >= 0 && Influencing(Tick) && CID != m_Owner; }
	bool FullyStopped(int Tick) const { return Active(Tick) && Tick >= m_FullStopStart; }
	bool Start(int CID, int Tick, int Duration, int Windup = 0, int AudioLead = 0)
	{
		if(CID < 0 || Duration <= 0 || Active(Tick))
			return false;
		m_Owner = CID;
		m_SlowStart = Tick + std::max(0, AudioLead);
		m_Until = m_SlowStart + std::max(0, Windup) + Duration;
		m_FullStopStart = m_SlowStart + std::max(0, Windup);
		m_Start = Tick;
		m_FadeStart = -1;
		m_FadeOffset = 0;
		m_Accumulator = 0.0f;
		return true;
	}
	void End(int Tick)
	{
		if(m_Owner >= 0 && Tick >= m_SlowStart)
		{
			m_FadeStart = Tick;
			// Early cancellation fades from the current opacity, not full gray.
			m_FadeOffset = ASYLUM_WORLD_GRAY_FADE_MS - std::min((int)ASYLUM_WORLD_GRAY_FADE_MS,
				(Tick - m_SlowStart) * ASYLUM_WORLD_GRAY_FADE_MS / std::max(1, m_FullStopStart - m_SlowStart));
		}
		m_Owner = -1;
		m_Until = 0;
	}
	void Reset() { m_Owner = -1; m_Until = 0; m_FadeStart = -1; m_Accumulator = 0.0f; }
	void Pause() { if(m_Owner >= 0) { ++m_Until; ++m_Start; ++m_SlowStart; ++m_FullStopStart; } }
	bool AdvanceOthers(int Tick)
	{
		if(!Influencing(Tick))
			return true;
		if(FullyStopped(Tick))
			return false;
		const float Progress = (Tick - m_SlowStart) / (float)std::max(1, m_FullStopStart - m_SlowStart);
		const float Rate = 1.0f - Progress * Progress * (3.0f - 2.0f * Progress);
		m_Accumulator += Rate;
		if(m_Accumulator >= 1.0f)
		{
			m_Accumulator -= 1.0f;
			return true;
		}
		return false;
	}
	int VisualMillis(int Tick, int Speed) const
	{
		if(Influencing(Tick))
			return std::min((int)ASYLUM_WORLD_WINDUP_MS, (Tick - m_SlowStart) * 1000 / Speed);
		if(m_FadeStart >= 0)
		{
			const int Elapsed = m_FadeOffset + (Tick - m_FadeStart) * 1000 / Speed;
			if(Elapsed < ASYLUM_WORLD_GRAY_FADE_MS)
				return ASYLUM_WORLD_WINDUP_MS + ASYLUM_WORLD_SECONDS * 1000 + Elapsed;
		}
		return -1;
	}
};

class CAsylumWorldCooldown
{
	int m_Ready;
	int m_Reduced;
	int m_Pending;

public:
	CAsylumWorldCooldown() : m_Ready(0), m_Reduced(0), m_Pending(0) {}
	void Reset() { m_Ready = m_Reduced = m_Pending = 0; }
	int Remaining(int Tick) const { return std::max(0, m_Ready - Tick); }
	void Activate(int Tick, int Speed)
	{
		m_Ready = Tick + ASYLUM_WORLD_COOLDOWN_SECONDS * Speed;
		m_Reduced = 0;
		m_Pending = 0;
	}
	int AwardKill(int Tick, int Speed, bool Defer = false)
	{
		const int Reduction = std::min(std::max(0, Remaining(Tick) - m_Pending), std::min(ASYLUM_WORLD_KILL_REDUCTION_SECONDS * Speed,
			ASYLUM_WORLD_MAX_REDUCTION_SECONDS * Speed - m_Reduced));
		if(Defer)
			m_Pending += Reduction;
		else
			m_Ready -= Reduction;
		m_Reduced += Reduction;
		return Reduction;
	}
	void ApplyPending(int Tick) { m_Ready -= std::min(Remaining(Tick), m_Pending); m_Pending = 0; }
	void Pause(int Tick) { if(Remaining(Tick) > 0) ++m_Ready; }
};

inline int AsylumWorldVolumeLevel(float Distance, float MapDiagonal)
{
	const float Fraction = std::max(0.0f, std::min(1.0f, Distance / std::max(1.0f, MapDiagonal)));
	return std::max(0, std::min(ASYLUM_WORLD_VOLUME_LEVELS - 1,
		(int)((1.0f - Fraction) * (ASYLUM_WORLD_VOLUME_LEVELS - 1) + 0.5f)));
}

#endif
