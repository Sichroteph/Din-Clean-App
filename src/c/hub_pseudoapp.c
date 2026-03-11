#include "hub_pseudoapp.h"

/* Persistent state (BSS — zero heap cost) */
static time_t s_tend;          /* timer: absolute end, 0=idle */
static uint8_t s_tmin = 5;     /* timer: setup minutes 1-99 */
static uint8_t s_ah = 7, s_am; /* alarm: target H:M */
static uint8_t s_ast;          /* alarm: 0=editH 1=editM 2=armed */
static AppTimer *s_at;         /* shared background timer */
static time_t s_sw_start;      /* stopwatch: time() when started, 0=stopped */
static uint32_t s_sw_elapsed;  /* stopwatch: accumulated seconds when stopped */

typedef struct {
  Window *w;
  TextLayer *t1, *t2;
  uint8_t id;
} PA;
static PA *s_v;       /* visible PA (NULL when closed) */
static char s_buf[6]; /* "HH:MM\0" */
static uint8_t s_ring_count;
uint8_t g_hub_ring_active;

static void play_vibe(void) {
  switch (g_hub_config.vibe_pattern) {
    case 0: vibes_short_pulse(); break;
    case 2: vibes_double_pulse(); break;
    default: vibes_long_pulse(); break;
  }
}

void hub_ring_dismiss(void) {
  s_ring_count = 0;
  g_hub_ring_active = 0;
  vibes_cancel();
}
static const char *s_names[] = {"Stopwatch", "Timer", "Alarm"};

static void fmt(int a, int b) {
  s_buf[0] = '0' + a / 10;
  s_buf[1] = '0' + a % 10;
  s_buf[2] = ':';
  s_buf[3] = '0' + b / 10;
  s_buf[4] = '0' + b % 10;
  s_buf[5] = 0;
}

static void refresh(void) {
  if (!s_v)
    return;
  int a = 0, b = 0;
  const char *title = s_names[s_v->id];
  if (s_v->id == HUB_APP_STOPWATCH) {
    uint32_t e = s_sw_elapsed;
    if (s_sw_start)
      e += (uint32_t)(time(NULL) - s_sw_start);
    a = (int)(e / 60);
    if (a > 99)
      a = 99;
    b = (int)(e % 60);
    title = s_sw_start ? "STOP" : "Stopwatch";
  } else if (s_v->id == HUB_APP_TIMER) {
    title = s_tend ? "STOP" : "Timer";
    if (s_tend) {
      int r = (int)(s_tend - time(NULL));
      if (r < 0)
        r = 0;
      a = r / 60;
      b = r % 60;
    } else {
      a = s_tmin;
    }
  } else if (s_v->id == HUB_APP_ALARM) {
    title = s_ast == 2 ? "ARMED" : "Alarm";
    a = s_ah;
    b = s_am;
  }
  fmt(a, b);
  text_layer_set_text(s_v->t1, title);
  text_layer_set_text((s_v)->t2, s_buf);
}

static void pa_tick(void *d) {
  s_at = NULL;
  /* Ring loop: replay pattern, decrement, reschedule */
  if (s_ring_count) {
    play_vibe();
    light_enable_interaction();
    if (--s_ring_count)
      s_at = app_timer_register(2000, pa_tick, NULL);
    else
      g_hub_ring_active = 0;
    if (s_v) refresh();
    return;
  }
  if (s_tend && time(NULL) >= s_tend) {
    s_tend = 0;
    s_ring_count = 10;
    g_hub_ring_active = 1;
    play_vibe();
    light_enable_interaction();
  }
  if (s_ast == 2) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (tm->tm_hour == s_ah && tm->tm_min == s_am) {
      s_ast = 0;
      s_ring_count = 10;
      g_hub_ring_active = 2;
      play_vibe();
      light_enable_interaction();
    }
  }
  if (s_v)
    refresh();
  else if (g_hub_ring_active)
    layer_mark_dirty(window_get_root_layer(window_stack_get_top_window()));
  int sw_vis = s_sw_start && s_v && s_v->id == HUB_APP_STOPWATCH;
  if (s_ring_count || s_tend || s_ast == 2 || sw_vis) {
    uint32_t ms = s_ring_count ? 2000
                 : (sw_vis || (s_tend && s_v && s_v->id == HUB_APP_TIMER)) ? 1000
                 : s_tend ? (uint32_t)(s_tend - time(NULL)) * 1000
                 : 30000;
    s_at = app_timer_register(ms, pa_tick, NULL);
  }
}

static void adj(int d) {
  hub_timeout_reset();
  if (!s_v)
    return;
  if (s_v->id == HUB_APP_STOPWATCH && !s_sw_start) {
    s_sw_elapsed = 0;
  } else if (s_v->id == HUB_APP_TIMER && !s_tend) {
    int v = s_tmin + d;
    if (v < 1)
      v = 1;
    if (v > 99)
      v = 99;
    s_tmin = (uint8_t)v;
  } else if (s_v->id == HUB_APP_ALARM && s_ast < 2) {
    if (!s_ast)
      s_ah = (s_ah + 24 + d) % 24;
    else
      s_am = (s_am + 60 + d * 5) % 60;
  }
  refresh();
}

