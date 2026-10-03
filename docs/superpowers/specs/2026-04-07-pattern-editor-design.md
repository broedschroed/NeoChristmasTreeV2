# NeoChristmasTree Pattern Editor — Design Spec

**Datum:** 2026-04-07  
**Projekt:** NeoChristmasTreeV2KI  
**Ziel:** Web-basierter visueller Editor für LED-Muster-Arrays, die in `AllPattern.h` eingebunden werden.

---

## 1. Übersicht

Einzelne HTML-Datei (`pattern-editor.html`) ohne externe Abhängigkeiten. Öffnen per Doppelklick im Browser. Kein Server, keine Installation.

Der Editor ermöglicht das visuelle Definieren und Animieren von LED-Mustern für einen 4-flügeligen Weihnachtsbaum mit 66 NeoPixel-LEDs (ESP32-S3). Ein Muster wird als C-Array-Block exportiert und per Copy-Paste in `AllPattern.h` eingefügt.

---

## 2. Hardware-Referenz

### LED-Layout (66 LEDs total)

Zwei dreieckige Leiterplatten (PCB1, PCB2) werden rechtwinklig ineinander gesteckt und erzeugen 4 Flügel. LEDs sitzen an den Schrägen der Leiterplatten.

Die LED-Kette läuft kontinuierlich um jede Leiterplatte herum. Alle Seiten-Angaben gelten aus **Frontansicht** (bzw. "Durchschauen" von vorne).

**PCB1:**
| Bereich                      | LEDs    | Richtung                  |
|------------------------------|---------|---------------------------|
| Front, linke Schräge         | 1–8     | unten (1) → Spitze (8)    |
| Front, rechte Schräge        | 9–16    | Spitze (9) → unten (16)   |
| Back, rechte Schräge (Frontansicht) | 17–24   | unten (17) → Spitze (24)  |
| Back, linke Schräge (Frontansicht)  | 25–32   | Spitze (25) → unten (32)  |

> Wichtig: LED 17 setzt die Kette an der rechten Schräge fort (gleiche Seite wie LED 9–16), da die Kette "durch die Platine hindurch" auf die Rückseite übergeht. Von der Rückseite aus betrachtet (Platine umdrehen) erscheinen LEDs 17–24 links und 25–32 rechts.

**PCB2:** (identisches Muster)
| Bereich                      | LEDs    | Richtung                  |
|------------------------------|---------|---------------------------|
| Front, linke Schräge         | 33–40   | unten (33) → Spitze (40)  |
| Front, rechte Schräge        | 41–48   | Spitze (41) → unten (48)  |
| Back, rechte Schräge (Frontansicht) | 49–56   | unten (49) → Spitze (56)  |
| Back, linke Schräge (Frontansicht)  | 57–64   | Spitze (57) → unten (64)  |

**Spitzen:** LED 65 (PCB2 Front), LED 66 (PCB2 Back)

### Farb-Definitionen

36 Farben: `BLK` + je 5 Helligkeitsstufen für RD, GN, BL, YL, CY, MG, WH.  
Definiert in `AllPattern.h` als `int colordef[36][3]`.

### Muster-Format

```c
#define LEDLENMYPATTERN 16   // Anzahl definierter LEDs (< 66 → wird wiederholt)
#define LENMYPATTERN    8    // Anzahl Frames (Animationsschritte)
#define WAITMYPATTERN   100  // Wartezeit in ms zwischen Frames

int mypattern [LENMYPATTERN] [LEDLENMYPATTERN] = {
    { RD1, GN2, BLK, ... },  // Frame 1
    { RD2, GN3, BLK, ... },  // Frame 2
    ...
};
```

---

## 3. UI-Layout

