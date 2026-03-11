#include "weather_utils.h"

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

int weather_utils_build_icon(const char *code) {
  if (!code || code[0] == '\0' || code[0] == ' ')
    return RESOURCE_ID_ENSOLEILLE_W;

  char c0 = code[0], c1 = code[1];
  if (c0 == 's' && c1 == 'd') return RESOURCE_ID_ENSOLEILLE_W;
  if (c0 == 'c' && c1 == 'n') return RESOURCE_ID_NUIT_CLAIRE_W;
  if (c0 == 'f' && c1 == 'd') return RESOURCE_ID_FAIBLES_PASSAGES_NUAGEUX_W;
  if (c0 == 'f' && c1 == 'n') return RESOURCE_ID_NUIT_BIEN_DEGAGEE_W;
  if (c0 == 'p' && c1 == 'd') return RESOURCE_ID_DEVELOPPEMENT_NUAGEUX_W;
  if (c0 == 'p' && c1 == 'n') return RESOURCE_ID_NUIT_AVEC_DEVELOPPEMENT_NUAGEUX_W;
  if (c0 == 'c' && c1 == 'l') return RESOURCE_ID_FORTEMENT_NUAGEUX_W;
  if (c0 == 'r' && c1 == 'a') return RESOURCE_ID_AVERSES_DE_PLUIE_FORTE_W;
  if (c0 == 'r' && c1 == 'n') return RESOURCE_ID_NUIT_AVEC_AVERSES_W;
  if (c0 == 't' && c1 == 'h') return RESOURCE_ID_FORTEMENT_ORAGEUX_W;
  if (c0 == 's' && c1 == 'n') return RESOURCE_ID_NEIGE_FORTE_W;
  if (c0 == 'f' && c1 == 'g') return RESOURCE_ID_BROUILLARD_W;

  return RESOURCE_ID_ENSOLEILLE_W;
}