static void h_up(ClickRecognizerRef r, void *c) {
  if (s_ring_count) { hub_ring_dismiss(); refresh(); return; }
  adj(1);
}
static void h_dn(ClickRecognizerRef r, void *c) {
  if (s_ring_count) { hub_ring_dismiss(); refresh(); return; }
  adj(-1);
}

static void h_sel(ClickRecognizerRef r, void *c) {
  hub_timeout_reset();
  if (!s_v)
    return;
  if (s_ring_count) { hub_ring_dismiss(); refresh(); return; }
  if (s_v->id == HUB_APP_STOPWATCH) {
    if (s_sw_start) {
      s_sw_elapsed += (uint32_t)(time(NULL) - s_sw_start);
      s_sw_start = 0;
      if (s_at) {
        app_timer_cancel(s_at);
        s_at = NULL;
      }
    } else {
      s_sw_start = time(NULL);
      if (s_at)
        app_timer_cancel(s_at);
      s_at = app_timer_register(1000, pa_tick, NULL);
    }
  } else if (s_v->id == HUB_APP_TIMER) {
    if (s_tend) {
      s_tend = 0;
      if (s_at) {
        app_timer_cancel(s_at);
        s_at = NULL;
      }
    } else {
      s_tend = time(NULL) + (time_t)s_tmin * 60;
      if (s_at)
        app_timer_cancel(s_at);
      s_at = app_timer_register(1000, pa_tick, NULL);
    }
  } else if (s_v->id == HUB_APP_ALARM) {
    if (s_ast == 2) {
      s_ast = 0;
      if (s_at) {
        app_timer_cancel(s_at);
        s_at = NULL;
      }
    } else {
      if (++s_ast == 2) {
        if (s_at)
          app_timer_cancel(s_at);
        s_at = app_timer_register(30000, pa_tick, NULL);
      }
    }
  }
  refresh();
}

static void ccfg(void *c) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 200, h_up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 200, h_dn);
  window_single_click_subscribe(BUTTON_ID_SELECT, h_sel);
}

/* Shared TextLayer factory — saves ~80 bytes vs inline setup */
static TextLayer *mktl(GRect r, const char *font, Layer *par) {
  TextLayer *tl = text_layer_create(r);
  if (!tl)
    return NULL;
  text_layer_set_font(tl, fonts_get_system_font(font));
  text_layer_set_text_alignment(tl, GTextAlignmentCenter);
  text_layer_set_background_color(tl, GColorBlack);
  text_layer_set_text_color(tl, GColorWhite);
  layer_add_child(par, text_layer_get_layer(tl));
  return tl;
}

static void pa_load(Window *w) {
  PA *p = window_get_user_data(w);
  Layer *root = window_get_root_layer(w);
  GRect b = layer_get_bounds(root);
  p->t1 = mktl(GRect(0, 20, b.size.w, 30), FONT_KEY_GOTHIC_24_BOLD, root);
  p->t2 = mktl(GRect(0, 55, b.size.w, 50), FONT_KEY_BITHAM_42_BOLD, root);
  s_v = p;
  refresh();
  window_set_click_config_provider(w, ccfg);
  if (s_tend || s_ast == 2 || s_sw_start) {
    if (s_at) {
      app_timer_cancel(s_at);
      s_at = NULL;
    }
    uint32_t ms = ((p->id == HUB_APP_STOPWATCH && s_sw_start) ||
                   (p->id == HUB_APP_TIMER && s_tend))
                      ? 1000
                      : 30000;
    s_at = app_timer_register(ms, pa_tick, NULL);
  }
  hub_timeout_reset();
}

static void pa_unload(Window *w) {
  PA *p = window_get_user_data(w);
  s_v = NULL;
  if (s_at) {
    app_timer_cancel(s_at);
    s_at = NULL;
  }
  if (s_tend || s_ast == 2) {
    uint32_t ms = s_tend ? (uint32_t)(s_tend - time(NULL)) * 1000 : 30000;
    if (ms > 0 && ms < 360000000)
      s_at = app_timer_register(ms, pa_tick, NULL);
    else if (s_tend)
      s_tend = 0;
  }
  text_layer_destroy(p->t1);
  text_layer_destroy(p->t2);
  window_destroy(p->w);
  free(p);
}

void hub_pseudoapp_push(uint8_t app_id) {
  if (app_id >= HUB_APP_COUNT)
    return;
  PA *p = malloc(sizeof(PA));
  if (!p)
    return;
  p->id = app_id;
  p->w = window_create();
  if (!p->w) {
    free(p);
    return;
  }
  window_set_user_data(p->w, p);
  window_set_window_handlers(
      p->w, (WindowHandlers){.load = pa_load, .unload = pa_unload});
  window_set_background_color(p->w, GColorBlack);
  window_stack_push(p->w, true);
}
