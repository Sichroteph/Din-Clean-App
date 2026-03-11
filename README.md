# Din Clean App

A Pebble application that complements the default firmware by adding customizable menus, widgets, and shortcuts to a watchface-like interface. It runs as an app rather than a watchface, which gives it access to all four hardware buttons. The firmware continues to run normally behind it, and the user can exit at any time to access other applications.

![Main screen](docs/images/main_screen.png)
<!-- TODO: screenshot of the main time screen with icon bar -->

## Why an app instead of a watchface

On Pebble, watchfaces can only react to time ticks and wrist shakes. They have no access to button presses (except back, which is reserved by the system). By running as an app, Din Clean can use all four buttons to offer menus, widgets, and shortcuts alongside the usual time display.

The watch shows time and weather exactly like a watchface would, but short-presses, long-presses, and the select button all become available. On Aplite in particular, the select button has never been usable from a watchface context, so this is new.

This does not replace the firmware or other Pebble apps. It covers a good part of everyday needs (time, weather, quick tools, shortcuts), but users will still want to leave the app regularly to use other applications.

## Recommended usage

The app is designed to be easy to leave and come back to. Three quick presses on the back button exit the app immediately.

A good setup is to use TikTok as the default watchface, because it launches almost instantly after exiting an app. From TikTok (or any watchface that supports it), configure a long-press shortcut to relaunch Din Clean App. This way, the round trip is fast: back-back-back to exit, pick another app if needed, then long-press to return. In practice, this covers most daily use without friction.

## Main screen

The primary screen shows time with four large bitmap digits arranged in a 2x2 grid. The typography is inspired by the Din Time watchface, using a custom Clearview-based font for maximum readability.

Below and around the digits, an information bar displays:

- Day of the week and date (localized in French, English, German, Spanish)
- Current temperature and weather icon
- Min/max temperatures for the next 24 hours
- Humidity percentage
- Wind speed (km/h, m/s, or mph)
- Bluetooth connection status
- Quiet time indicator

![Icon bar detail](docs/images/icon_bar.png)
<!-- TODO: screenshot closeup showing the icon bar details -->

## Button mapping

### Short press

| Button | Action |
|--------|--------|
| Back | Returns to main view if on an alternate view. On the main view: first press wakes the screen, second press shows an exit hint, third press exits the app. |
| Up | Opens the up-button content: either a custom menu or a widget screen, depending on configuration. |
| Down | Opens the down-button content: menu or widgets, same as up but independently configured. |
| Select | Cycles through the configured views (main time, weather detail, date/battery info). |

### Long press (400ms)

Each of the three right-side buttons (up, down, select) can be assigned a long-press action:

- Launch a pseudo-application (stopwatch, timer, or alarm)
- Execute a webhook (HTTP POST to a user-defined URL)

The select button supporting long-press is unusual for Pebble, and was not possible in a watchface context on Aplite.

## Alternate views

Pressing select cycles through up to four views. Each view can be toggled on or off from the configuration page.

### Weather detail view

Shows the current weather icon, temperature, min/max, wind speed, humidity, and the last data refresh time. Displays "Offline" if the data is stale.

![Weather view](docs/images/view_weather.png)
<!-- TODO: screenshot of the weather detail view -->

### Date and battery view

Shows a battery gauge, and optionally a countdown to a target date (displayed as "J-X" followed by a custom label).

![Date view](docs/images/view_date.png)
<!-- TODO: screenshot of the date/battery view, ideally with a countdown visible -->

## Menus

When a button (up or down) is configured in menu mode, pressing it opens a hierarchical menu. The menu supports up to 3 levels of depth, with folders, pseudo-app launchers, and webhook triggers as item types.

Menu navigation is done with up/down to browse and select to enter a folder or execute an action. Pressing back returns to the parent level or closes the menu at the root.

![Menu](docs/images/menu.png)
<!-- TODO: screenshot of a custom menu with a few items -->

## Widgets

When a button is configured in widget mode, pressing it opens a full-screen widget viewer. Multiple widgets can be assigned to the same button, and the user navigates between them with up/down. Select cycles through the pages of the current widget.

### Stocks

Displays up to 5 stock panels, each showing symbol, current price, percentage change, and a sparkline chart with 8 data points. The chart includes dotted grid lines and min/max price labels. Data comes from Yahoo Finance, with configurable period (1 week, 1 month, or 3 months). Symbols can be entered manually or picked from presets (DJIA, S&P 500, NASDAQ, BTC, Gold, EUR/USD, etc.).

On Aplite, the stock widget is disabled to save memory.

