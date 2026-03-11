#include <pebble_worker.h>

// --- Persist keys (shared with app) ---
#define PERSIST_STEPS_TODAY 230
#define PERSIST_STEPS_DAY0  231  // yesterday (DAY0..DAY6 = 231..237)
#define PERSIST_STEPS_DATE  238  // julian day of current "today"

// --- Algorithm parameters ---
#define POLL_INTERVAL_MS  500   // accelerometer poll rate (2 Hz)
#define PERSIST_INTERVAL  10    // persist every N polls (10 × 500ms = 5s)
#define MAG_THRESHOLD     340   // magnitude delta threshold for step detection
#define MIN_STEP_INTERVAL 1     // minimum polls between steps (500ms debounce)

// --- State ---
static uint32_t s_steps_today;
static int16_t  s_last_mag;
static uint8_t  s_polls_since_persist;
static uint8_t  s_polls_since_step;
static int32_t  s_today_jday;       // julian day number of "today"
static AppTimer *s_timer;

// --- Helpers ---

// Simple julian day number from time_t (days since epoch, ignoring TZ drift)
static int32_t jday_from_time(time_t t) {
  struct tm *tm = localtime(&t);
  // Approximate: just use yday + year*366 for a monotonic day counter
  return tm->tm_year * 366 + tm->tm_yday;
}

// Rotate history: shift DAY0-5 → DAY1-6, DAY0 = today's count, reset today
static void rotate_day(void) {
  // Shift existing days forward
  for (int i = 5; i >= 0; i--) {
    int32_t val = 0;
    if (persist_exists(PERSIST_STEPS_DAY0 + i)) {
      val = persist_read_int(PERSIST_STEPS_DAY0 + i);
    }
    persist_write_int(PERSIST_STEPS_DAY0 + i + 1, val);
  }
  // Yesterday = today's count (cap at uint16 max for persist)
  uint16_t capped = (s_steps_today > 65535) ? 65535 : (uint16_t)s_steps_today;
  persist_write_int(PERSIST_STEPS_DAY0, capped);
  // Reset today
  s_steps_today = 0;
  persist_write_int(PERSIST_STEPS_TODAY, 0);
  // Update date
  s_today_jday = jday_from_time(time(NULL));
  persist_write_int(PERSIST_STEPS_DATE, s_today_jday);
}

// --- Timer callback: poll accelerometer ---
static void timer_callback(void *data) {
  AccelData accel = (AccelData){ .x = 0, .y = 0, .z = 0 };
  accel_service_peek(&accel);

  // Skip samples contaminated by vibration motor
  if (!accel.did_vibrate) {
    // Approximate magnitude using Manhattan distance (no sqrt needed)
    // Gravity is ~1000 on z-axis at rest; we care about changes
    int16_t mag = (int16_t)((abs(accel.x) + abs(accel.y) + abs(accel.z)));
    int16_t delta = abs(mag - s_last_mag);

    // Step detection: delta exceeds threshold + debounce
    if (delta > MAG_THRESHOLD && s_polls_since_step >= MIN_STEP_INTERVAL) {
      s_steps_today++;
      s_polls_since_step = 0;
    } else {
      if (s_polls_since_step < 255)
        s_polls_since_step++;
    }

    s_last_mag = mag;
  }

  // Persist periodically
  s_polls_since_persist++;
  if (s_polls_since_persist >= PERSIST_INTERVAL) {
    persist_write_int(PERSIST_STEPS_TODAY, (int32_t)s_steps_today);
    s_polls_since_persist = 0;
  }

  // Check for day change
  int32_t now_jday = jday_from_time(time(NULL));
  if (now_jday != s_today_jday) {
    rotate_day();
  }

  // Reschedule
  s_timer = app_timer_register(POLL_INTERVAL_MS, timer_callback, NULL);
}

// --- Init / Deinit ---

static void prv_init(void) {
  // Load saved state
  s_today_jday = persist_exists(PERSIST_STEPS_DATE)
                     ? persist_read_int(PERSIST_STEPS_DATE)
                     : 0;

  int32_t now_jday = jday_from_time(time(NULL));

  if (s_today_jday != now_jday && s_today_jday != 0) {
    // Day has changed since last run — rotate history
    // Handle multi-day gap: rotate once per missed day
    int32_t gap = now_jday - s_today_jday;
    if (gap < 0 || gap > 7) gap = 1; // safety clamp
    for (int32_t d = 0; d < gap; d++) {
      rotate_day();
    }
    s_today_jday = now_jday;
    persist_write_int(PERSIST_STEPS_DATE, s_today_jday);
  } else if (s_today_jday == 0) {
    // First ever run
    s_today_jday = now_jday;
    persist_write_int(PERSIST_STEPS_DATE, s_today_jday);
    s_steps_today = 0;
    persist_write_int(PERSIST_STEPS_TODAY, 0);
  } else {
    // Same day — resume count
    s_steps_today = persist_exists(PERSIST_STEPS_TODAY)
                        ? (uint32_t)persist_read_int(PERSIST_STEPS_TODAY)
                        : 0;
  }

  s_last_mag = 0;
  s_polls_since_persist = 0;
  s_polls_since_step = MIN_STEP_INTERVAL; // allow immediate first step

  // Subscribe to accelerometer (needed for accel_service_peek)
  accel_data_service_subscribe(0, NULL);

  // Start polling timer
  s_timer = app_timer_register(POLL_INTERVAL_MS, timer_callback, NULL);
}

static void prv_deinit(void) {
  // Persist final count before exit
  persist_write_int(PERSIST_STEPS_TODAY, (int32_t)s_steps_today);

  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
  accel_data_service_unsubscribe();
}

int main(void) {
  prv_init();
  worker_event_loop();
  prv_deinit();
}
