// force la météo sur l'émulateur
var b_force_internet = false;
var bFakeData = 0;
var bFakePosition = 0;

var myGoogleAPIKey = '';

var phone_bat = 100;

var KEY_CONFIG = 157;
var KEY_LAST_UPDATE = 158;

var bIsImperial;
var windSpeedUnit; // 'kmh' or 'ms' for metric mode

var currentCity;
var current_Latitude;
var current_Longitude;
var current_Last_update;
var current_dictionary;

var current_page = 0;
var cityIndex = 0;

var API_URL = "https://api.iopool.com/v1/pools";

var poolTemp = 0;
var poolPH = 0;
var poolORP = 0;


function convertMpsToMph(mps) {
  const conversionFactor = 2.23694;
  return Math.round(mps * conversionFactor);
}

function celsiusToFahrenheit(celsius) {
  return Math.round((celsius * 9 / 5) + 32);
}

function SendStatus(status) {
  var dictionary = {
    "KEY_STATUS": status,
  };

  Pebble.sendAppMessage(dictionary, function () {
  }, function () {
  });
}

// Weather fetch retry configuration
var weatherRetryCount = 0;
var weatherMaxRetries = 3;
var weatherRetryDelayMs = 10000; // 10 seconds between retries (3 retries in ~30s max)
var weatherRetryTimer = null;
var weatherXhrPending = false;

var xhrRequest = function (url, type, callback, errorCallback) {
  var xhr = new XMLHttpRequest();

  // Timeout after 15 seconds
  xhr.timeout = 15000;

  xhr.onload = function () {
    callback(this.responseText);
  };

  xhr.onerror = function (err) {
    console.error('XHR failed', err);
    if (errorCallback) errorCallback('network_error');
  };

  xhr.ontimeout = function () {
    console.error('XHR timeout after 15s');
    if (errorCallback) errorCallback('timeout');
  };

  xhr.open(type, url);
  xhr.send();
};

// WMO Weather Code to MET Norway symbol_code mapping
// WMO codes: https://open-meteo.com/en/docs (weather_code field)
// Maps to MET Norway icon names for compatibility with existing build_icon() in C
function wmoCodeToSymbolCode(wmoCode, isNight) {
  var dayNight = isNight ? '_night' : '_day';

  switch (wmoCode) {
    case 0:  // Clear sky
      return 'clearsky' + dayNight;
    case 1:  // Mainly clear
      return 'fair' + dayNight;
    case 2:  // Partly cloudy
      return 'partlycloudy' + dayNight;
    case 3:  // Overcast
      return 'cloudy';
    case 45: // Fog
    case 48: // Depositing rime fog
      return 'fog';
    case 51: // Drizzle: Light
      return 'lightrain';
    case 53: // Drizzle: Moderate
      return 'rain';
    case 55: // Drizzle: Dense
      return 'rain';
    case 56: // Freezing Drizzle: Light
      return 'sleet';
    case 57: // Freezing Drizzle: Dense
      return 'sleet';
    case 61: // Rain: Slight
      return 'lightrainshowers' + dayNight;
    case 63: // Rain: Moderate
      return 'rain';
    case 65: // Rain: Heavy
      return 'heavyrain';
    case 66: // Freezing Rain: Light
      return 'sleet';
    case 67: // Freezing Rain: Heavy
      return 'heavysleet';
    case 71: // Snow fall: Slight
      return 'lightsnow';
    case 73: // Snow fall: Moderate
      return 'snow';
    case 75: // Snow fall: Heavy
      return 'heavysnow';
    case 77: // Snow grains
      return 'snow';
    case 80: // Rain showers: Slight
      return 'lightrainshowers' + dayNight;
    case 81: // Rain showers: Moderate
      return 'rainshowers' + dayNight;
    case 82: // Rain showers: Violent
      return 'heavyrainshowers' + dayNight;
    case 85: // Snow showers: Slight
      return 'lightsnowshowers' + dayNight;
    case 86: // Snow showers: Heavy
      return 'heavysnowshowers' + dayNight;
    case 95: // Thunderstorm: Slight or moderate
      return 'rainandthunder';
    case 96: // Thunderstorm with slight hail
      return 'rainandthunder';
    case 99: // Thunderstorm with heavy hail
      return 'heavyrainandthunder';
    default:
      return 'partlycloudy' + dayNight;
  }
}

// Check if current hour is night time (between sunset and sunrise)
function isNightTime(hour) {
  // Approximate: night is between 21:00 and 6:00
  return hour >= 21 || hour < 6;
}

