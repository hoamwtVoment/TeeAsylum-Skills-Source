/* Build with g++ -std=c++11 -Wall -Wextra -Werror -Isrc and run. */
#include <game/server/asylum_time.h>

#include <cassert>
#include <cstdio>

int main()
{
	const int Speed = 50;
	const int Lead = ASYLUM_WORLD_AUDIO_LEAD_MS * Speed / 1000;
	const int Windup = ASYLUM_WORLD_WINDUP_MS * Speed / 1000;
	const int Duration = ASYLUM_WORLD_SECONDS * Speed;
	const int End = Lead + Windup + Duration;
	CAsylumTimeStop Stop;
	assert(Stop.Start(7, 0, Duration, Windup, Lead));
	assert(!Stop.Start(8, 0, Duration));
	for(int Tick = 0; Tick < Lead; ++Tick)
	{
		assert(Stop.Active(Tick));
		assert(!Stop.Influencing(Tick));
		assert(Stop.AdvanceOthers(Tick));
		assert(Stop.VisualMillis(Tick, Speed) == -1);
	}
	int Windows[3] = {0, 0, 0};
	for(int Tick = Lead; Tick < Lead + Windup; ++Tick)
	{
		assert(!Stop.StopsClient(7, Tick));
		assert(Stop.StopsClient(8, Tick));
		assert(!Stop.FullyStopped(Tick));
		if(Stop.AdvanceOthers(Tick))
			++Windows[(Tick - Lead) * 3 / Windup];
	}
	assert(Windows[0] > Windows[1] && Windows[1] > Windows[2]);
	assert(Windows[0] > 45 && Windows[2] < 12);
	for(int Tick = Lead + Windup; Tick < End; ++Tick)
	{
		assert(Stop.FullyStopped(Tick));
		assert(Stop.VisualMillis(Tick, Speed) == ASYLUM_WORLD_WINDUP_MS);
		assert(!Stop.AdvanceOthers(Tick));
		assert(!Stop.StopsClient(7, Tick));
		assert(Stop.StopsClient(8, Tick));
	}
	assert(!Stop.Active(End));
	Stop.End(End);
	assert(Stop.VisualMillis(End, Speed) == ASYLUM_WORLD_WINDUP_MS + 5000);
	assert(Stop.VisualMillis(End + 13, Speed) == -1);
	assert(Stop.AdvanceOthers(End));

	CAsylumWorldCooldown Cooldown;
	Cooldown.Activate(0, Speed);
	for(int Tick = 1; Tick <= End; ++Tick)
	{
		Cooldown.Pause(Tick);
		assert(Cooldown.Remaining(Tick) == 60 * Speed);
		if(Tick == 240)
		{
			assert(Cooldown.AwardKill(Tick, Speed, true) == 2 * Speed);
			assert(Cooldown.Remaining(Tick) == 60 * Speed); // Pending, not reduced while stopped.
		}
	}
	Cooldown.ApplyPending(End);
	assert(Cooldown.Remaining(End) == 58 * Speed);
	for(int i = 0; i < 4; ++i)
		assert(Cooldown.AwardKill(End, Speed) == 2 * Speed);
	assert(Cooldown.Remaining(End) == 50 * Speed);
	assert(Cooldown.AwardKill(End, Speed) == 0);
	assert(Cooldown.Remaining(End + 50 * Speed) == 0);
	Cooldown.Activate(1000, Speed);
	assert(Cooldown.AwardKill(1001, Speed, true) > 0);
	Cooldown.Reset(); // Administrator reset/no-CD must also clear pending refunds.
	assert(Cooldown.Remaining(1001) == 0);
	Cooldown.ApplyPending(1002);
	assert(Cooldown.Remaining(1002) == 0);
	Cooldown.Activate(2000, Speed);
	assert(Cooldown.AwardKill(2000, Speed) == 2 * Speed); // Reset clears the old refund cap.

	CAsylumTimeStop Paused;
	assert(Paused.Start(0, 0, Duration, Windup, Lead));
	for(int Tick = 1; Tick <= 100; ++Tick)
		Paused.Pause();
	assert(!Paused.Influencing(100 + Lead - 1));
	assert(Paused.Influencing(100 + Lead));
	assert(Paused.Active(100 + End - 1));
	assert(!Paused.Active(100 + End));
	Paused.Reset();
	assert(!Paused.Active(0) && Paused.VisualMillis(0, Speed) == -1);
	CAsylumTimeStop Cancelled;
	assert(Cancelled.Start(0, 0, Duration, Windup, Lead));
	const int Midway = Lead + Windup / 2;
	Cancelled.End(Midway);
	assert(Cancelled.VisualMillis(Midway, Speed) >= ASYLUM_WORLD_WINDUP_MS + 5000 + 120);
	assert(Cancelled.VisualMillis(Midway + 13, Speed) == -1);

	int Previous = 11;
	for(int Distance = 0; Distance <= 10000; Distance += 100)
	{
		const int Level = AsylumWorldVolumeLevel((float)Distance, 10000.0f);
		assert(Level <= Previous && Level >= 0 && Level <= 11);
		Previous = Level;
	}
	assert(AsylumWorldVolumeLevel(0.0f, 10000.0f) == 11);
	assert(AsylumWorldVolumeLevel(10000.0f, 10000.0f) == 0);
	assert(AsylumWorldVolumeLevel(999999.0f, 0.0f) == 0);
	std::printf("Time stop rules passed: 1s lead, progressive slowdown %d/%d/%d advances, full stop after 4.42s, 5s stop, frozen cooldown, capped queued refunds, fade, map-wide audio.\n", Windows[0], Windows[1], Windows[2]);
}
