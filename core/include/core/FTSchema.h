#pragma once

#include "core/FTTypes.h"

#include <cstdint>
#include <string>

class FTDatabase;

inline constexpr int CurrentSchemaVersion = 37;

/** The version stored in the file, or 0 for a database that has never been migrated. */
int ReadSchemaVersion(FTDatabase& Target);

bool MigrateSchema(FTDatabase& Target);

bool MigrateDeviceSchema(FTDatabase& Target);

/** The id this copy of the database writes its own daily stats under. Created when the database is
    first migrated, never merged, and empty only for a database that has not been migrated. */
std::string ReadLocalDeviceId(FTDatabase& Target);

/** Milliseconds since the Unix epoch on this machine's own clock. What the day logic reads. */
int64_t GetRawSystemTimeMilliseconds();

/**
 * Milliseconds since the Unix epoch, as the sync compares them: what CreatedDate and
 * LastUpdatedDate hold. The machine's clock corrected by the offset the last sync measured against
 * Google's, and never earlier than the newest stamp this process issued or saw in a merge, so an
 * edit made after a sync is always newer than what the sync brought in.
 */
int64_t GetSystemTimeMilliseconds();

/** How far a clock may differ from Google's before it is corrected, and how far past now a stamp
    may sit before a merge treats it as a clock error. */
inline constexpr int64_t MaximumClockDriftMilliseconds = 5 * 60 * 1000;

/** Records the server's time, read from a response that has just arrived. A clock within the
    allowed drift is left alone; one further out is corrected by the difference. */
void ObserveServerTime(int64_t ServerMilliseconds);

void SetClockSkewMilliseconds(int64_t Skew);
int64_t GetClockSkewMilliseconds();

/** Raises the floor under new stamps to a timestamp read from another device. A stamp too far past
    now to be real is ignored, so one device with a wrong clock cannot drag the others forward. */
void NoteSeenTimestamp(int64_t Milliseconds);

/** A timestamp from another device, brought back to now when it lies further ahead than a clock
    may drift, so a wrong clock cannot win every conflict from then on. */
int64_t ClampFutureTimestamp(int64_t Milliseconds);

/** The offset the last sync measured, kept in the database so an offline start uses it too. */
bool StoreClockSkew(FTDatabase& Target, int64_t Skew);
int64_t LoadClockSkew(FTDatabase& Target);

/**
 * The day an instant belongs to, in the machine's own timezone, as whole days since the Unix
 * epoch. What DueDate, BoxChangedDate and CompletedDate hold.
 *
 * The day turns at 02:00 rather than at midnight, so someone still working after midnight is
 * still on the day they started. Late-evening study is the normal case and a session that
 * silently counts towards tomorrow loses the streak the person just earned.
 */
FTDayNumber DayNumberForMilliseconds(int64_t Milliseconds);

/** The day right now belongs to, by the rule above. */
FTDayNumber GetTodayDayNumber();

#ifdef QT_DEBUG
/**
 * Debug builds only. Shifts what the whole app believes today is, so the schedule can be walked
 * forward without waiting real days for cards to come due.
 *
 * Every due date, expiry check and promotion reads the day through GetTodayDayNumber, so moving it
 * moves all of them together. Nothing stored changes: the cards keep their real dates and the
 * offset only alters which of them look due, so setting it back to zero restores the true state.
 *
 * Persisted, since a restart while debugging a multi-day schedule should not silently return to
 * today and make the schedule look wrong.
 */
void SetDebugDayOffset(int Days);
int GetDebugDayOffset();
#endif