// Process Open-Meteo API response and convert to the same dictionary format as MET Norway
function processOpenMeteoResponse(responseText) {
  var json = JSON.parse(responseText);

  var hourly = json.hourly;
  var units = localStorage.getItem(152);

  // Current conditions (first hour)
  var currentTemp = hourly.temperature_2m[0];
  var currentHumidity = Math.round(hourly.relative_humidity_2m[0]);
  var currentWindSpeed = hourly.wind_gusts_10m[0]; // Wind gusts in km/h from API
  var currentWmoCode = hourly.weather_code[0];

  // Get current hour to determine day/night for icon
  var now = new Date();
  var currentHour = now.getHours();
  var isNight = isNightTime(currentHour);
  var icon = wmoCodeToSymbolCode(currentWmoCode, isNight);

  // Calculate min/max for next 24 hours
  var tmin = 1000;
  var tmax = -1000;
  for (var i = 0; i <= 24 && i < hourly.temperature_2m.length; i++) {
    var temp = hourly.temperature_2m[i];
    if (temp < tmin) tmin = temp;
    if (temp > tmax) tmax = temp;
  }

  var rTemperature = currentTemp;
  var humidity = currentHumidity;

  if (units == 1) {
    rTemperature = celsiusToFahrenheit(rTemperature);
    tmin = celsiusToFahrenheit(tmin);
    tmax = celsiusToFahrenheit(tmax);
  } else {
    rTemperature = Math.round(rTemperature);
    tmin = Math.round(tmin);
    tmax = Math.round(tmax);
  }

  var temperature = Math.round(rTemperature);
  tmax = Math.round(tmax);
  tmin = Math.round(tmin);

  var windSpeedUnit = localStorage.getItem(181) || 'kmh';
  var wind;
  if (units == 1) {
    // Convert km/h to mph
    wind = Math.round(currentWindSpeed * 0.621371);
  } else if (windSpeedUnit === 'ms') {
    // Convert km/h to m/s
    wind = Math.round(currentWindSpeed / 3.6);
  } else {
    // Keep in km/h
    wind = Math.round(currentWindSpeed);
  }

  // Hourly data extraction
  // Calculate offset to start from current hour (API returns data from midnight)
  var now = new Date();
  var currentHour = now.getHours();
  var hourOffset = currentHour; // Start from current hour in API data

  var hourlyTemperatures = {
    hour0: 0, hour3: 0, hour6: 0, hour9: 0, hour12: 0, hour15: 0, hour18: 0, hour21: 0, hour24: 0,
    hour27: 0, hour30: 0, hour33: 0, hour36: 0, hour39: 0, hour42: 0, hour45: 0, hour48: 0
  };
  var hourly_time = {
    hour0: 0, hour3: 0, hour6: 0, hour9: 0, hour12: 0, hour15: 0, hour18: 0, hour21: 0, hour24: 0,
    hour27: 0, hour30: 0, hour33: 0, hour36: 0, hour39: 0, hour42: 0, hour45: 0, hour48: 0
  };
  var hourly_icons = {
    hour0: "", hour3: "", hour6: "", hour9: "", hour12: "", hour15: "", hour18: "", hour21: "", hour24: "",
    hour27: "", hour30: "", hour33: "", hour36: "", hour39: "", hour42: "", hour45: "", hour48: ""
  };
  var hourlyWind = {
    hour0: "", hour3: "", hour6: "", hour9: "", hour12: "", hour15: "", hour18: "", hour21: "", hour24: "",
    hour27: "", hour30: "", hour33: "", hour36: "", hour39: "", hour42: "", hour45: "", hour48: ""
  };
  var hourlyRain = {};
  for (var ri = 0; ri < 48; ri++) { hourlyRain['hour' + ri] = 0; }
  var hourlyWmoCode = {};
  for (var wi = 0; wi < 16; wi++) { hourlyWmoCode['block' + wi] = 0; }

  var units_setting = localStorage.getItem(152);

  // Extract hourly data starting from current hour (extended to 48h)
  for (var j = 0; j <= 48; j++) {
    var apiIndex = hourOffset + j; // Index in API data (current hour + offset)
    if (apiIndex >= hourly.time.length) break;

    // Process every 3 hours for main forecast
    if ((j % 3) === 0) {
      // Calculate local hour from current time
      var localHour = (currentHour + j) % 24;
      hourly_time['hour' + j] = localHour;

      var tempI = hourly.temperature_2m[apiIndex];
      if (units_setting == 1) {
        tempI = celsiusToFahrenheit(tempI);
      } else {
        tempI = Math.round(tempI);
      }
      hourlyTemperatures['hour' + j] = Math.round(tempI);

      var windSpeedKmh = hourly.wind_gusts_10m[apiIndex]; // Use wind gusts
      var windValue;
      if (units_setting == 1) {
        windValue = Math.round(windSpeedKmh * 0.621371);
      } else if (windSpeedUnit === 'ms') {
        windValue = Math.round(windSpeedKmh / 3.6);
      } else {
        windValue = Math.round(windSpeedKmh);
      }
      hourlyWind['hour' + j] = windValue + "\n";

      // Icon for this hour
      var hourIsNight = isNightTime(localHour);
      hourly_icons['hour' + j] = wmoCodeToSymbolCode(hourly.weather_code[apiIndex], hourIsNight);

      // Store WMO code for this 3h block
      var blockIdx = j / 3;
      if (blockIdx < 16) {
        hourlyWmoCode['block' + blockIdx] = hourly.weather_code[apiIndex];
      }
    }

    // Precipitation for each hour (scaled same as MET Norway processing)
    if (j < 48) {
      hourlyRain['hour' + j] = Math.round((hourly.precipitation[apiIndex] || 0) * 20);
    }
  }

  // --- 3-day forecast data extraction using DAILY data ---
  var day_temps = ["", "", "", "", ""];
  var day_icons = ["", "", "", "", ""];
  var day_rains = ["", "", "", "", ""];
  var day_winds = ["", "", "", "", ""];

  var daily = json.daily;
  if (daily && daily.time && daily.time.length > 1) {
    var maxDays = Math.min(daily.time.length - 1, 5); // 5 days
    for (var d = 0; d < maxDays; d++) {
      var dayIdx = d + 1; // Skip today (index 0)
      if (dayIdx >= daily.time.length) break;

      var tempMax = daily.temperature_2m_max[dayIdx];
      var tempMin = daily.temperature_2m_min[dayIdx];
      var dayTemp = (tempMax + tempMin) / 2;
      var wmoCode = daily.weather_code[dayIdx];
      var rainSum = daily.precipitation_sum[dayIdx] || 0;
      var dayWindKmh = daily.wind_gusts_10m_max[dayIdx];

      // Convert temperature
      var dayTempConverted;
      if (units == 1) {
        dayTempConverted = celsiusToFahrenheit(dayTemp);
      } else {
        dayTempConverted = Math.round(dayTemp);
      }
      dayTempConverted = Math.round(dayTempConverted);

      // Convert wind
      var dayWind;
      if (units == 1) {
        dayWind = Math.round(dayWindKmh * 0.621371);
      } else if (windSpeedUnit === 'ms') {
        dayWind = Math.round(dayWindKmh / 3.6);
      } else {
        dayWind = Math.round(dayWindKmh);
      }

      day_temps[d] = dayTempConverted + "°";
      if (wmoCode === 3 && rainSum > 2) {
        day_icons[d] = 'rain';
      } else {
        day_icons[d] = wmoCodeToSymbolCode(wmoCode, false);
      }
      day_rains[d] = Math.round(rainSum) + "mm";
      if (units == 1) {
        day_winds[d] = dayWind + "mph";
      } else if (windSpeedUnit === 'ms') {
        day_winds[d] = dayWind + "m/s";
      } else {
        day_winds[d] = dayWind + "kmh";
      }
    }
  }

  // Calculate hours from current time (simpler and more reliable than API extraction)
  var now = new Date();
  var currentHour = now.getHours();
  var h0 = currentHour;
  var h1 = (currentHour + 3) % 24;
  var h2 = (currentHour + 6) % 24;
  var h3 = (currentHour + 9) % 24;

  if (units_setting == 1) {
    h0 = h0 % 12; if (h0 === 0) h0 = 12;
    h1 = h1 % 12; if (h1 === 0) h1 = 12;
    h2 = h2 % 12; if (h2 === 0) h2 = 12;
    h3 = h3 % 12; if (h3 === 0) h3 = 12;
  }

  // Split into 2 messages to fit in 512-byte inbox (real watch constraint).
  // Each message is chained via success callback.
  var dict1 = {
    "KEY_TEMPERATURE": temperature,
    "KEY_HUMIDITY": humidity,
    "KEY_WIND_SPEED": wind,
    "KEY_ICON": icon,
    "KEY_TMIN": tmin,
    "KEY_TMAX": tmax,
    "KEY_FORECAST_TEMP1": hourlyTemperatures.hour0,
    "KEY_FORECAST_TEMP2": hourlyTemperatures.hour3,
    "KEY_FORECAST_TEMP3": hourlyTemperatures.hour6,
    "KEY_FORECAST_TEMP4": hourlyTemperatures.hour9,
    "KEY_FORECAST_TEMP5": hourlyTemperatures.hour12,
    "KEY_FORECAST_H0": h0,
    "KEY_FORECAST_H1": h1,
    "KEY_FORECAST_H2": h2,
    "KEY_FORECAST_H3": h3,
    "KEY_FORECAST_WIND0": hourlyWind.hour0,
    "KEY_FORECAST_WIND1": hourlyWind.hour3,
    "KEY_FORECAST_WIND2": hourlyWind.hour6,
    "KEY_FORECAST_WIND3": hourlyWind.hour9,
    "KEY_FORECAST_RAIN1": hourlyRain.hour0,
    "KEY_FORECAST_RAIN11": hourlyRain.hour1,
    "KEY_FORECAST_RAIN12": hourlyRain.hour2,
    "KEY_FORECAST_RAIN2": hourlyRain.hour3,
    "KEY_FORECAST_RAIN21": hourlyRain.hour4,
    "KEY_FORECAST_RAIN22": hourlyRain.hour5,
    "KEY_FORECAST_RAIN3": hourlyRain.hour6,
    "KEY_FORECAST_RAIN31": hourlyRain.hour7,
    "KEY_FORECAST_RAIN32": hourlyRain.hour8,
    "KEY_FORECAST_RAIN4": hourlyRain.hour9,
    "KEY_FORECAST_RAIN41": hourlyRain.hour10,
    "KEY_FORECAST_RAIN42": hourlyRain.hour11,
    "KEY_LOCATION": "",
    "POOLTEMP": poolTemp * 10,
    "POOLPH": poolPH * 100,
    "POOLORP": poolORP,
  };

  var dict2 = {
    "KEY_FORECAST_ICON1": hourly_icons.hour3,
    "KEY_FORECAST_ICON2": hourly_icons.hour6,
    "KEY_FORECAST_ICON3": hourly_icons.hour9,
    "KEY_DAY1_TEMP": day_temps[0],
    "KEY_DAY1_ICON": day_icons[0],
    "KEY_DAY1_RAIN": day_rains[0],
    "KEY_DAY1_WIND": day_winds[0],
    "KEY_DAY2_TEMP": day_temps[1],
    "KEY_DAY2_ICON": day_icons[1],
    "KEY_DAY2_RAIN": day_rains[1],
    "KEY_DAY2_WIND": day_winds[1],
    "KEY_DAY3_TEMP": day_temps[2],
    "KEY_DAY3_ICON": day_icons[2],
    "KEY_DAY3_RAIN": day_rains[2],
    "KEY_DAY3_WIND": day_winds[2],
    "KEY_DAY4_TEMP": day_temps[3],
    "KEY_DAY4_ICON": day_icons[3],
    "KEY_DAY4_RAIN": day_rains[3],
    "KEY_DAY4_WIND": day_winds[3],
    "KEY_DAY5_TEMP": day_temps[4],
    "KEY_DAY5_ICON": day_icons[4],
    "KEY_DAY5_RAIN": day_rains[4],
    "KEY_DAY5_WIND": day_winds[4],
    "KEY_FORECAST_TEMP6": hourlyTemperatures.hour15,
    "KEY_FORECAST_TEMP7": hourlyTemperatures.hour18,
    "KEY_FORECAST_TEMP8": hourlyTemperatures.hour21,
    "KEY_FORECAST_TEMP9": hourlyTemperatures.hour24,
    "KEY_FORECAST_WIND4": hourlyWind.hour12,
    "KEY_FORECAST_WIND5": hourlyWind.hour15,
    "KEY_FORECAST_WIND6": hourlyWind.hour18,
    "KEY_FORECAST_WIND7": hourlyWind.hour21,
  };

  Pebble.sendAppMessage(dict1, function () {
    console.log("Open-Meteo msg1 sent");
    Pebble.sendAppMessage(dict2, function () {
      console.log("Open-Meteo weather info sent to Pebble successfully!");
    }, function () {
      console.log("Error sending Open-Meteo weather info msg2!");
    });
  }, function () {
    console.log("Error sending Open-Meteo weather info to Pebble!");
  });
}

