#include "weather_utils.h"

// Packed weekday abbreviations (3 chars each, NUL-separated, indexed by day*4)
static const char s_wd_fr[] = "DIM\0LUN\0MAR\0MER\0JEU\0VEN\0SAM";
static const char s_wd_en[] = "SUN\0MON\0TUE\0WED\0THU\0FRI\0SAT";
static const char s_wd_de[] = "SON\0MON\0DIE\0MIT\0DON\0FRE\0SAM";
static const char s_wd_es[] = "DOM\0LUN\0MAR\0MIE\0JUE\0VIE\0SAB";

const char *weather_utils_get_weekday_abbrev(const char *locale,
                                             int day_index) {
  if (day_index < 0 || day_index > 6)
    return "";
  const char *base = s_wd_en;
  if (locale && locale[0] == 'f' && locale[1] == 'r')
    base = s_wd_fr;
  else if (locale && locale[0] == 'd' && locale[1] == 'e')
    base = s_wd_de;
  else if (locale && locale[0] == 'e' && locale[1] == 's')
    base = s_wd_es;
  return base + day_index * 4;
}

int weather_utils_build_icon(const char *code) {
  if (!code || code[0] == '\0' || code[0] == ' ')
    return RESOURCE_ID_ENSOLEILLE_W;

  char c0 = code[0], c1 = code[1];
  if (c0 == 's' && c1 == 'd')
    return RESOURCE_ID_ENSOLEILLE_W;
  if (c0 == 'c' && c1 == 'n')
    return RESOURCE_ID_NUIT_CLAIRE_W;
  if (c0 == 'f' && c1 == 'd')
    return RESOURCE_ID_FAIBLES_PASSAGES_NUAGEUX_W;
  if (c0 == 'f' && c1 == 'n')
    return RESOURCE_ID_NUIT_BIEN_DEGAGEE_W;
  if (c0 == 'p' && c1 == 'd')
    return RESOURCE_ID_DEVELOPPEMENT_NUAGEUX_W;
  if (c0 == 'p' && c1 == 'n')
    return RESOURCE_ID_NUIT_AVEC_DEVELOPPEMENT_NUAGEUX_W;
  if (c0 == 'c' && c1 == 'l')
    return RESOURCE_ID_FORTEMENT_NUAGEUX_W;
  if (c0 == 'r' && c1 == 'a')
    return RESOURCE_ID_AVERSES_DE_PLUIE_FORTE_W;
  if (c0 == 'r' && c1 == 'n')
    return RESOURCE_ID_NUIT_AVEC_AVERSES_W;
  if (c0 == 't' && c1 == 'h')
    return RESOURCE_ID_FORTEMENT_ORAGEUX_W;
  if (c0 == 's' && c1 == 'n')
    return RESOURCE_ID_NEIGE_FORTE_W;
  if (c0 == 'f' && c1 == 'g')
    return RESOURCE_ID_BROUILLARD_W;

  return RESOURCE_ID_ENSOLEILLE_W;
}
