---
classoption: oneside
---

\begin{titlepage}
\centering
\vspace*{3cm}

{\fontsize{28}{34}\selectfont\bfseries ESPclock\par}

\vspace{0.8cm}

{\fontsize{16}{20}\selectfont User Manual\par}

\vspace{0.5cm}

{\large Setup \& Usage Guide\par}

\vspace{2cm}

\begin{tikzpicture}
  \draw[line width=1.5pt, rounded corners=6pt] (0,0) rectangle (3,1.6);
  \node[font=\fontsize{22}{26}\selectfont\ttfamily\bfseries] at (1.5,0.8) {12:00};
\end{tikzpicture}

\vfill

{\normalsize ESPclock Project\par}
{\small Wi-Fi Enabled Desk Clock\par}

\vspace{1cm}
\etocsettocstyle{}{}
\etocsetstyle{part}{}{}{\normalsize\etocname\dotfill\etocpage\par\vspace{0.1em}}{}
\setcounter{tocdepth}{-1}
\tableofcontents

\end{titlepage}

\refstepcounter{part}
\addcontentsline{toc}{part}{English}

\begin{center}
{\fontsize{20}{24}\selectfont\bfseries English\par}
\end{center}

\etocsettocstyle{\section*{Table of Contents}}{}
\etocsettocdepth{1}
\localtableofcontents
\newpage
# Welcome

Thank you for choosing the **ESPclock** — a Wi-Fi-enabled desk clock built on
the ESP microcontroller platform. This manual will guide you through setting up
your clock step by step.

The ESPclock can work in two modes:

- **Online mode** — connects to your home Wi-Fi and automatically gets the
  correct time from the internet.
- **Offline mode** — works without Wi-Fi; you set the time manually using two
  buttons on the device.

The clock has two buttons, labeled **S** (Setup) and **A** (Action). On the
bottom of the device you will find a sticker with a **QR code** and the
clock's web address — you can scan it later to access the settings page.

# What's in the Box

Before you start, make sure you have:

- The ESPclock device (fully assembled)
- A USB cable for power
- A phone, tablet, or computer with Wi-Fi

# Powering On

1. Plug the USB cable into the clock and connect it to a USB power source
   (wall adapter, power bank, or computer).
2. The display will light up. If this is the first time you are using the
   clock, it will start in **offline mode** and show `00:00`.

# Online Setup (Recommended)

Online mode is the easiest way to use your clock. It connects to your Wi-Fi
network and keeps the time accurate automatically.

## Step 1: Connect to the Clock's Wi-Fi

When the clock starts in offline mode, it creates its own temporary Wi-Fi
network (a "hotspot") for 15 minutes.

1. On your phone or computer, open the **Wi-Fi settings**.
2. Look for a network named **ESPclock-XXXXXX** (where XXXXXX is a unique
   code for your clock).
3. Connect to it using the password: **`waltwhite64`**

> **Tip:** The clock's hotspot is only active for 15 minutes after powering
> on. If you miss it, simply unplug the clock and plug it back in.

## Step 2: Open the Setup Page

Once connected to the clock's Wi-Fi:

1. Open a web browser (Chrome, Safari, Firefox, etc.).
2. Go to: **http://192.168.4.1**
3. The ESPclock setup page will appear.

## Step 3: Connect to Your Home Wi-Fi

On the setup page you will see a **Wi-Fi Settings** section:

1. The **Available networks** dropdown shows all Wi-Fi networks the clock can
   detect. Select your home network.
   - If you don't see your network, tap the **Refresh** button.
2. Enter your Wi-Fi **password** in the password field.
   - You can tap the eye icon to show/hide the password.
3. Tap **CONNECT**.

The clock will now try to connect. You will see an animated status indicator.
If the connection is successful, you will see a green checkmark and the
clock's new IP address on your home network.

> **If connection fails:** Double-check your Wi-Fi password and try again.

## Step 4: Choose Your Timezone

After connecting to Wi-Fi, the setup page shows a **Timezone** section:

1. Select your **Region** (e.g., Europe, America, Asia).
2. Select your **City** (e.g., Amsterdam, New York, Tokyo).
3. Tap **SAVE & FINISH**.