// Build a small, offline fake forecast so emulator tests work without network.
function buildFakeResponse() {
  var timeseries = [];
  var baseTime = Date.UTC(2024, 11, 16, 9, 0, 0);

  // Icônes variées : matin clair → nuageux → pluie → orage → neige → brouillard → clair
  var icons = [
    "clearsky_day", "fair_day", "partlycloudy_day", "cloudy",
    "rainshowers_day", "rain", "heavyrain", "heavyrainandthunder",
    "sleet", "snow", "fog", "partlycloudy_day",
    "fair_day", "clearsky_day", "clearsky_night", "fair_night",
    "partlycloudy_night", "cloudy", "rainshowers_night", "lightrainshowers_night",
    "clearsky_night", "fair_night", "partlycloudy_night", "cloudy", "clearsky_night"
  ];

  for (var i = 0; i <= 24; i++) {
    var temp, rainAmount;

    if (i <= 12) {
      // Sur les 12 premières heures : courbe en U (commence haut, descend, remonte)
      var t = i / 12.0; // normaliser 0-1 sur 12h
      // Température : commence à 18°C, descend à 8°C à 6h, remonte à 18°C à 12h
      temp = 18 - 10 * Math.sin(Math.PI * t);

      // Pluie : cloche avec pic à 6h (milieu de 12h)
      rainAmount = 2.0 * Math.sin(Math.PI * t); // Max 2mm à t=0.5 (6h)
    } else {
      // Après 12h : stabiliser les valeurs
      temp = 18;
      rainAmount = 0;
    }

    // Vitesse du vent : varie légèrement
    var windSpeed = 2 + 3 * Math.sin(Math.PI * i / 12.0);

    timeseries.push({
      time: new Date(baseTime + (i * 3600000)).toISOString(),
      data: {
        instant: {
          details: {
            air_temperature: Math.round(temp * 10) / 10,
            relative_humidity: 50 + Math.round(10 * Math.sin(Math.PI * i / 12.0)),
            wind_speed: Math.round(windSpeed * 10) / 10,
            wind_from_direction: (180 + (i * 15)) % 360
          }
        },
        next_12_hours: {
          summary: { symbol_code: icons[i] },
          details: {}
        },
        next_1_hours: {
          summary: { symbol_code: icons[i] },
          details: { precipitation_amount: Math.round(rainAmount * 100) / 100 }
        },
        next_6_hours: {
          summary: { symbol_code: icons[i] },
          details: {
            air_temperature_max: Math.round(temp + 2),
            air_temperature_min: Math.round(temp - 2),
            precipitation_amount: Math.round(rainAmount * 6 * 100) / 100
          }
        }
      }
    });
  }

  return JSON.stringify({
    type: "Feature",
    geometry: { type: "Point", coordinates: [2.35, 48.85, 0] },
    properties: {
      meta: { updated_at: "2024-12-16T09:18:24Z" },
      timeseries: timeseries
    }
  });
}

