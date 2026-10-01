# Calendar for Office 365 – Privacy Policy

_Last updated: October 1, 2026_

**Short version: the developer receives none of your data. There is no server.**

## What the app accesses

When you sign in, you allow the app to use the following Microsoft Graph permissions on your behalf:

| Permission | Why |
|---|---|
| `Calendars.Read` | Read your calendars and events to show them on the watch |
| `offline_access` | Stay signed in without entering a code every time |
| `Calendars.ReadWrite` | **Only if you turn on "Allow moving".** Change the start and end time of your own events when you move them from the watch |

The app reads event title, time, location, description preview, status, organizer and attendee information. Attendee information is only used to decide whether an event may be moved; it is not shown or stored.

## Where your data goes

```
Microsoft Graph  →  Pebble app on your phone  →  your watch
```

- Your calendar data is fetched directly from Microsoft by the app on your phone and sent to your watch over Bluetooth.
- **No developer server, no analytics, no advertising, no tracking.** The developer cannot see your account, your events or your sign-in.
- Your sign-in tokens are stored only in the app's storage on your phone.

## What is stored

| Where | What | How long |
|---|---|---|
| Phone (app storage) | Sign-in tokens, settings, list of your calendar names, descriptions of recently shown events (max. 80) | Until you sign out or uninstall the app |
| Watch | Settings and today's and tomorrow's events (title, time, location) for offline use | Until the next update, sign-out or uninstall |

Event descriptions are kept on the watch only while you view them.

## Changes to your calendar

The app never creates, deletes, accepts or declines events. It changes an event only when you turn on "Allow moving" and confirm a move on the watch. If the event has attendees, Microsoft sends them the usual update.

## Revoking access

- Turn on **Sign out** in the app's settings to delete the tokens on your phone.
- To revoke access completely: personal accounts at <https://account.live.com/consent/Manage>, work and school accounts at <https://myapps.microsoft.com> or via your IT department.
- Uninstalling the app deletes all data stored by it on the phone and the watch.

## Third parties

Data is exchanged only with Microsoft (sign-in and Microsoft Graph). Microsoft's privacy statement applies: <https://privacy.microsoft.com>. The Pebble app on your phone transfers the data to the watch.

## Contact

Questions: kasser@mk-cs.at
