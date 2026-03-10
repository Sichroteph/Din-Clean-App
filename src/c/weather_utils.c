#include "weather_utils.h"

#include <string.h>

// Abbreviated weekday names (3 chars)
static const char *const s_weekday_abbrev_fr[] = {"DIM", "LUN", "MAR", "MER",
                                                  "JEU", "VEN", "SAM"};
static const char *const s_weekday_abbrev_en[] = {"SUN", "MON", "TUE", "WED",
                                                  "THU", "FRI", "SAT"};
static const char *const s_weekday_abbrev_de[] = {"SON", "MON", "DIE", "MIT",
                                                  "DON", "FRE", "SAM"};
static const char *const s_weekday_abbrev_es[] = {"DOM", "LUN", "MAR", "MIE",
                                                  "JUE", "VIE", "SAB"};

const char *weather_utils_get_weekday_abbrev(const char *locale,
                                             int day_index) {
  if (day_index < 0 || day_index > 6) {
    return "";
  }

  if (locale && locale[0] == 'f' && locale[1] == 'r') {
    return s_weekday_abbrev_fr[day_index];
  }
  if (locale && locale[0] == 'd' && locale[1] == 'e') {
    return s_weekday_abbrev_de[day_index];
  }
  if (locale && locale[0] == 'e' && locale[1] == 's') {
    return s_weekday_abbrev_es[day_index];
  }
  return s_weekday_abbrev_en[day_index];
}

int weather_utils_build_icon(const char *text_icon, bool is_bw_icon) {
  (void)is_bw_icon; // Always use B&W icons

  if (!text_icon || text_icon[0] == '\0' || text_icon[0] == ' ') {
    return RESOURCE_ID_ENSOLEILLE_W;
  }

  // Compact lookup table: exact matches
  static const struct {
    const char *name;
    uint16_t id;
  } exact[] = {
      {"clearsky_day", RESOURCE_ID_ENSOLEILLE_W},
      {"clearsky_night", RESOURCE_ID_NUIT_CLAIRE_W},
      {"fair_day", RESOURCE_ID_FAIBLES_PASSAGES_NUAGEUX_W},
      {"fair_polartwilight", RESOURCE_ID_FAIBLES_PASSAGES_NUAGEUX_W},
      {"fair_night", RESOURCE_ID_NUIT_BIEN_DEGAGEE_W},
      {"wind", RESOURCE_ID_WIND},
      {"partlycloudy_night", RESOURCE_ID_NUIT_AVEC_DEVELOPPEMENT_NUAGEUX_W},
      {"cloudy", RESOURCE_ID_FORTEMENT_NUAGEUX_W},
      {"rainshowers_night", RESOURCE_ID_NUIT_AVEC_AVERSES_W},
      {"fog", RESOURCE_ID_BROUILLARD_W},
  };
  for (unsigned i = 0; i < sizeof(exact) / sizeof(exact[0]); i++) {
    if (strcmp(text_icon, exact[i].name) == 0)
      return exact[i].id;
  }

  // Prefix / substring matches (order matters: check before generic "rain")
  if (strncmp(text_icon, "partlycloudy_", 13) == 0)
    return RESOURCE_ID_DEVELOPPEMENT_NUAGEUX_W;
  if (strstr(text_icon, "thunder") != NULL)
    return RESOURCE_ID_FORTEMENT_ORAGEUX_W;
  if (strstr(text_icon, "snow") != NULL || strstr(text_icon, "sleet") != NULL)
    return RESOURCE_ID_NEIGE_FORTE_W;
  if (strstr(text_icon, "rain") != NULL)
    return RESOURCE_ID_AVERSES_DE_PLUIE_FORTE_W;

  return RESOURCE_ID_BT;
}