// Shared weather parsing/sending logic.
function processWeatherResponse(responseText) {
  var responseFixed = responseText.replace(/3h/g, "hh");
  var jsonWeather = JSON.parse(responseFixed);

  var units = localStorage.getItem(152);

  var tmin = 1000;
  var tmax = -1000;
  for (var i = 0; i <= 24; i++) {
    var temp = jsonWeather.properties.timeseries[i].data.instant.details.air_temperature;
    if (temp < tmin) {
      tmin = temp;
    }
    if (temp > tmax) {
      tmax = temp;
    }
  }

  var rTemperature = jsonWeather.properties.timeseries[0].data.instant.details.air_temperature;
  var humidity = Math.round(jsonWeather.properties.timeseries[0].data.instant.details.relative_humidity);

  if (units == 1) {
    rTemperature = celsiusToFahrenheit(rTemperature);
    tmin = celsiusToFahrenheit(tmin);
    tmax = celsiusToFahrenheit(tmax);
  } else {
    rTemperature = Math.round(rTemperature);
    tmin = Math.round(tmin);
    tmax = Math.round(tmax);
  }

  var temperature = Math.round(rTemperature);
  tmax = Math.round(tmax);
  tmin = Math.round(tmin);
  var windSpeedMps = jsonWeather.properties.timeseries[0].data.instant.details.wind_speed;
  var windSpeedUnit = localStorage.getItem(181) || 'kmh';

  var wind;
  if (units == 1) {
    wind = convertMpsToMph(windSpeedMps);
  } else if (windSpeedUnit === 'ms') {
    wind = Math.round(windSpeedMps);
  } else {
    wind = Math.round(windSpeedMps * 3.6);
  }

  if (bFakeData == 1) {
    wind = 666;
    tmin = 20;
    tmax = 10;
    temperature = 28;
    humidity = 50;
  }

  var icon = jsonWeather.properties.timeseries[0].data.next_12_hours.summary.symbol_code;

  var hourlyTemperatures = {
    hour0: 0, hour3: 0, hour6: 0, hour9: 0, hour12: 0, hour15: 0, hour18: 0, hour21: 0, hour24: 0
  };
  var hourly_time = {
    hour0: 0, hour3: 0, hour6: 0, hour9: 0, hour12: 0, hour15: 0, hour18: 0, hour21: 0, hour24: 0
  };
  var hourly_icons = {
    hour0: "", hour3: "", hour6: "", hour9: "", hour12: "", hour15: "", hour18: "", hour21: "", hour24: ""
  };
  var hourlyWind = {
    hour0: "", hour3: "", hour6: "", hour9: "", hour12: "", hour15: "", hour18: "", hour21: "", hour24: ""
  };
  var hourlyRain = {
    hour0: 0, hour1: 0, hour2: 0, hour3: 0, hour4: 0, hour5: 0, hour6: 0, hour7: 0, hour8: 0, hour9: 0,
    hour10: 0, hour11: 0, hour12: 0, hour13: 0, hour14: 0, hour15: 0, hour16: 0, hour17: 0, hour18: 0,
    hour19: 0, hour20: 0, hour21: 0, hour22: 0, hour23: 0
  };

  var units_setting = localStorage.getItem(152);
  for (var j = 0; j <= 24; j++) {
    if ((j % 3) === 0) {
      var utcTimeString = jsonWeather.properties.timeseries[j].time;
      var utcDate = new Date(utcTimeString);
      var offsetMinutes2 = new Date().getTimezoneOffset();
      var localTime = new Date(utcDate.getTime() - (offsetMinutes2 * 60000));
      var localHour = localTime.getHours();
      hourly_time['hour' + j] = localHour;

      var tempI = jsonWeather.properties.timeseries[j].data.instant.details.air_temperature;
      if (units_setting == 1) {
        tempI = celsiusToFahrenheit(tempI);
      } else {
        tempI = Math.round(tempI);
      }
      hourlyTemperatures['hour' + j] = Math.round(tempI);

      var windSpeedMps = jsonWeather.properties.timeseries[j].data.instant.details.wind_speed;
      var humidityHour = Math.round(jsonWeather.properties.timeseries[j].data.instant.details.relative_humidity);
      var windValue;
      if (units_setting == 1) {
        windValue = convertMpsToMph(windSpeedMps);
      } else if (windSpeedUnit === 'ms') {
        windValue = Math.round(windSpeedMps);
      } else {
        windValue = Math.round(windSpeedMps * 3.6);
      }
      hourlyWind['hour' + j] = windValue + "\n";

      if (jsonWeather.properties.timeseries[j].data.next_1_hours && jsonWeather.properties.timeseries[j].data.next_1_hours.summary) {
        hourly_icons['hour' + j] = jsonWeather.properties.timeseries[j].data.next_1_hours.summary.symbol_code;
      } else if (jsonWeather.properties.timeseries[j].data.next_6_hours && jsonWeather.properties.timeseries[j].data.next_6_hours.summary) {
        hourly_icons['hour' + j] = jsonWeather.properties.timeseries[j].data.next_6_hours.summary.symbol_code;
      }
    }

    if (j < 24) {
      if (jsonWeather.properties.timeseries[j].data.next_1_hours && jsonWeather.properties.timeseries[j].data.next_1_hours.details) {
        hourlyRain['hour' + j] = jsonWeather.properties.timeseries[j].data.next_1_hours.details.precipitation_amount;
      }
      hourlyRain['hour' + j] = Math.round(hourlyRain['hour' + j] * 20);
    }
  }

  // Override with fake data for testing negative temperatures
  if (bFakeData == 1) {
    hourlyTemperatures.hour0 = -12;
    hourlyTemperatures.hour3 = -4;
    hourlyTemperatures.hour6 = 2;
    hourlyTemperatures.hour9 = 10;
    hourlyTemperatures.hour12 = 16;
  }

  // --- 3-day forecast data extraction ---
  var day_temps = ["", "", "", "", ""];
  var day_icons = ["", "", "", "", ""];
  var day_rains = ["", "", "", "", ""];
  var day_winds = ["", "", "", "", ""];

  // Day offsets: 24h (tomorrow), 48h (day after), 72h (3rd day), 96h (4th), 120h (5th) - at noon
  var dayOffsets = [24, 48, 72, 96, 120];

  for (var d = 0; d < 5; d++) {
    var idx = dayOffsets[d];
    if (jsonWeather.properties.timeseries.length > idx) {
      var dayData = jsonWeather.properties.timeseries[idx].data;

      // Temperature (use next_6_hours if available for min/max, otherwise instant)
      var dayTemp = 0;
      if (dayData.next_6_hours && dayData.next_6_hours.details) {
        var tMax = dayData.next_6_hours.details.air_temperature_max || dayData.instant.details.air_temperature;
        var tMin = dayData.next_6_hours.details.air_temperature_min || dayData.instant.details.air_temperature;
        dayTemp = (tMax + tMin) / 2;
      } else {
        dayTemp = dayData.instant.details.air_temperature;
      }
      if (units == 1) {
        dayTemp = celsiusToFahrenheit(dayTemp);
      } else {
        dayTemp = Math.round(dayTemp);
      }
      day_temps[d] = Math.round(dayTemp) + "°";

      // Icon
      if (dayData.next_12_hours && dayData.next_12_hours.summary) {
        day_icons[d] = dayData.next_12_hours.summary.symbol_code;
      } else if (dayData.next_6_hours && dayData.next_6_hours.summary) {
        day_icons[d] = dayData.next_6_hours.summary.symbol_code;
      } else if (dayData.next_1_hours && dayData.next_1_hours.summary) {
        day_icons[d] = dayData.next_1_hours.summary.symbol_code;
      }

      // Rain - sum over 6 hours
      var rainSum = 0;
      if (dayData.next_6_hours && dayData.next_6_hours.details && dayData.next_6_hours.details.precipitation_amount) {
        rainSum = dayData.next_6_hours.details.precipitation_amount;
      } else if (dayData.next_1_hours && dayData.next_1_hours.details && dayData.next_1_hours.details.precipitation_amount) {
        rainSum = dayData.next_1_hours.details.precipitation_amount * 6;
      }
      day_rains[d] = Math.round(rainSum) + "mm";

      // Wind
      var dayWindMps = dayData.instant.details.wind_speed;
      var dayWind;
      if (units == 1) {
        dayWind = convertMpsToMph(dayWindMps);
        day_winds[d] = dayWind + "mph";
      } else if (windSpeedUnit === 'ms') {
        dayWind = Math.round(dayWindMps);
        day_winds[d] = dayWind + "m/s";
      } else {
        dayWind = Math.round(dayWindMps * 3.6);
        day_winds[d] = dayWind + "kmh";
      }
    }
  }

  console.log("Hours being sent: H0=" + hourly_time.hour0 + " H1=" + hourly_time.hour3 + " H2=" + hourly_time.hour6 + " H3=" + hourly_time.hour9);
  console.log("Temps being sent: T1=" + hourlyTemperatures.hour0 + " T2=" + hourlyTemperatures.hour3 + " T3=" + hourlyTemperatures.hour6 + " T4=" + hourlyTemperatures.hour9 + " T5=" + hourlyTemperatures.hour12);

  // Calculate hours from current time (simpler and more reliable than API extraction)
  var now = new Date();
  var currentHour = now.getHours();
  var h0 = currentHour;
  var h1 = (currentHour + 3) % 24;
  var h2 = (currentHour + 6) % 24;
  var h3 = (currentHour + 9) % 24;

  if (units_setting == 1) {
    h0 = h0 % 12;
    if (h0 === 0) h0 = 12;
    h1 = h1 % 12;
    if (h1 === 0) h1 = 12;
    h2 = h2 % 12;
    if (h2 === 0) h2 = 12;
    h3 = h3 % 12;
    if (h3 === 0) h3 = 12;
  }

  // Split into 2 messages to fit in 512-byte inbox (real watch constraint).
  var wdict1 = {
    "KEY_TEMPERATURE": temperature,
    "KEY_HUMIDITY": humidity,
    "KEY_WIND_SPEED": wind,
    "KEY_ICON": icon,
    "KEY_TMIN": tmin,
    "KEY_TMAX": tmax,
    "KEY_FORECAST_TEMP1": hourlyTemperatures.hour0,
    "KEY_FORECAST_TEMP2": hourlyTemperatures.hour3,
    "KEY_FORECAST_TEMP3": hourlyTemperatures.hour6,
    "KEY_FORECAST_TEMP4": hourlyTemperatures.hour9,
    "KEY_FORECAST_TEMP5": hourlyTemperatures.hour12,
    "KEY_FORECAST_H0": h0,
    "KEY_FORECAST_H1": h1,
    "KEY_FORECAST_H2": h2,
    "KEY_FORECAST_H3": h3,
    "KEY_FORECAST_WIND0": hourlyWind.hour0,
    "KEY_FORECAST_WIND1": hourlyWind.hour3,
    "KEY_FORECAST_WIND2": hourlyWind.hour6,
    "KEY_FORECAST_WIND3": hourlyWind.hour9,
    "KEY_FORECAST_RAIN1": hourlyRain.hour0,
    "KEY_FORECAST_RAIN11": hourlyRain.hour1,
    "KEY_FORECAST_RAIN12": hourlyRain.hour2,
    "KEY_FORECAST_RAIN2": hourlyRain.hour3,
    "KEY_FORECAST_RAIN21": hourlyRain.hour4,
    "KEY_FORECAST_RAIN22": hourlyRain.hour5,
    "KEY_FORECAST_RAIN3": hourlyRain.hour6,
    "KEY_FORECAST_RAIN31": hourlyRain.hour7,
    "KEY_FORECAST_RAIN32": hourlyRain.hour8,
    "KEY_FORECAST_RAIN4": hourlyRain.hour9,
    "KEY_FORECAST_RAIN41": hourlyRain.hour10,
    "KEY_FORECAST_RAIN42": hourlyRain.hour11,
    "KEY_LOCATION": "",
    "POOLTEMP": poolTemp * 10,
    "POOLPH": poolPH * 100,
    "POOLORP": poolORP,
  };

  var wdict2 = {
    "KEY_FORECAST_ICON1": hourly_icons.hour3,
    "KEY_FORECAST_ICON2": hourly_icons.hour6,
    "KEY_FORECAST_ICON3": hourly_icons.hour9,
    "KEY_DAY1_TEMP": day_temps[0],
    "KEY_DAY1_ICON": day_icons[0],
    "KEY_DAY1_RAIN": day_rains[0],
    "KEY_DAY1_WIND": day_winds[0],
    "KEY_DAY2_TEMP": day_temps[1],
    "KEY_DAY2_ICON": day_icons[1],
    "KEY_DAY2_RAIN": day_rains[1],
    "KEY_DAY2_WIND": day_winds[1],
    "KEY_DAY3_TEMP": day_temps[2],
    "KEY_DAY3_ICON": day_icons[2],
    "KEY_DAY3_RAIN": day_rains[2],
    "KEY_DAY3_WIND": day_winds[2],
    "KEY_DAY4_TEMP": day_temps[3],
    "KEY_DAY4_ICON": day_icons[3],
    "KEY_DAY4_RAIN": day_rains[3],
    "KEY_DAY4_WIND": day_winds[3],
    "KEY_DAY5_TEMP": day_temps[4],
    "KEY_DAY5_ICON": day_icons[4],
    "KEY_DAY5_RAIN": day_rains[4],
    "KEY_DAY5_WIND": day_winds[4],
    "KEY_FORECAST_TEMP6": hourlyTemperatures.hour15,
    "KEY_FORECAST_TEMP7": hourlyTemperatures.hour18,
    "KEY_FORECAST_TEMP8": hourlyTemperatures.hour21,
    "KEY_FORECAST_TEMP9": hourlyTemperatures.hour24,
    "KEY_FORECAST_WIND4": hourlyWind.hour12,
    "KEY_FORECAST_WIND5": hourlyWind.hour15,
    "KEY_FORECAST_WIND6": hourlyWind.hour18,
    "KEY_FORECAST_WIND7": hourlyWind.hour21,
  };

  Pebble.sendAppMessage(wdict1, function () {
    console.log("MET Norway msg1 sent");
    Pebble.sendAppMessage(wdict2, function () {
      console.log("Weather info sent to Pebble successfully!");
    }, function () {
      console.log("Error sending weather info msg2!");
    });
  }, function () {
    console.log("Error sending weather info to Pebble!");
  });
}