The clock will now sync with an internet time server and display the correct
time for your timezone. The setup is complete!

## Step 5: Reconnect to Your Home Wi-Fi

After the setup finishes, the clock turns off its hotspot.

1. On your phone or computer, reconnect to your **home Wi-Fi** network.
2. You can now access the clock's settings at any time by scanning the
   **QR code** on the bottom of the clock, or by typing the address printed
   on the sticker into your browser.

# Offline Setup (No Internet)

If you don't have Wi-Fi or prefer not to connect the clock to the internet,
you can set the time manually using the two buttons on the device.

## Entering Time Edit Mode

1. Make sure the clock is **not** connected to Wi-Fi (offline mode).
2. **Press and hold** the **S button** (Setup) for about 2 seconds.
3. The display will start blinking — you are now in time edit mode.

## Setting the Time

Once in time edit mode, you will go through three steps:

### 1. Choose 12-Hour or 24-Hour Format

- The display will show either **12h** or **24h**.
- **Hold** the **A button** (Action) to switch between formats.
- **Press** the **A button** briefly to confirm and move on.

### 2. Set the Hour

- The hour digits will blink.
- **Hold** the **A button** to increase the hour.
  The value will cycle from 0 through 23 (or 1 through 12 in 12-hour mode)
  and wrap around.
- **Press** the **A button** briefly to confirm and move to minutes.

### 3. Set the Minutes

- The minute digits will blink.
- **Hold** the **A button** to increase the minutes.
  The value will cycle from 0 through 59 and wrap around.
- **Press** the **A button** briefly to confirm.

The clock will now start keeping time from the values you set.

# Changing Settings (Online Mode)

When the clock is connected to your Wi-Fi, you can change its settings using
any web browser on the same network.

1. Scan the **QR code** on the bottom of the clock, or type the address
   printed on the sticker into your browser.
2. The settings page will appear.

## Time Settings

- **NTP Server** — The internet time server. The default (`pool.ntp.org`)
  works well for most people. You usually don't need to change this.
- **Timezone** — Select your Region and City, then tap **UPDATE TIME**.

## Display Settings

- **Brightness** — Drag the slider (0 = dimmest, 7 = brightest).
- **Auto brightness** — When turned on, the clock automatically adjusts
  brightness based on the time of day:
  - Night (12 AM – 9 AM): Very dim
  - Day (9 AM – 5 PM): Bright
  - Evening (5 PM – 8 PM): Medium
  - Late evening (8 PM – 12 AM): Dim
- **Blinking colon** — Toggle the blinking dots between the hours and minutes.
- **12-Hour clock** — Toggle between 12-hour (1:00 – 12:59) and 24-hour
  (0:00 – 23:59) display.

## Saving Settings

- Tap **UPDATE** to store your settings permanently. They will be remembered
  even after a power outage.
- If you ever want to start fresh, tap **DELETE** to erase all saved settings
  and return the clock to offline mode.

# Resetting the Clock

If you want to erase all settings and start over (for example, to connect to
a different Wi-Fi network):

1. **Press and hold both buttons (S and A)** at the same time.
2. Keep holding — after about **5 seconds** the display will show `88:88` to
   let you know the reset is being registered.
3. **Continue holding** until the display flashes on and off (at about
   **10 seconds**). This confirms the reset is complete.
4. You can now **release both buttons**. The clock will restart in offline
   you can set it up from scratch.

> You can also reset the clock from the web interface by tapping **DELETE**
> in the Configuration section.

# Troubleshooting

## The display shows "Err0"

The clock's internal storage has a problem. Try unplugging and replugging the
clock. If the error persists, the device may need to be reflashed.

## The display shows "Err1"

The web interface files are missing from the clock's storage. The device needs
to have its filesystem re-uploaded. Contact the person who built the clock.

## I can't find the clock's Wi-Fi hotspot

The hotspot is only active for **15 minutes** after the clock is powered on.
Unplug the clock, wait a few seconds, and plug it back in. The hotspot will
appear again.

## The clock shows the wrong time

