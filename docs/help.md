# Calendar for Office 365 – Help

Calendar for Office 365 shows your Microsoft 365 / Outlook calendar on your Pebble, one day at a time. It is not an official Microsoft product.

## Getting started

1. Install the app and open it on the watch.
2. The watch shows an address and a code. On a PC or phone open **microsoft.com/devicelogin**, enter the code and sign in with your Microsoft account.
3. **Keep the app open on the watch until your events appear.** The phone part of a Pebble app only runs while the app is open on the watch.

Microsoft may show the app as "unverified". That is expected for apps from independent developers; the app only asks for permission to read your calendar (and, if you turn on "Allow moving", to change your own events).

## Using the app

| Action | Buttons | Touch (Pebble Time 2) |
|---|---|---|
| Select an event | Up / Down (the first press only shows the selection) | – |
| Open event details | Select | Tap the event |
| Next / previous day | Long-press Down / Up | Swipe left / right |
| Scroll | Up / Down | Swipe up / down |
| Close the details | Back | Swipe right |
| Exit the app | Back | – |

- The top bar always shows the date of the displayed day. A status line appears below it only when needed ("Updating...", "Offline - as of HH:MM", error messages).
- Events are refreshed **once each time you open the app**. Today and tomorrow are stored on the watch and are available offline; other days (60 days back, 90 days ahead) are loaded from the phone.
- Smooth scrolling and tapping use the watch's **touch navigation**. Turn it on in the watch's system settings if they do not work.
- **Event description:** the details show the description from Outlook. It is loaded from the phone when you open the details. Without the phone nearby the watch shows "The description needs the phone nearby."

## Settings

Open the app's settings in the Pebble app on your phone.

| Setting | What it does |
|---|---|
| Calendars | Which calendars to show (default: your default calendar) |
| Show all-day events | Show or hide all-day events |
| Hide declined events | Hide events you declined |
| Gray out past events | Off = all events have the same color |
| Touch control | Off = no swiping/tapping, change the day with long presses. Saves battery. |
| Close automatically | Exit the app after 2, 5 or 10 minutes without use, or never. Saves battery. |
| Language | Automatic (phone language), German or English |
| Colors | Accent, text and background color (color watches only) |
| Daily briefing | Opens the app at a set time on chosen weekdays, vibrates and shows your day. Closes itself after 60 seconds without use. |
| Allow moving | Lets you move your own events from the watch (see below) |
| Sign out | Deletes the sign-in on your phone |

## Moving events

1. Turn on **Allow moving** in the settings and choose the options you want (1 hour, 1 day, 2 days, 1 week later, next Monday).
2. Reopen the app. The watch shows a new sign-in code **once**, because the app now needs permission to change events.
3. Open one of your events, press **Select**, pick an option and confirm with Select.

Only events you organized can be moved. By default only events **without attendees** can be moved; under "Moving allowed for" you can allow all your own events – attendees then receive an update from Outlook. For a recurring event only that one occurrence is moved. If an event cannot be moved, the details say why.

## Battery

The app uses no power when it is closed – there is no background process. While it is open, the touch sensor is the biggest consumer. If you tend to leave the app open, set **Close automatically** or turn off **Touch control**.

## Switching accounts

Turn on **Sign out** in the settings and save, then reopen the app. A new sign-in code appears.

## Lost your phone or watch?

Your sign-in is stored only on your phone. To revoke it without affecting anything else:

- **Personal Microsoft account:** go to <https://account.live.com/consent/Manage>, find the app and choose **Remove these permissions**. You can also change your password, which ends all sign-ins.
- **Work or school account:** go to <https://mysignins.microsoft.com/security-info> or <https://myaccount.microsoft.com> and choose **Sign out everywhere**, or ask your IT department to revoke your sessions.

The next time the app tries to load events it shows "Please reopen the app to sign in."

## Work and school accounts

The app works with personal Microsoft accounts (outlook.com, hotmail.com, live.com) and usually with small organizations. Larger organizations often restrict third-party apps:

| Message on the watch | Reason | What you can do |
|---|---|---|
| Your IT admin must approve this app | Your organization does not allow users to approve apps themselves | Ask your IT department to approve the app |
| Company policy blocks this sign-in | A Conditional Access policy blocks the sign-in method (device code) or requires a managed device | No workaround – the app cannot be used with this account |
| Please reopen the app to sign in | Your organization forces regular re-sign-in | Reopen the app and sign in again |

Accounts in companies with Intune or strict Conditional Access will almost certainly not work.

## Messages

| Message | Meaning |
|---|---|
| Updating... | Loading events from the phone |
| Offline - as of HH:MM | Phone or internet not reachable; stored events are shown |
| No connection to the phone. | No stored events and no phone connection |
| Please reopen the app to sign in | Sign-in expired or revoked |
| No access to calendar | Microsoft denied access to the calendar |
| Graph error <number> | Unexpected answer from Microsoft – try again later |

## Privacy

See the [privacy policy](privacy.md).