function getIOPoolData() {

  var apiKey = localStorage.getItem(158);

  if (apiKey !== null) {

    var size = apiKey.length;

    if (size == 40) {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", API_URL, true);
      xhr.setRequestHeader("x-api-key", apiKey);
      xhr.onload = function () {


        if (xhr.status === 200) {
          try {
            var data = JSON.parse(xhr.responseText);
            if (data && data.length > 0 && data[0].latestMeasure) {
              poolTemp = Math.round(data[0].latestMeasure.temperature);
              poolPH = Math.round(data[0].latestMeasure.ph * 100) / 100;
              poolORP = Math.round(data[0].latestMeasure.orp);
              console.log("poolTemp");
              console.log(poolTemp);
              console.log("poolPH");
              console.log(poolPH);
              console.log("poolORP");
              console.log(poolORP);
              getForecast();
            } else {
              console.error("Données inattendues :", data);
            }
          } catch (e) {
            console.error("Erreur de parsing JSON :", e);
          }
        } else {
          console.error("Erreur HTTP :", xhr.status, xhr.statusText);
        }
      };
      xhr.onerror = function () {
        console.error("Erreur réseau");
      };
      xhr.send();
      console.log("fin");
    }
    else {
      getForecast();
      console.error("API key manquante");
    }

  } else {
    getForecast();
    console.error("API key manquante");
  }
}


// Called when weather fetch succeeds - reset retry state
function onWeatherFetchSuccess() {
  weatherRetryCount = 0;
  weatherXhrPending = false;
  if (weatherRetryTimer) {
    clearTimeout(weatherRetryTimer);
    weatherRetryTimer = null;
  }
  console.log("Weather fetch successful, retry count reset");
}

// Called when weather fetch fails - schedule retry if under limit
function onWeatherFetchError(reason) {
  weatherXhrPending = false;
  weatherRetryCount++;
  console.log("Weather fetch failed (" + reason + "), retry " + weatherRetryCount + "/" + weatherMaxRetries);

  if (weatherRetryCount < weatherMaxRetries) {
    console.log("Scheduling retry in " + weatherRetryDelayMs + "ms");
    weatherRetryTimer = setTimeout(function () {
      weatherRetryTimer = null;
      getForecast();
    }, weatherRetryDelayMs);
  } else {
    console.log("Max retries reached, giving up until next scheduled refresh");
    weatherRetryCount = 0; // Reset for next scheduled attempt
  }
}

