#include <pebble_worker.h>

// --- Persist keys (shared with app) ---
#define PERSIST_STEPS_TODAY 230
#define PERSIST_STEPS_DAY0 231 // yesterday (DAY0..DAY6 = 231..237)
#define PERSIST_STEPS_DATE 238 // julian day of current "today"

// --- Algorithm parameters ---
#define ACCEL_BATCH_SIZE                                                       \
  25 // samples per batch (25 samples @ 25Hz = 1 wakeup/sec)
#define PERSIST_BATCHES 60 // persist every N batches (60 × 1s = 60s)
#define MAG_THRESHOLD 340  // magnitude delta threshold for step detection
#define MIN_SAMPLE_GAP                                                         \
  12 // minimum samples between steps (~480ms debounce @ 25Hz)

// --- State ---
static uint32_t s_steps_today;
static int16_t s_last_mag;
static uint16_t s_batches_since_persist;
static uint8_t s_samples_since_step;
static int32_t s_today_jday; // julian day number of "today"

// --- Helpers ---

// Simple julian day number from time_t (days since epoch, ignoring TZ drift)
static int32_t jday_from_time(time_t t) {
  struct tm *tm = localtime(&t);
  return tm->tm_year * 366 + tm->tm_yday;
}

// Rotate history: shift DAY0-5 → DAY1-6, DAY0 = today's count, reset today
static void rotate_day(void) {
  for (int i = 5; i >= 0; i--) {
    int32_t val = 0;
    if (persist_exists(PERSIST_STEPS_DAY0 + i)) {
      val = persist_read_int(PERSIST_STEPS_DAY0 + i);
    }
    persist_write_int(PERSIST_STEPS_DAY0 + i + 1, val);
  }
  uint16_t capped = (s_steps_today > 65535) ? 65535 : (uint16_t)s_steps_today;
  persist_write_int(PERSIST_STEPS_DAY0, capped);
  s_steps_today = 0;
  persist_write_int(PERSIST_STEPS_TODAY, 0);
  s_today_jday = jday_from_time(time(NULL));
  persist_write_int(PERSIST_STEPS_DATE, s_today_jday);
}

// --- Accelerometer batch callback (hardware-driven, ~1×/sec) ---
static void accel_batch_handler(AccelData *data, uint32_t num_samples) {
  for (uint32_t i = 0; i < num_samples; i++) {
    if (data[i].did_vibrate) {
      if (s_samples_since_step < 255)
        s_samples_since_step++;
      continue;
    }

    int16_t mag = (int16_t)(abs(data[i].x) + abs(data[i].y) + abs(data[i].z));
    int16_t delta = abs(mag - s_last_mag);

    if (delta > MAG_THRESHOLD && s_samples_since_step >= MIN_SAMPLE_GAP) {
      s_steps_today++;
      s_samples_since_step = 0;
    } else {
      if (s_samples_since_step < 255)
        s_samples_since_step++;
    }

    s_last_mag = mag;
  }

  // Persist periodically (~60s)
  s_batches_since_persist++;
  if (s_batches_since_persist >= PERSIST_BATCHES) {
    persist_write_int(PERSIST_STEPS_TODAY, (int32_t)s_steps_today);
    s_batches_since_persist = 0;

    // Check for day change only at persist time (every ~60s, not every sample)
    int32_t now_jday = jday_from_time(time(NULL));
    if (now_jday != s_today_jday) {
      rotate_day();
    }
  }
}

// --- Init / Deinit ---

static void prv_init(void) {
  s_today_jday = persist_exists(PERSIST_STEPS_DATE)
                     ? persist_read_int(PERSIST_STEPS_DATE)
                     : 0;

  int32_t now_jday = jday_from_time(time(NULL));

  if (s_today_jday != now_jday && s_today_jday != 0) {
    int32_t gap = now_jday - s_today_jday;
    if (gap < 0 || gap > 7)
      gap = 1;
    for (int32_t d = 0; d < gap; d++) {
      rotate_day();
    }
    s_today_jday = now_jday;
    persist_write_int(PERSIST_STEPS_DATE, s_today_jday);
  } else if (s_today_jday == 0) {
    s_today_jday = now_jday;
    persist_write_int(PERSIST_STEPS_DATE, s_today_jday);
    s_steps_today = 0;
    persist_write_int(PERSIST_STEPS_TODAY, 0);
  } else {
    s_steps_today = persist_exists(PERSIST_STEPS_TODAY)
                        ? (uint32_t)persist_read_int(PERSIST_STEPS_TODAY)
                        : 0;
  }

  s_last_mag = 0;
  s_batches_since_persist = 0;
  s_samples_since_step = MIN_SAMPLE_GAP; // allow immediate first step

  // Hardware batching: accelerometer fills its FIFO at 25Hz,
  // CPU wakes only when 25 samples are ready (~1×/sec)
  accel_data_service_subscribe(ACCEL_BATCH_SIZE, accel_batch_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);
}

static void prv_deinit(void) {
  persist_write_int(PERSIST_STEPS_TODAY, (int32_t)s_steps_today);
  accel_data_service_unsubscribe();
}

int main(void) {
  prv_init();
  worker_event_loop();
  prv_deinit();
}