1. Open the clock's web interface.
2. Check that the correct **Timezone** (Region and City) is selected.
3. Tap **UPDATE TIME** to re-sync.

## I forgot the clock's address

Scan the **QR code** on the bottom of the clock, or type the address printed
on the sticker into your browser. The address is also shown in the Wi-Fi
hotspot name (ESPclock-**XXXXXX** → **http://espclock-XXXXXX.local**).

## The Wi-Fi password was wrong

Double-check your password and tap **CONNECT** again. Make sure you're
entering the password for your home Wi-Fi — not the clock's hotspot password.

# Quick Reference

| What you want to do | How to do it |
|---|---|
| First-time setup | Connect to ESPclock-XXXXXX Wi-Fi → open 192.168.4.1 → follow steps |
| Change settings | Scan QR code on bottom of clock |
| Set time manually | Hold S button 2 sec → use A button to set |
| Reset the clock | Hold both buttons (S + A) for 10 seconds |
| Hotspot password | `waltwhite64` |

---

*ESPclock — an open-source project licensed under GPL-3.0.*
*Visit https://github.com/nltimv for more information.*

\newpage

\refstepcounter{part}
\addcontentsline{toc}{part}{Nederlands}

\begin{center}
{\fontsize{20}{24}\selectfont\bfseries Nederlands\par}
\end{center}

\bigskip

\etocsettocstyle{\section*{Inhoudsopgave}}{}
\etocsettocdepth{1}
\localtableofcontents
\newpage

\setcounter{section}{0}

# Welkom

Bedankt voor het kiezen van de **ESPclock** — een bureauklok met
Wi-Fi-verbinding, gebouwd op het ESP-microcontrollerplatform. Deze
handleiding begeleidt u stap voor stap bij het instellen van uw klok.

De ESPclock heeft twee modi:

- **Online modus** — maakt verbinding met uw Wi-Fi-netwerk en haalt
  automatisch de juiste tijd op via internet.
- **Offline modus** — werkt zonder Wi-Fi; u stelt de tijd handmatig in
  met de twee knoppen op het apparaat.

De klok heeft twee knoppen, gelabeld **S** (Setup) en **A** (Action). Op
de onderkant van het apparaat vindt u een sticker met een **QR-code** en
het webadres van de klok — scan deze later om de instellingenpagina te
openen.

# Inhoud van de verpakking

Controleer voordat u begint of u het volgende heeft:

- Het ESPclock-apparaat (volledig gemonteerd)
- Een USB-kabel voor de stroomvoorziening
- Een telefoon, tablet of computer met Wi-Fi

# Inschakelen

1. Sluit de USB-kabel aan op de klok en verbind deze met een
   USB-stroombron (adapter, powerbank of computer).
2. Het display licht op. Als u de klok voor het eerst gebruikt, start
   deze in **offline modus** en toont `00:00`.

# Online instellen (aanbevolen)

Online modus is de eenvoudigste manier om uw klok te gebruiken. De klok
maakt verbinding met uw Wi-Fi-netwerk en houdt de tijd automatisch
nauwkeurig.

## Stap 1: Verbind met het Wi-Fi van de klok

Wanneer de klok in offline modus opstart, maakt deze een eigen tijdelijk
Wi-Fi-netwerk (een "hotspot") aan, dat 15 minuten actief blijft.

1. Open de **Wi-Fi-instellingen** op uw telefoon of computer.
2. Zoek een netwerk met de naam **ESPclock-XXXXXX** (waarbij XXXXXX een
   unieke code is voor uw klok).
3. Maak verbinding met het wachtwoord: **`waltwhite64`**

> **Tip:** De hotspot van de klok is slechts 15 minuten actief na het
> inschakelen. Als u deze mist, trek de stekker eruit en sluit de klok
> opnieuw aan.

## Stap 2: Open de instellingenpagina

Nadat u verbonden bent met het Wi-Fi van de klok:

1. Open een webbrowser (Chrome, Safari, Firefox, etc.).
2. Ga naar: **http://192.168.4.1**
3. De instellingenpagina van de ESPclock verschijnt.

## Stap 3: Verbind met uw eigen Wi-Fi-netwerk

Op de instellingenpagina ziet u het onderdeel **Wi-Fi Settings**:

1. Het menu **Available networks** toont alle Wi-Fi-netwerken die de klok
   kan detecteren. Selecteer uw thuisnetwerk.
   - Ziet u uw netwerk niet? Tik op **Refresh**.
2. Voer uw Wi-Fi-**wachtwoord** in.
   - U kunt op het oogpictogram tikken om het wachtwoord te
     tonen/verbergen.
3. Tik op **CONNECT**.

De klok probeert nu verbinding te maken. U ziet een geanimeerde
statusindicator. Bij een geslaagde verbinding verschijnt een groen
vinkje en het nieuwe IP-adres van de klok op uw thuisnetwerk.

> **Als de verbinding mislukt:** controleer uw Wi-Fi-wachtwoord en
> probeer het opnieuw.

## Stap 4: Kies uw tijdzone

Na het verbinden met Wi-Fi toont de instellingenpagina het onderdeel
**Timezone**:

1. Selecteer uw **Region** (bijv. Europe, America, Asia).
2. Selecteer uw **City** (bijv. Amsterdam, New York, Tokyo).
3. Tik op **SAVE & FINISH**.

De klok synchroniseert nu met een internettijdserver en toont de juiste
tijd voor uw tijdzone. De installatie is voltooid!

## Stap 5: Maak opnieuw verbinding met uw eigen Wi-Fi

Na het afronden van de installatie schakelt de klok de hotspot uit.

1. Verbind uw telefoon of computer opnieuw met uw **eigen
   Wi-Fi-netwerk**.
2. U kunt de instellingen van de klok op elk moment openen door de
   **QR-code** op de onderkant van de klok te scannen, of door het adres
   op de sticker in uw browser in te voeren.

# Offline instellen (zonder internet)

Als u geen Wi-Fi heeft of de klok liever niet met internet verbindt,
kunt u de tijd handmatig instellen met de twee knoppen op het apparaat.

## Tijdinstelmodus activeren

1. Zorg ervoor dat de klok **niet** verbonden is met Wi-Fi (offline
   modus).
2. **Houd** de **S-knop** (Setup) ongeveer 2 seconden **ingedrukt**.
3. Het display begint te knipperen — u bent nu in de tijdinstelmodus.

## De tijd instellen

In de tijdinstelmodus doorloopt u drie stappen:

### 1. Kies 12-uurs- of 24-uursnotatie

- Het display toont **12h** of **24h**.
- **Houd** de **A-knop** (Action) ingedrukt om te wisselen.
- **Druk** kort op de **A-knop** om te bevestigen en verder te gaan.

### 2. Stel het uur in

- De urcijfers knipperen.
- **Houd** de **A-knop** ingedrukt om het uur te verhogen. De waarde
  loopt van 0 tot 23 (of 1 tot 12 in 12-uursmodus) en begint daarna
  opnieuw.
- **Druk** kort op de **A-knop** om te bevestigen en naar de minuten te
  gaan.

### 3. Stel de minuten in

- De minutencijfers knipperen.
- **Houd** de **A-knop** ingedrukt om de minuten te verhogen. De waarde
  loopt van 0 tot 59 en begint daarna opnieuw.
- **Druk** kort op de **A-knop** om te bevestigen.

De klok begint nu de tijd bij te houden vanaf de ingestelde waarden.

# Instellingen wijzigen (online modus)

Wanneer de klok verbonden is met uw Wi-Fi, kunt u de instellingen
wijzigen via een webbrowser op hetzelfde netwerk.

1. Scan de **QR-code** op de onderkant van de klok, of voer het adres op
   de sticker in uw browser in.
2. De instellingenpagina verschijnt.

## Tijdinstellingen

- **NTP Server** — De internettijdserver. De standaardwaarde
  (`pool.ntp.org`) werkt goed voor de meeste gebruikers. Dit hoeft u
  meestal niet te wijzigen.
- **Timezone** — Selecteer uw Region en City en tik op **UPDATE TIME**.

## Beeldscherminstellingen

- **Brightness** — Versleep de schuifbalk (0 = dimst, 7 = helderst).
- **Auto brightness** — Wanneer ingeschakeld, past de klok de
  helderheid automatisch aan op basis van het tijdstip:
  - Nacht (00:00 – 09:00): Zeer dim
  - Dag (09:00 – 17:00): Helder
  - Avond (17:00 – 20:00): Gemiddeld
  - Late avond (20:00 – 00:00): Dim
- **Blinking colon** — Schakel de knipperende dubbele punt tussen de
  uren en minuten in of uit.
- **12-Hour clock** — Schakel tussen 12-uursnotatie (1:00 – 12:59) en
  24-uursnotatie (0:00 – 23:59).

## Instellingen opslaan

- Tik op **UPDATE** om uw instellingen permanent op te slaan. Ze blijven
  bewaard, ook na een stroomuitval.
- Als u opnieuw wilt beginnen, tik dan op **DELETE** om alle opgeslagen
  instellingen te wissen en de klok terug te zetten naar offline modus.

# De klok resetten

Als u alle instellingen wilt wissen en opnieuw wilt beginnen
(bijvoorbeeld om verbinding te maken met een ander Wi-Fi-netwerk):

1. **Houd beide knoppen (S en A)** tegelijkertijd ingedrukt.
2. Blijf vasthouden — na ongeveer **5 seconden** toont het display
   `88:88` om aan te geven dat de reset wordt geregistreerd.
3. **Blijf vasthouden** totdat het display aan en uit flitst (na
   ongeveer **10 seconden**). Dit bevestigt dat de reset is voltooid.
4. U kunt nu **beide knoppen loslaten**. De klok start opnieuw op in
   offline modus en de hotspot wordt weer 15 minuten beschikbaar, zodat
   u de klok opnieuw kunt instellen.

> U kunt de klok ook resetten via de webinterface door op **DELETE** te
> tikken in het onderdeel Configuration.

# Probleemoplossing

## Het display toont "Err0"

Het interne geheugen van de klok heeft een probleem. Probeer de klok los
te koppelen en opnieuw aan te sluiten. Als de fout aanhoudt, moet het
apparaat mogelijk opnieuw worden geflasht.

## Het display toont "Err1"

De bestanden voor de webinterface ontbreken in het geheugen van de klok.
Het bestandssysteem moet opnieuw worden geüpload. Neem contact op met de
persoon die de klok heeft gebouwd.

## Ik kan de Wi-Fi-hotspot van de klok niet vinden

De hotspot is slechts **15 minuten** actief na het inschakelen van de
klok. Trek de stekker eruit, wacht een paar seconden en sluit de klok
opnieuw aan. De hotspot verschijnt dan weer.

## De klok toont de verkeerde tijd

1. Open de webinterface van de klok.
2. Controleer of de juiste **Timezone** (Region en City) is geselecteerd.
3. Tik op **UPDATE TIME** om opnieuw te synchroniseren.

## Ik ben het adres van de klok vergeten

Scan de **QR-code** op de onderkant van de klok, of voer het adres op de
sticker in uw browser in. Het adres is ook af te leiden uit de naam van
de hotspot (ESPclock-**XXXXXX** → **http://espclock-XXXXXX.local**).

## Het Wi-Fi-wachtwoord was onjuist

Controleer uw wachtwoord en tik opnieuw op **CONNECT**. Zorg ervoor dat
u het wachtwoord van uw thuisnetwerk invoert — niet het wachtwoord van
de hotspot van de klok.

# Naslagkaart

| Wat u wilt doen | Hoe u het doet |
|---|---|
| Eerste installatie | Verbind met ESPclock-XXXXXX Wi-Fi → open 192.168.4.1 → volg de stappen |
| Instellingen wijzigen | Scan de QR-code op de onderkant van de klok |
| Tijd handmatig instellen | Houd S-knop 2 sec. ingedrukt → gebruik A-knop |
| Klok resetten | Houd beide knoppen (S + A) 10 seconden ingedrukt |
| Hotspot-wachtwoord | `waltwhite64` |

---

*ESPclock — een opensourceproject onder de GPL-3.0-licentie.*
*Bezoek https://github.com/nltimv voor meer informatie.*