function getForecast() {

  console.log("getForecast");

  // Prevent concurrent requests
  if (weatherXhrPending) {
    console.log("Weather XHR already pending, skipping");
    return;
  }

  if (bFakeData == 1) {
    console.log("Using offline fake weather sample");
    processWeatherResponse(buildFakeResponse());
    onWeatherFetchSuccess();
    return;
  }

  weatherXhrPending = true;

  // Check which API to use (default to Open-Meteo with AROME model for France)
  var weatherApi = localStorage.getItem(180) || 'openmeteo';
  console.log("Using weather API: " + weatherApi);

  if (weatherApi === 'openmeteo') {
    // Open-Meteo API with Météo-France AROME model (1.5km resolution, excellent for France)
    var urlOpenMeteo = 'https://api.open-meteo.com/v1/meteofrance?' +
      'latitude=' + current_Latitude + '&longitude=' + current_Longitude +
      '&hourly=temperature_2m,relative_humidity_2m,precipitation,weather_code,wind_gusts_10m' +
      '&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_sum,wind_gusts_10m_max' +
      '&forecast_days=6&timezone=auto';

    console.log(urlOpenMeteo);

    xhrRequest(urlOpenMeteo, 'GET',
      function (responseText) {
        try {
          processOpenMeteoResponse(responseText);
          onWeatherFetchSuccess();
        } catch (e) {
          console.error("Error processing Open-Meteo response: " + e);
          onWeatherFetchError('parse_error');
        }
      },
      onWeatherFetchError
    );
  } else {
    // MET Norway API (fallback)
    var coordinates = 'lat=' + current_Latitude + '&lon=' + current_Longitude;
    var urlWeatherRequest = 'https://api.met.no/weatherapi/locationforecast/2.0/complete?' + coordinates;

    console.log(urlWeatherRequest);

    xhrRequest(urlWeatherRequest, 'GET',
      function (responseText) {
        try {
          processWeatherResponse(responseText);
          onWeatherFetchSuccess();
        } catch (e) {
          console.error("Error processing MET Norway response: " + e);
          onWeatherFetchError('parse_error');
        }
      },
      onWeatherFetchError
    );
  }
}

function locationSuccess(pos) {

  current_Latitude = pos.coords.latitude;
  current_Longitude = pos.coords.longitude;

  localStorage.setItem(160, current_Latitude);
  localStorage.setItem(161, current_Longitude);

  console.log("location success");
  getIOPoolData();
}


function locationError(err) {
  console.log("Error requesting location!");

  current_Latitude = localStorage.getItem(160);
  current_Longitude = localStorage.getItem(161);
  console.log("GPS data saved");
  console.log(current_Latitude);
  console.log(current_Longitude);

  if (current_Latitude !== null && current_Longitude !== null) {
    const position = {
      coords: {
        latitude: current_Latitude,
        longitude: current_Longitude,
        altitude: null,
        accuracy: 10,
        altitudeAccuracy: null,
        heading: null,
        speed: null
      },
      timestamp: 1692448765123
    };
    locationSuccess(position);
  }

}


function getPosition() {

  console.log("Get position");
  if (bFakePosition == 0) {
    navigator.geolocation.getCurrentPosition(
      locationSuccess,
      locationError,
      { timeout: 15000, maximumAge: 120000 }
    );
  }
  else {
    const position = {
      coords: {
        latitude: 43.1380428,
        longitude: 5.7337657,
        altitude: null,
        accuracy: 10,
        altitudeAccuracy: null,
        heading: null,
        speed: null
      },
      timestamp: 1692448765123
    };
    locationSuccess(position);
  }
}

function getWeather() {
  console.log("getWeather !!");
  var gps = localStorage.getItem(150);
  var city = localStorage.getItem(151);
  getPosition();

}

// Execute a webhook POST request (triggered from C side via KEY_HUB_WEBHOOK)
function executeWebhook(index) {
  var url = localStorage.getItem('webhook_url_' + index);
  if (!url) {
    console.log("No webhook URL configured for index " + index);
    return;
  }

  var xhr = new XMLHttpRequest();
  xhr.timeout = 10000;
  xhr.onload = function () {
    console.log("Webhook sent successfully: " + xhr.status);
  };
  xhr.onerror = function () {
    console.error("Webhook request failed");
  };
  xhr.ontimeout = function () {
    console.error("Webhook request timed out");
  };
  xhr.open('POST', url);
  xhr.setRequestHeader('Content-Type', 'application/json');
  xhr.send(JSON.stringify({
    source: 'din-clean-app',
    timestamp: Date.now(),
    index: index
  }));
}

// ================================================================
// Stock Widget — Yahoo Finance fetch + AppMessage send
// ================================================================
var stockXhrPending = false;

// Default stock panels if none configured
var DEFAULT_STOCK_SYMBOLS = ['^DJI', 'EURCHF=X', 'BTC-USD'];
var DEFAULT_STOCK_NAMES = ['DJIA', 'EUR/CHF', 'BTC'];

function getStockConfig() {
  var symbols, names;
  try {
    symbols = JSON.parse(localStorage.getItem('stock_symbols'));
    names = JSON.parse(localStorage.getItem('stock_names'));
  } catch (e) { symbols = null; names = null; }
  if (!symbols || !symbols.length) {
    symbols = DEFAULT_STOCK_SYMBOLS;
    names = DEFAULT_STOCK_NAMES;
  }
  if (!names) names = symbols.slice();
  return { symbols: symbols, names: names };
}

// Format a price for the Pebble display (max 11 chars)
function formatStockPrice(price) {
  if (price >= 10000) return Math.round(price).toLocaleString('en-US');
  if (price >= 100) return price.toFixed(1);
  if (price >= 1) return price.toFixed(2);
  return price.toFixed(4);
}

// Sample an array to exactly n evenly-spaced points
function sampleArray(arr, n) {
  if (arr.length <= n) {
    // Pad or return as-is
    var out = arr.slice();
    while (out.length < n) out.push(out[out.length - 1]);
    return out;
  }
  var result = [];
  for (var i = 0; i < n; i++) {
    var idx = Math.round(i * (arr.length - 1) / (n - 1));
    result.push(arr[idx]);
  }
  return result;
}

// Normalize values to 0-100 range
function normalizeHistory(values) {
  var min = values[0], max = values[0];
  for (var i = 1; i < values.length; i++) {
    if (values[i] < min) min = values[i];
    if (values[i] > max) max = values[i];
  }
  var range = max - min;
  if (range < 0.0001) range = 1; // flat line → all 50
  var result = [];
  for (var j = 0; j < values.length; j++) {
    var v = Math.round((values[j] - min) / range * 100);
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    result.push(v);
  }
  return result;
}

// Build fake stock data for emulator testing
function buildFakeStockData() {
  return [
    { symbol: 'DJIA', price: '42,531', change: '+0.8%', positive: true,
      history: [30, 35, 42, 50, 48, 55, 62, 70, 78, 85], price_min: 41800, price_max: 43200 },
    { symbol: 'EUR/CHF', price: '0.9385', change: '-0.3%', positive: false,
      history: [80, 75, 70, 65, 60, 55, 50, 48, 45, 40], price_min: 0.930, price_max: 0.960 },
    { symbol: 'BTC', price: '97,500', change: '+2.1%', positive: true,
      history: [10, 20, 15, 30, 45, 40, 60, 55, 80, 95], price_min: 88000, price_max: 102000 }
  ];
}