```
┌─────────────────────────────────────────────────────────────┐
│  NeoChristmasTree Pattern Editor  [Import]  [▶ Vorschau]  [Export] │
├───────────────┬─────────────────────┬───────────────────────┤
│  FRAME-LISTE  │   BAUM-ANSICHT      │   FARBPALETTE         │
│               │   (Draufsicht)      │                       │
│  Frame 1  ←  │                     │  ■ BLK                │
│  Frame 2      │      [Kreuz]        │  RD: ■₁■₂■₃■₄■₅      │
│  Frame 3      │                     │  GN: ■₁■₂■₃■₄■₅      │
│  ...          │                     │  ...                  │
│               │                     │                       │
│  [+] [-] [⧉] │                     │  Aktive Farbe: RD3    │
├───────────────┴─────────────────────┴───────────────────────┤
│  Name: [________]  LEDLEN: [66]  WAIT: [100] ms             │
│  [◀◀] [▶/❙❙] [▶▶]   Geschwindigkeit: [────O──]             │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. Baum-Draufsicht (Kreuzform)

Das Kreuz hat 4 Arme + Zentrum:

Alle Arme werden aus der **Außenperspektive** dargestellt (als würde man von außen auf diese Baumseite schauen). Dadurch sind die Spalten bei Vorder- und Rückseite eines PCBs spiegelverkehrt — was der physischen Realität entspricht (die Kette geht einmal um die Platine herum).

- **Oben (PCB2-Front):** Linke Spalte: LED 33→40 (Basis→Spitze, von oben nach unten), rechte Spalte: LED 48→41 (Basis→Spitze)
- **Rechts (PCB1-Front):** Obere Zeile: LED 1→8 (Basis→Spitze, von rechts nach links), untere Zeile: LED 16→9 (Basis→Spitze)
- **Unten (PCB2-Back):** Linke Spalte: LED 56→49 (Spitze→Basis, von oben nach unten — linke Seite aus Außensicht = rechte Schräge Frontansicht), rechte Spalte: LED 57→64
- **Links (PCB1-Back):** Obere Zeile: LED 24→17 (Spitze→Basis, von rechts nach links — obere Reihe aus Außensicht = rechte Schräge Frontansicht), untere Zeile: LED 25→32
- **Mitte:** 2 Zellen für LED 65 (PCB2 Front-Spitze) und LED 66 (PCB2 Back-Spitze)

Die Außenperspektive bedeutet: Wenn man von links auf den linken Arm (PCB1-Back) schaut, sieht man LED 17 unten-links und LED 32 unten-rechts — genau wie man die Rückseite physisch sehen würde.

Jede LED-Zelle:
- Zeigt Hintergrundfarbe gemäß gewählter Farbe
- **Hover:** Tooltip mit LED-Nummer und Farbname
- **Linksklick:** Setzt LED auf aktive Farbe
- **Rechtsklick:** Setzt LED auf BLK

---

## 5. Frame-Liste

- Nummerierte Liste, aktiver Frame hervorgehoben
- Mini-Vorschau je Frame: alle LEDs als winzige Punkte in Kreuzform
- Buttons: `[+]` Frame hinzufügen (Kopie des aktuellen), `[-]` Frame löschen, `[⧉]` Frame duplizieren
- Drag & Drop zum Umsortieren

---

## 6. Farbpalette

36 Farben als farbige Quadrate, nach Farbart gruppiert (BLK, dann RD1–RD5, GN1–GN5 usw.).  
Aktiv ausgewählte Farbe mit Rahmen markiert.  
Klick = Stift wechseln.

---

## 7. LEDLEN-Verhalten

- **Bearbeitung:** Bei LEDLEN < 66 sind nur LEDs 1–LEDLEN aktiv anklickbar; LEDs > LEDLEN werden ausgegraut
- **Animationsvorschau:** Alle 66 LEDs werden angezeigt. Das Muster wird intern wiederholt (analog zum `patternwork()`-Code auf dem ESP32), sodass die Vorschau exakt dem Hardware-Verhalten entspricht

---

## 8. Symmetrie-Hilfe

Button **"PCB1 → PCB2 kopieren"**: Kopiert LED-Werte 1–32 auf LEDs 33–64 des aktuellen Frames.  
Nützlich für Muster, die auf beiden Leiterplatten identisch sein sollen.

---

## 9. Animationsvorschau

- Play/Pause-Button wechselt in Vorschau-Modus
- Geschwindigkeits-Slider: unabhängig vom WAIT-Wert des Musters (nur für die Vorschau)
- Im Vorschau-Modus: Frame-Liste scrollt mit, aktueller Frame ist markiert
- Vorspulen / Zurückspulen: Frame-für-Frame

---

## 10. Import

Ein **[Import]**-Button öffnet einen Dialog mit einem Textfeld. Der Nutzer fügt einen vollständigen C-Array-Block aus `AllPattern.h` per Copy-Paste ein. Die App parst daraus automatisch:

- Pattern-Name (aus dem Array-Namen)
- LEDLEN (aus `#define LEDLEN...`)
- WAIT (aus `#define WAIT...`)
- Alle Frames (alle Zeilen des Arrays)

Der Parser ist tolerant gegenüber Kommentaren (z.B. `// 1  2  3 ...`) und unterschiedlichen Leerzeichen. Unbekannte Token (die nicht in den 36 Farbnamen enthalten sind) werden als `BLK` interpretiert und dem Nutzer als Warnung angezeigt.

Nach dem Import wird das aktuelle Muster vollständig ersetzt (mit Bestätigungsdialog, falls bereits Daten vorhanden sind).

---

## 11. Export

Klick auf **[Export]** öffnet einen Dialog/Textbereich mit vollständigem C-Code-Block:

```c
// ============ PATTERN-NAME ===================

#define LEDLENMYPATTERN 16
#define LENMYPATTERN    8
#define WAITMYPATTERN   100

int mypattern [LENMYPATTERN] [LEDLENMYPATTERN] = {

    // 1    2    3    ...  16
    { RD1, GN2, BLK, ... },
    ...

};
```

- Pattern-Name aus dem Namensfeld: Großbuchstaben für `#define`-Prefix, Kleinbuchstaben für Array-Name
- Leerzeichen im Namen werden durch Underscore ersetzt
- **[Kopieren]**-Button: Kopiert Text in Zwischenablage

---

## 11. Technische Umsetzung

- **Datei:** Einzelne `pattern-editor.html` im Projektverzeichnis
- **Technologie:** Vanilla HTML + CSS + JavaScript (keine Frameworks, keine Build-Tools)
- **State:** Alles im Speicher als JS-Objekt; kein LocalStorage (bewusst einfach gehalten)
- **Kompatibilität:** Moderne Browser (Chrome, Firefox, Edge)