![Stocks widget](docs/images/widget_stocks.png)
<!-- TODO: screenshot of a stock panel with graph -->

### Hourly weather

Shows a 24-hour forecast split into two pages (0-12h and 12-24h). Each page displays a temperature curve with data points, rain bars (dithered), wind speeds, and hour labels. Dotted reference lines subdivide the graph area.

![Hourly weather widget](docs/images/widget_hourly.png)
<!-- TODO: screenshot of the hourly weather widget -->

### Daily weather

Shows up to 5 days of forecast, two days per page. Each day row includes the weekday name, a weather icon, temperature, rain amount, and wind speed.

![Daily weather widget](docs/images/widget_daily.png)
<!-- TODO: screenshot of the daily weather widget -->

## Pseudo-applications

Three small apps are built in and can be launched from long-press actions or from menu items.

### Stopwatch

- Select: start/stop
- Up/Down: reset (when stopped)
- Display: MM:SS, updated every second while running

### Timer

- Up/Down: adjust duration in minutes (1-99) when stopped
- Select: start the countdown
- Display: remaining time in MM:SS
- When the timer expires, the watch vibrates repeatedly for about 20 seconds
- Any button press dismisses the alarm

### Alarm

- Select: toggles between editing hours, editing minutes, and arming
- Up/Down: adjust the selected field (hours by 1, minutes by 5)
- When armed, the title changes to "ARMED"
- Triggers once daily at the set time with a vibration sequence
- Arming and firing happen in the background even if the pseudo-app screen is closed

All three pseudo-apps keep their state as long as the application is running. The timer and alarm continue to count down in the background after leaving their screen.

![Pseudo-app](docs/images/pseudoapp.png)
<!-- TODO: screenshot of the timer or stopwatch in use -->

## Webhooks

Webhooks send an HTTP POST request to a URL configured in the settings page. The payload contains a JSON object with a source identifier, a timestamp, and the webhook index. Execution triggers a vibration and a brief "Action sent!" overlay on screen. Timeout is 10 seconds.

This can be used to integrate with home automation systems, IFTTT, or any HTTP endpoint.

## Weather data

Two weather API providers are supported, both free and requiring no API key:

- Open-Meteo with the Meteo-France AROME model (1.5 km resolution, best for France)
- MET Norway (worldwide coverage, used as fallback)

Data collected includes current conditions, a 48-hour hourly forecast (temperature, rain, wind), and a 5-day daily forecast with icons. The refresh interval is configurable (30 or 60 minutes). If a request fails, the app retries up to 3 times with 10-second intervals. Stale data remains displayed for up to one hour with an "Offline" indicator.

## IOPool integration

If an IOPool API token is provided in the settings, pool monitoring data (temperature, pH, ORP) is fetched alongside each weather update and displayed on the date view.

## Inactivity timeout

An optional timeout (0-300 seconds, configurable) automatically returns the watch to the main time view and closes any open menu, widget, or pseudo-app after a period of inactivity. Setting 0 disables the timeout. Any button press resets the timer.

## Back button behavior

Since this is an app and not a watchface, the back button exists and needs handling. Accidental exits are prevented with a triple-press mechanism:

1. First press: the screen wakes up, nothing visible happens
2. Second press (within 2 seconds): a "Press < to exit" hint appears with a short vibration
3. Third press (within 2 seconds): the app exits

If the user is on an alternate view, back returns to the main time screen first, then the exit sequence starts from there.

## Configuration

Settings are managed through a web page hosted on GitHub Pages, opened from the Pebble phone app. The configuration covers:

- Button assignments (menu or widgets for up/down)
- Long-press actions for all three buttons (pseudo-app or webhook, with URL)
- Menu tree construction (folders, items, up to 3 levels)
- Widget selection and ordering
- View toggles (weather detail, date/battery)
- Countdown target date and label
- Stock symbols, period, and preset picker
- Weather API provider and refresh interval
- Inactivity timeout duration
- Vibration pattern (short, long, urgent)
- Animated window transitions toggle

## Platforms

Compatible with all Pebble SDK 3 platforms:

- Aplite (Pebble, Pebble Steel) -- 144x168, black and white, 24 KB memory limit
- Basalt (Pebble Time, Pebble Time Steel) -- forced black and white rendering
- Diorite (Pebble 2)

The stock widget is conditionally disabled on Aplite to stay within the 24 KB memory constraint. All rendering is strictly monochrome regardless of hardware capabilities.

## Building

Requires the Pebble SDK 3.

```
pebble build
pebble install --phone <phone_ip>
```

## License

This project is not currently under a specific open-source license. Contact the author for usage terms.