// Send stock panels to watch via chained AppMessages
function sendStockPanels(panels) {
  if (!panels || panels.length === 0) return;

  // Send count first
  Pebble.sendAppMessage({ 'KEY_STOCK_COUNT': panels.length }, function () {
    console.log('Stock count sent: ' + panels.length);
    // Chain-send each panel
    sendStockPanel(panels, 0);
  }, function () {
    console.log('Error sending stock count');
  });
}

function sendStockPanel(panels, idx) {
  if (idx >= panels.length) {
    console.log('All stock panels sent successfully');
    stockXhrPending = false;
    return;
  }
  var p = panels[idx];
  var histStr = normalizeHistory(p.history).join(',');
  var priceMin = (p.price_min !== undefined) ? formatStockPrice(p.price_min) : '?';
  var priceMax = (p.price_max !== undefined) ? formatStockPrice(p.price_max) : '?';
  var dataStr = idx + '|' + p.symbol + '|' + p.price + '|' +
                (p.positive ? '+' : '') + p.change + '|' + histStr +
                '|' + priceMin + '|' + priceMax;

  Pebble.sendAppMessage({ 'KEY_STOCK_DATA': dataStr }, function () {
    console.log('Stock panel ' + idx + ' sent: ' + p.symbol);
    sendStockPanel(panels, idx + 1);
  }, function () {
    console.log('Error sending stock panel ' + idx);
    stockXhrPending = false;
  });
}

// Fetch stock data from Yahoo Finance for all configured symbols
function fetchStockData() {
  if (stockXhrPending) {
    console.log('Stock fetch already pending, skipping');
    return;
  }

  if (bFakeData == 1) {
    console.log('Using fake stock data');
    sendStockPanels(buildFakeStockData());
    return;
  }

  stockXhrPending = true;
  var config = getStockConfig();
  var panels = [];
  var remaining = config.symbols.length;

  for (var i = 0; i < config.symbols.length; i++) {
    (function (idx) {
      var symbol = config.symbols[idx];
      var displayName = config.names[idx] || symbol;
      // Always fetch 5 days of hourly data; variation is computed over the last 24h
      var url = 'https://query1.finance.yahoo.com/v8/finance/chart/' +
                encodeURIComponent(symbol) +
                '?range=5d&interval=1h';

      console.log('Fetching stock: ' + symbol + ' → ' + url);

      xhrRequest(url, 'GET', function (responseText) {
        try {
          var json = JSON.parse(responseText);
          var result = json.chart.result[0];
          var closes = result.indicators.quote[0].close;
          var timestamps = result.timestamp || [];

          // Filter out null values, keeping paired (timestamp, close)
          var validCloses = [];
          var validTs = [];
          for (var c = 0; c < closes.length; c++) {
            if (closes[c] !== null) {
              validCloses.push(closes[c]);
              validTs.push(timestamps[c] || 0);
            }
          }

          if (validCloses.length < 2) {
            console.log('Not enough data for ' + symbol);
            remaining--;
            if (remaining === 0) sendStockPanels(panels);
            return;
          }

          var sampled = sampleArray(validCloses, 10);
          var lastPrice = validCloses[validCloses.length - 1];
          var lastTs = validTs[validTs.length - 1];

          // Find the last point recorded on the previous trading day
          // (i.e. last point whose local calendar date is strictly before today's)
          var lastDate = new Date(lastTs * 1000);
          var lastDay = lastDate.toDateString();
          var baseIdx = 0;
          var foundPrevDay = false;
          for (var ti = validTs.length - 2; ti >= 0; ti--) {
            var d = new Date(validTs[ti] * 1000).toDateString();
            if (d !== lastDay) {
              // Walk forward to the last point of that previous day
              var prevDay = d;
              var prevDayLastIdx = ti;
              for (var tj = ti + 1; tj < validTs.length - 1; tj++) {
                if (new Date(validTs[tj] * 1000).toDateString() === prevDay) prevDayLastIdx = tj;
                else break;
              }
              baseIdx = prevDayLastIdx;
              foundPrevDay = true;
              break;
            }
          }
          // Fallback: use oldest available point
          if (!foundPrevDay) baseIdx = 0;
          var basePrice = validCloses[baseIdx];

          var changePct = ((lastPrice - basePrice) / basePrice * 100);
          var changeStr = (changePct >= 0 ? '+' : '') + changePct.toFixed(1) + '%';

          // Compute raw min/max of the valid history window
          var rawMin = validCloses[0], rawMax = validCloses[0];
          for (var vi = 1; vi < validCloses.length; vi++) {
            if (validCloses[vi] < rawMin) rawMin = validCloses[vi];
            if (validCloses[vi] > rawMax) rawMax = validCloses[vi];
          }

          panels[idx] = {
            symbol: displayName.substring(0, 9),
            price: formatStockPrice(lastPrice),
            change: changeStr,
            positive: changePct >= 0,
            history: sampled,
            price_min: rawMin,
            price_max: rawMax
          };
        } catch (e) {
          console.error('Error parsing stock data for ' + symbol + ': ' + e);
        }
        remaining--;
        if (remaining === 0) {
          // Remove null entries (failed fetches)
          var validPanels = [];
          for (var p = 0; p < panels.length; p++) {
            if (panels[p]) validPanels.push(panels[p]);
          }
          sendStockPanels(validPanels);
        }
      }, function (err) {
        console.error('Stock fetch failed for ' + symbol + ': ' + err);
        remaining--;
        if (remaining === 0) {
          var validPanels = [];
          for (var p = 0; p < panels.length; p++) {
            if (panels[p]) validPanels.push(panels[p]);
          }
          if (validPanels.length > 0) sendStockPanels(validPanels);
          else stockXhrPending = false;
        }
      });
    })(i);
  }
}

// Listen for when the app is opened
Pebble.addEventListener('ready',
  function () {
    console.log("JS: ready", Date.now());
    try {
      var nowSeconds = Math.floor(Date.now() / 1000);
      Pebble.sendAppMessage({ 200: nowSeconds }, function () {
        console.log('JS: ready message sent');
      }, function (err) {
        console.log('JS: ready send failed', err);
      });
      console.log('JS: ready message queued');
    } catch (err) {
      console.error('JS: ready send threw', err && err.message ? err.message : err);
    }

    console.log("PebbleKit JS ready");

    // Auto-trigger one weather fetch on startup
    try {
      setTimeout(function () {
        console.log('Auto weather fetch on startup');
        getWeather();
      }, 500);
      // Fetch stock data after weather (delayed to avoid message collision)
      setTimeout(function () {
        console.log('Auto stock fetch on startup');
        fetchStockData();
      }, 5000);
      console.log('JS: startup timer scheduled');
    } catch (err) {
      console.error('JS: startup timer failed', err && err.message ? err.message : err);
    }
  }
);



Pebble.addEventListener('appmessage',
  function (e) {
    // Check for webhook trigger from C side
    if (e.payload && e.payload['KEY_HUB_WEBHOOK'] !== undefined) {
      var webhookIndex = e.payload['KEY_HUB_WEBHOOK'];
      executeWebhook(webhookIndex);
      return;
    }

    if ((navigator.onLine) || (b_force_internet)) {
      console.log("Appel météo !!");
      getWeather();
      fetchStockData();
    }
  }
);

Pebble.addEventListener('showConfiguration', function () {

  var url = 'https://sichroteph.github.io/Din-Clean-App/config/index.html?v=' + Date.now();
  Pebble.openURL(url);
});


Pebble.addEventListener('webviewclosed', function (e) {
  try {
    if (!e.response || e.response === 'CANCELLED') {
      console.log('Webview closed without submitting config (response: ' + e.response + ')');
      return;
    }
    var configData = JSON.parse(decodeURIComponent(e.response));
    console.log('Configuration page returned: ' + JSON.stringify(configData));

    var input_iopool_token = configData['input_iopool_token'];
    var radio_units = configData['radio_units'];
    var radio_refresh = configData['radio_refresh'];
    var toggle_vibration = configData['toggle_vibration'];
    var toggle_bt = configData['toggle_bt'];
    var color_right_back = configData['color_right_back'];
    var color_left_back = configData['color_left_back'];

    var dict = {};

    // Weather API selection (Open-Meteo or MET Norway)
    var weather_api = configData['weather_api'] || 'openmeteo';
    localStorage.setItem(180, weather_api);
    console.log("Weather API set to: " + weather_api);

    localStorage.setItem(152, radio_units ? 0 : 1); // 0=metric (no Fahrenheit), 1=imperial (Fahrenheit)
    localStorage.setItem(158, input_iopool_token);

    // Wind speed unit for metric mode (kmh or ms)
    var wind_speed_unit = configData['wind_speed_unit'] || 'kmh';
    localStorage.setItem(181, wind_speed_unit);
    console.log("Wind speed unit set to: " + wind_speed_unit);

    // Encoding: 1 = on/true/metric/30min, 2 = off/false/imperial/15min
    // Never send 0 — Pebble SDK silently drops zero-value integer entries
    dict['KEY_RADIO_UNITS'] = radio_units ? 2 : 1;       // 2=metric, 1=imperial
    dict['KEY_RADIO_REFRESH'] = radio_refresh ? 1 : 2;   // 1=30min, 2=15min
    dict['KEY_TOGGLE_VIBRATION'] = toggle_vibration ? 1 : 2; // 1=on, 2=off
    dict['KEY_TOGGLE_BT'] = toggle_bt ? 1 : 2;           // 1=on, 2=off

    function safeColorComponent(hex, start, end) {
      if (typeof hex === 'string' && hex.length >= end) {
        return parseInt(hex.substring(start, end), 16);
      }
      return 0;
    }

    dict['KEY_COLOR_RIGHT_BACK_R'] = safeColorComponent(color_right_back, 2, 4);
    dict['KEY_COLOR_RIGHT_BACK_G'] = safeColorComponent(color_right_back, 4, 6);
    dict['KEY_COLOR_RIGHT_BACK_B'] = safeColorComponent(color_right_back, 6, 8);

    dict['KEY_COLOR_LEFT_BACK_R'] = safeColorComponent(color_left_back, 2, 4);
    dict['KEY_COLOR_LEFT_BACK_G'] = safeColorComponent(color_left_back, 4, 6);
    dict['KEY_COLOR_LEFT_BACK_B'] = safeColorComponent(color_left_back, 6, 8);

    // --- Hub configuration ---
    var hub_timeout = parseInt(configData['hub_timeout']) || 30;
    var hub_anim = (configData['hub_anim'] !== undefined) ? parseInt(configData['hub_anim']) : 1;
    var hub_btn_up = (configData['hub_btn_up'] !== undefined) ? parseInt(configData['hub_btn_up']) : 1;  // 0=menu, 1=widgets
    var hub_btn_down = (configData['hub_btn_down'] !== undefined) ? parseInt(configData['hub_btn_down']) : 0; // 0=menu, 1=widgets

    // Long press: encode as (type << 4) | data
    // type: 0=pseudoapp, 1=webhook
    var hub_lp_up = parseInt(configData['hub_lp_up']) || 0x10; // webhook:0
    var hub_lp_down = parseInt(configData['hub_lp_down']) || 0x00; // pseudoapp:stopwatch
    var hub_lp_select = parseInt(configData['hub_lp_select']) || 0x01; // pseudoapp:timer

    // Views: comma-separated enabled view IDs
    var hub_views = configData['hub_views'] || '0,1,2,3';

    dict['KEY_HUB_TIMEOUT'] = hub_timeout;
    dict['KEY_HUB_ANIM'] = (hub_anim === 1) ? 1 : 2;          // 1=on, 2=off
    dict['KEY_HUB_BTN_UP'] = (hub_btn_up === 1) ? 1 : 2;      // 1=widgets, 2=menu
    dict['KEY_HUB_BTN_DOWN'] = (hub_btn_down === 1) ? 1 : 2;  // 1=widgets, 2=menu
    dict['KEY_HUB_LP_UP'] = hub_lp_up;
    dict['KEY_HUB_LP_DOWN'] = hub_lp_down;
    dict['KEY_HUB_LP_SELECT'] = hub_lp_select;
    dict['KEY_HUB_VIEWS'] = hub_views;

    // Custom menu/widget data
    var hub_menu_up = configData['hub_menu_up'] || '';
    var hub_menu_down = configData['hub_menu_down'] || '';
    var hub_widgets_up = configData['hub_widgets_up'] || '0,1';
    var hub_widgets_down = configData['hub_widgets_down'] || '0,1';

    dict['KEY_HUB_MENU_UP'] = hub_menu_up;
    dict['KEY_HUB_MENU_DOWN'] = hub_menu_down;
    dict['KEY_HUB_WIDGETS_UP'] = hub_widgets_up;
    dict['KEY_HUB_WIDGETS_DOWN'] = hub_widgets_down;

    // Webhook URLs (stored in JS localStorage only, keyed by index)
    var webhookUrlsStr = configData['webhook_urls'];
    if (webhookUrlsStr) {
      try {
        var webhookUrls = JSON.parse(webhookUrlsStr);
        for (var k in webhookUrls) {
          if (webhookUrls.hasOwnProperty(k)) {
            localStorage.setItem('webhook_url_' + k, webhookUrls[k]);
          }
        }
      } catch (e) {
        console.log('Failed to parse webhook_urls');
      }
    }
    // Backward compat: single webhook_url from old config
    var webhook_url = configData['webhook_url'];
    if (webhook_url && !webhookUrlsStr) {
      localStorage.setItem('webhook_url_0', webhook_url);
    }

    // Stock configuration (stored in JS localStorage only)
    var stock_symbols_str = configData['stock_symbols'];
    var stock_names_str = configData['stock_names'];
    var stock_period = configData['stock_period'];
    if (stock_symbols_str) localStorage.setItem('stock_symbols', stock_symbols_str);
    if (stock_names_str) localStorage.setItem('stock_names', stock_names_str);
    if (stock_period) localStorage.setItem('stock_period', stock_period);

    console.log('[CFG] Sending dict keys: ' + Object.keys(dict).join(', '));
    console.log('[CFG] KEY_RADIO_UNITS=' + dict['KEY_RADIO_UNITS'] + ' KEY_HUB_TIMEOUT=' + dict['KEY_HUB_TIMEOUT'] + ' KEY_HUB_WIDGETS_UP=' + dict['KEY_HUB_WIDGETS_UP']);
    Pebble.sendAppMessage(dict, function () {
      // Refresh weather data after configuration changes (e.g., API provider, units)
      console.log("Configuration sent successfully, fetching weather data");
      setTimeout(function () {
        getWeather();
      }, 500);
      // Refresh stock data after configuration changes
      setTimeout(function () {
        fetchStockData();
      }, 3000);
    }, function () {
      console.log("Failed to send configuration");
    });
  } catch (err) {
    console.error('webviewclosed error: ' + (err && err.message ? err.message : err));
  }
});
