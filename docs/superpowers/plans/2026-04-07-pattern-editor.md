# NeoChristmasTree Pattern Editor — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eine einzelne `pattern-editor.html`-Datei, die LED-Animationsmuster für den 66-LED-Weihnachtsbaum visuell bearbeitet und als C-Array-Code exportiert.

**Architecture:** Vanilla HTML/CSS/JS in einer einzigen Datei ohne externe Abhängigkeiten. Der globale `state`-Objekt hält alle Daten; jede Änderung ruft gezielte Render-Funktionen auf. CSS-Grid für das 18×18-Kreuz-Layout.

**Tech Stack:** HTML5, CSS Grid, Vanilla JavaScript (ES6), `<canvas>` für Mini-Frame-Vorschauen

---

## Dateistruktur

Alles in einer Datei: `pattern-editor.html`

JavaScript-Sektionen (durch Kommentare getrennt):
- `// === CONSTANTS ===` — Farben, Kreuz-Layout
- `// === STATE ===` — globales Zustandsobjekt
- `// === RENDERING ===` — alle render*()-Funktionen
- `// === EVENT HANDLERS ===` — Klick-, Drag-Handler
- `// === ANIMATION ===` — Play/Pause-Loop
- `// === EXPORT ===` — C-Code-Generierung
- `// === IMPORT ===` — C-Code-Parser
- `// === INIT ===` — App-Start

---

## Kreuz-Layout-Logik (Referenz für alle Tasks)

Das Kreuz ist ein CSS-Grid mit 18 Spalten × 18 Zeilen à 22px.
- Zeilen 1–8 / Spalten 9–10: oberer Arm (PCB2-Front)
- Zeilen 9–10 / Spalten 11–18: rechter Arm (PCB1-Front)
- Zeilen 9–10 / Spalten 1–8: linker Arm (PCB1-Back)
- Zeilen 11–18 / Spalten 9–10: unterer Arm (PCB2-Back)
- Zeilen 9–10 / Spalten 9–10: Mitte (LEDs 65, 66)

LED-Positionen (0-basierter Index = LED_Nummer − 1):

```
Rechter Arm (PCB1-Front), obere Zeile (row 9):
  col 11=LED8, 12=LED7, 13=LED6, 14=LED5, 15=LED4, 16=LED3, 17=LED2, 18=LED1

Rechter Arm (PCB1-Front), untere Zeile (row 10):
  col 11=LED9, 12=LED10, 13=LED11, 14=LED12, 15=LED13, 16=LED14, 17=LED15, 18=LED16

Linker Arm (PCB1-Back), obere Zeile (row 9):
  col 1=LED17, 2=LED18, 3=LED19, 4=LED20, 5=LED21, 6=LED22, 7=LED23, 8=LED24

Linker Arm (PCB1-Back), untere Zeile (row 10):
  col 1=LED32, 2=LED31, 3=LED30, 4=LED29, 5=LED28, 6=LED27, 7=LED26, 8=LED25

Oberer Arm (PCB2-Front), linke Spalte (col 9):
  row 1=LED33, 2=LED34, 3=LED35, 4=LED36, 5=LED37, 6=LED38, 7=LED39, 8=LED40

Oberer Arm (PCB2-Front), rechte Spalte (col 10):
  row 1=LED48, 2=LED47, 3=LED46, 4=LED45, 5=LED44, 6=LED43, 7=LED42, 8=LED41

Unterer Arm (PCB2-Back), linke Spalte (col 9):
  row 11=LED56, 12=LED55, 13=LED54, 14=LED53, 15=LED52, 16=LED51, 17=LED50, 18=LED49

Unterer Arm (PCB2-Back), rechte Spalte (col 10):
  row 11=LED57, 12=LED58, 13=LED59, 14=LED60, 15=LED61, 16=LED62, 17=LED63, 18=LED64

Mitte:
  row 9, col 9 = LED65
  row 10, col 9 = LED66
```

---

## Task 1: HTML-Skeleton + CSS-Layout

**Files:**
- Create: `pattern-editor.html`

- [ ] **Schritt 1: HTML-Grundstruktur erstellen**

```html
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="UTF-8">
  <title>NeoChristmasTree Pattern Editor</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: monospace; background: #1a1a1a; color: #ddd; height: 100vh; display: flex; flex-direction: column; }

    /* === TOP BAR === */
    #topbar {
      display: flex; align-items: center; gap: 12px;
      background: #2a2a2a; padding: 8px 16px; border-bottom: 1px solid #444;
      flex-shrink: 0;
    }
    #topbar h1 { font-size: 14px; color: #aaa; flex: 1; }
    .btn {
      background: #3a3a3a; color: #ddd; border: 1px solid #555;
      padding: 5px 12px; cursor: pointer; font-family: monospace; font-size: 13px;
      border-radius: 3px;
    }
    .btn:hover { background: #4a4a4a; }
    .btn-primary { background: #1a5a1a; border-color: #2a8a2a; color: #aeffae; }
    .btn-primary:hover { background: #2a6a2a; }

    /* === MAIN AREA === */
    #main {
      display: grid;
      grid-template-columns: 200px 1fr 180px;
      flex: 1;
      overflow: hidden;
    }

    /* === FRAME LIST (left panel) === */
    #frame-panel {
      background: #222; border-right: 1px solid #444;
      display: flex; flex-direction: column; overflow: hidden;
    }
    #frame-panel-header {
      padding: 8px; background: #2a2a2a; border-bottom: 1px solid #444;
      font-size: 12px; color: #888;
    }
    #frame-list { flex: 1; overflow-y: auto; }
    .frame-item {
      display: flex; align-items: center; gap: 6px;
      padding: 4px 8px; cursor: pointer; border-bottom: 1px solid #333;
      user-select: none;
    }
    .frame-item:hover { background: #2a2a2a; }
    .frame-item.active { background: #1a3a1a; border-left: 3px solid #4a4; }
    .frame-item .frame-num { font-size: 11px; color: #666; width: 24px; text-align: right; }
    .frame-item canvas { border: 1px solid #444; }
    #frame-buttons { display: flex; gap: 4px; padding: 8px; background: #2a2a2a; border-top: 1px solid #444; }
    #frame-buttons .btn { flex: 1; font-size: 16px; padding: 4px; }

    /* === CROSS VIEW (center panel) === */
    #cross-panel {
      display: flex; align-items: center; justify-content: center;
      background: #1a1a1a; overflow: auto; padding: 20px;
    }
    #cross-grid {
      display: grid;
      grid-template-columns: repeat(18, 22px);
      grid-template-rows: repeat(18, 22px);
      gap: 1px;
    }
    .led-cell {
      width: 22px; height: 22px;
      background: #000; border: 1px solid #333;
      cursor: pointer; border-radius: 2px;
      transition: border-color 0.1s;
    }
    .led-cell:hover { border-color: #fff; }
    .led-cell.led-inactive { opacity: 0.2; cursor: default; }
    .led-cell.led-inactive:hover { border-color: #333; }

    /* === COLOR PALETTE (right panel) === */
    #palette-panel {
      background: #222; border-left: 1px solid #444;
      display: flex; flex-direction: column; overflow-y: auto; padding: 8px;
    }
    #palette-panel h3 { font-size: 11px; color: #888; margin-bottom: 6px; }
    .color-group { margin-bottom: 6px; }
    .color-group-label { font-size: 10px; color: #666; margin-bottom: 2px; }
    .color-swatches { display: flex; flex-wrap: wrap; gap: 2px; }
    .color-swatch {
      width: 24px; height: 24px; cursor: pointer;
      border: 2px solid transparent; border-radius: 2px;
    }
    .color-swatch:hover { border-color: #aaa; }
    .color-swatch.active { border-color: #fff; outline: 1px solid #fff; }
    #active-color-label {
      margin-top: 8px; font-size: 11px; color: #aaa; padding: 4px;
      background: #2a2a2a; border: 1px solid #444; border-radius: 2px;
    }
    #sym-btn { margin-top: 12px; width: 100%; font-size: 11px; }

    /* === BOTTOM BAR === */
    #bottombar {
      background: #2a2a2a; border-top: 1px solid #444;
      padding: 8px 16px; display: flex; align-items: center; gap: 16px;
      flex-shrink: 0; flex-wrap: wrap;
    }
    .setting-group { display: flex; align-items: center; gap: 6px; font-size: 12px; }
    .setting-group label { color: #888; }
    .setting-group input[type=text], .setting-group input[type=number] {
      background: #1a1a1a; border: 1px solid #555; color: #ddd;
      padding: 3px 6px; font-family: monospace; font-size: 12px;
      border-radius: 2px;
    }
    .setting-group input[type=text] { width: 120px; }
    .setting-group input[type=number] { width: 60px; }
    #anim-controls { display: flex; align-items: center; gap: 8px; margin-left: auto; }
    #speed-slider { width: 100px; }

    /* === MODAL === */
    .modal-overlay {
      display: none; position: fixed; inset: 0;
      background: rgba(0,0,0,0.7); z-index: 100;
      align-items: center; justify-content: center;
    }
    .modal-overlay.open { display: flex; }
    .modal {
      background: #2a2a2a; border: 1px solid #555; border-radius: 4px;
      padding: 20px; width: 600px; max-width: 90vw; max-height: 80vh;
      display: flex; flex-direction: column; gap: 12px;
    }
    .modal h2 { font-size: 14px; color: #aaa; }
    .modal textarea {
      flex: 1; min-height: 300px; background: #1a1a1a; color: #ddd;
      border: 1px solid #555; font-family: monospace; font-size: 11px;
      padding: 8px; border-radius: 2px; resize: vertical;
    }
    .modal-buttons { display: flex; gap: 8px; justify-content: flex-end; }
    #import-warning { color: #f90; font-size: 11px; display: none; }
  </style>
</head>
<body>

  <!-- TOP BAR -->
  <div id="topbar">
    <h1>NeoChristmasTree Pattern Editor</h1>
    <button class="btn" id="import-btn">↓ Import</button>
    <button class="btn" id="preview-btn">▶ Vorschau</button>
    <button class="btn btn-primary" id="export-btn">↑ Export</button>
  </div>

  <!-- MAIN -->
  <div id="main">

    <!-- LEFT: Frame List -->
    <div id="frame-panel">
      <div id="frame-panel-header">Frames</div>
      <div id="frame-list"></div>
      <div id="frame-buttons">
        <button class="btn" id="frame-add-btn" title="Frame hinzufügen (Kopie)">+</button>
        <button class="btn" id="frame-del-btn" title="Frame löschen">−</button>
        <button class="btn" id="frame-dup-btn" title="Frame duplizieren">⧉</button>
      </div>
    </div>

    <!-- CENTER: Cross View -->
    <div id="cross-panel">
      <div id="cross-grid"></div>
    </div>

    <!-- RIGHT: Color Palette -->
    <div id="palette-panel">
      <h3>Farbe wählen</h3>
      <div id="color-groups"></div>
      <div id="active-color-label">Aktiv: BLK</div>
      <button class="btn" id="sym-btn">PCB1 → PCB2</button>
    </div>

  </div>

  <!-- BOTTOM BAR -->
  <div id="bottombar">
    <div class="setting-group">
      <label>Name:</label>
      <input type="text" id="pattern-name" value="mypattern">
    </div>
    <div class="setting-group">
      <label>LEDLEN:</label>
      <input type="number" id="ledlen-input" value="66" min="1" max="66">
    </div>
    <div class="setting-group">
      <label>WAIT (ms):</label>
      <input type="number" id="wait-input" value="100" min="1">
    </div>
    <div id="anim-controls">
      <button class="btn" id="prev-frame-btn">◀◀</button>
      <button class="btn" id="play-btn">▶</button>
      <button class="btn" id="next-frame-btn">▶▶</button>
      <label style="font-size:12px;color:#888;">Speed:</label>
      <input type="range" id="speed-slider" min="10" max="1000" value="200">
    </div>
  </div>

  <!-- EXPORT MODAL -->
  <div class="modal-overlay" id="export-modal">
    <div class="modal">
      <h2>Export — C-Code</h2>
      <textarea id="export-textarea" readonly></textarea>
      <div class="modal-buttons">
        <button class="btn btn-primary" id="copy-btn">Kopieren</button>
        <button class="btn" id="export-close-btn">Schließen</button>
      </div>
    </div>
  </div>

  <!-- IMPORT MODAL -->
  <div class="modal-overlay" id="import-modal">
    <div class="modal">
      <h2>Import — C-Code einfügen</h2>
      <textarea id="import-textarea" placeholder="Hier den C-Array-Block aus AllPattern.h einfügen..."></textarea>
      <div id="import-warning"></div>
      <div class="modal-buttons">
        <button class="btn btn-primary" id="do-import-btn">Importieren</button>
        <button class="btn" id="import-close-btn">Abbrechen</button>
      </div>
    </div>
  </div>

  <script>
    // === CONSTANTS ===
    // === STATE ===
    // === RENDERING ===
    // === EVENT HANDLERS ===
    // === ANIMATION ===
    // === EXPORT ===
    // === IMPORT ===
    // === INIT ===
  </script>
</body>
</html>
```

- [ ] **Schritt 2: Im Browser öffnen und Layout prüfen**

Datei im Browser öffnen. Erwartetes Ergebnis:
- Dunkles Drei-Spalten-Layout sichtbar
- Topbar mit Buttons, Bottombar mit Einstellungen
- Zwei leere Modals (nicht sichtbar)
- Keine JavaScript-Fehler in der Browser-Konsole

---

## Task 2: Konstanten — Farben + Kreuz-Layout

**Files:**
- Modify: `pattern-editor.html` — `// === CONSTANTS ===` Sektion

- [ ] **Schritt 1: Farbkonstanten einfügen**

Den Kommentar `// === CONSTANTS ===` ersetzen durch:

```javascript
// === CONSTANTS ===

const COLORS = [
  { name: 'BLK', r:   0, g:   0, b:   0 },
  { name: 'RD1', r:  50, g:   0, b:   0 },
  { name: 'RD2', r: 100, g:   0, b:   0 },
  { name: 'RD3', r: 150, g:   0, b:   0 },
  { name: 'RD4', r: 200, g:   0, b:   0 },
  { name: 'RD5', r: 255, g:   0, b:   0 },
  { name: 'GN1', r:   0, g:  50, b:   0 },
  { name: 'GN2', r:   0, g: 100, b:   0 },
  { name: 'GN3', r:   0, g: 150, b:   0 },
  { name: 'GN4', r:   0, g: 200, b:   0 },
  { name: 'GN5', r:   0, g: 255, b:   0 },
  { name: 'BL1', r:   0, g:   0, b:  50 },
  { name: 'BL2', r:   0, g:   0, b: 100 },
  { name: 'BL3', r:   0, g:   0, b: 150 },
  { name: 'BL4', r:   0, g:   0, b: 200 },
  { name: 'BL5', r:   0, g:   0, b: 255 },
  { name: 'YL1', r:  50, g:  50, b:   0 },
  { name: 'YL2', r: 100, g: 100, b:   0 },
  { name: 'YL3', r: 150, g: 150, b:   0 },
  { name: 'YL4', r: 200, g: 200, b:   0 },
  { name: 'YL5', r: 255, g: 255, b:   0 },
  { name: 'CY1', r:   0, g:  50, b:  50 },
  { name: 'CY2', r:   0, g: 100, b: 100 },
  { name: 'CY3', r:   0, g: 150, b: 150 },
  { name: 'CY4', r:   0, g: 200, b: 200 },
  { name: 'CY5', r:   0, g: 255, b: 255 },
  { name: 'MG1', r:  50, g:   0, b:  50 },
  { name: 'MG2', r: 100, g:   0, b: 100 },
  { name: 'MG3', r: 150, g:   0, b: 150 },
  { name: 'MG4', r: 200, g:   0, b: 200 },
  { name: 'MG5', r: 255, g:   0, b: 255 },
  { name: 'WH1', r:  50, g:  50, b:  50 },
  { name: 'WH2', r: 100, g: 100, b: 100 },
  { name: 'WH3', r: 150, g: 150, b: 150 },
  { name: 'WH4', r: 200, g: 200, b: 200 },
  { name: 'WH5', r: 255, g: 255, b: 255 },
];

// Map color name → index (for Import parser)
const COLOR_INDEX = {};
COLORS.forEach((c, i) => { COLOR_INDEX[c.name] = i; });

// CSS color string from index
function colorCSS(idx) {
  const c = COLORS[idx] || COLORS[0];
  return `rgb(${c.r},${c.g},${c.b})`;
}
```

- [ ] **Schritt 2: Kreuz-Layout-Array aufbauen**

Direkt nach den Farbkonstanten einfügen:

```javascript
// CROSS_LAYOUT[ledIdx] = { row, col } im 18×18-Grid (1-basiert)
// ledIdx = LED-Nummer − 1 (0-basiert, 0..65)
const CROSS_LAYOUT = (function buildLayout() {
  const layout = new Array(66).fill(null);

  function place(leds, rowFn, colFn) {
    leds.forEach((led, i) => { layout[led - 1] = { row: rowFn(i), col: colFn(i) }; });
  }

  // Rechter Arm: PCB1-Front (rows 9-10, cols 11-18)
  place([8,7,6,5,4,3,2,1],           () => 9,  i => 11 + i);
  place([9,10,11,12,13,14,15,16],    () => 10, i => 11 + i);

  // Linker Arm: PCB1-Back (rows 9-10, cols 1-8)
  place([17,18,19,20,21,22,23,24],   () => 9,  i => 1 + i);
  place([32,31,30,29,28,27,26,25],   () => 10, i => 1 + i);

  // Oberer Arm: PCB2-Front (col 9-10, rows 1-8)
  place([33,34,35,36,37,38,39,40],   i => 1 + i, () => 9);
  place([48,47,46,45,44,43,42,41],   i => 1 + i, () => 10);

  // Unterer Arm: PCB2-Back (col 9-10, rows 11-18)
  place([56,55,54,53,52,51,50,49],   i => 11 + i, () => 9);
  place([57,58,59,60,61,62,63,64],   i => 11 + i, () => 10);

  // Mitte: Spitzen-LEDs
  layout[64] = { row: 9,  col: 9 };  // LED 65
  layout[65] = { row: 10, col: 9 };  // LED 66

  return layout;
})();
```

- [ ] **Schritt 3: Prüfen in der Browser-Konsole**

Seite neu laden. In der Browser-Konsole eingeben:
```javascript
console.log(CROSS_LAYOUT[0])   // LED 1 → sollte {row:9, col:18} sein
console.log(CROSS_LAYOUT[7])   // LED 8 → sollte {row:9, col:11} sein
console.log(CROSS_LAYOUT[64])  // LED 65 → sollte {row:9, col:9} sein
console.log(CROSS_LAYOUT[65])  // LED 66 → sollte {row:10, col:9} sein
console.log(CROSS_LAYOUT[32])  // LED 33 → sollte {row:1, col:9} sein
```

Alle Werte müssen exakt übereinstimmen. Keine Fehler in der Konsole.

---

## Task 3: State + Kreuz-Rendering

**Files:**
- Modify: `pattern-editor.html` — `// === STATE ===` und `// === RENDERING ===`

- [ ] **Schritt 1: State initialisieren**

`// === STATE ===` ersetzen durch:

```javascript
// === STATE ===

const state = {
  name: 'mypattern',
  ledLen: 66,
  wait: 100,
  frames: [ new Array(66).fill(0) ],  // Anfang: 1 Frame, alles BLK
  currentFrame: 0,
  activeColorIdx: 0,
  isPlaying: false,
  previewInterval: null,
};
```

- [ ] **Schritt 2: Kreuz rendern**

`// === RENDERING ===` ersetzen durch:

```javascript
// === RENDERING ===

function renderCross() {
  const grid = document.getElementById('cross-grid');
  grid.innerHTML = '';

  CROSS_LAYOUT.forEach((pos, ledIdx) => {
    if (!pos) return;
    const ledNum = ledIdx + 1;
    const colorIdx = getDisplayColor(ledIdx);
    const active = ledIdx < state.ledLen || state.isPlaying;

    const cell = document.createElement('div');
    cell.className = 'led-cell' + (active ? '' : ' led-inactive');
    cell.style.gridColumn = pos.col;
    cell.style.gridRow = pos.row;
    cell.style.backgroundColor = colorCSS(colorIdx);
    cell.title = `LED ${ledNum}: ${COLORS[colorIdx].name}`;
    cell.dataset.ledIdx = ledIdx;
    grid.appendChild(cell);
  });
}

// Welche Farbe zeigt LED ledIdx im aktuellen Frame?
// Im Vorschau-Modus: Pattern wird für alle 66 LEDs wiederholt.
// Im Editier-Modus: direkter Zugriff; LEDs > ledLen zeigen gespeicherten Wert (aber gedimmt).
function getDisplayColor(ledIdx) {
  const frame = state.frames[state.currentFrame];
  if (state.isPlaying) {
    return frame[ledIdx % state.ledLen];
  }
  return frame[ledIdx];
}

function renderAll() {
  renderCross();
  renderFrameList();
  renderPalette();
  syncSettings();
}
```

- [ ] **Schritt 3: `renderFrameList` und `renderPalette` als Platzhalter, `syncSettings` hinzufügen**

Direkt nach `renderAll` einfügen (werden in späteren Tasks ausgebaut):

```javascript
function renderFrameList() { /* Task 6 */ }
function renderPalette() { /* Task 5 */ }

function syncSettings() {
  document.getElementById('pattern-name').value = state.name;
  document.getElementById('ledlen-input').value = state.ledLen;
  document.getElementById('wait-input').value = state.wait;
}
```

- [ ] **Schritt 4: `// === INIT ===` hinzufügen und App starten**

`// === INIT ===` ersetzen durch:

```javascript
// === INIT ===
renderAll();
```

- [ ] **Schritt 5: Im Browser prüfen**

Seite neu laden. Erwartetes Ergebnis:
- Im Kreuz-Zentrum erscheinen 66 schwarze Quadrate an den richtigen Grid-Positionen
- Das Kreuz-Muster ist deutlich erkennbar (4 Arme + Mitte)
- Hover über eine LED-Zelle zeigt einen Tooltip wie "LED 1: BLK"
- Keine Fehler in der Konsole

Zur Schnellprüfung in der Konsole:
```javascript
// Testet ob alle 66 LEDs gerendert werden
document.querySelectorAll('.led-cell').length // Erwartet: 66
```

---

## Task 4: LED-Klick-Interaktion

**Files:**
- Modify: `pattern-editor.html` — `// === EVENT HANDLERS ===`

- [ ] **Schritt 1: Event-Handler für Kreuz-Klicks einfügen**

`// === EVENT HANDLERS ===` ersetzen durch:

```javascript
// === EVENT HANDLERS ===

// LED anklicken: linke Maustaste = aktive Farbe, rechte Maustaste = BLK
document.getElementById('cross-grid').addEventListener('click', e => {
  const cell = e.target.closest('.led-cell');
  if (!cell || state.isPlaying) return;
  const ledIdx = parseInt(cell.dataset.ledIdx);
  if (ledIdx >= state.ledLen) return;  // inaktive LED ignorieren
  state.frames[state.currentFrame][ledIdx] = state.activeColorIdx;
  renderCross();
  renderFrameList();  // Mini-Vorschau aktualisieren
});

document.getElementById('cross-grid').addEventListener('contextmenu', e => {
  e.preventDefault();
  const cell = e.target.closest('.led-cell');
  if (!cell || state.isPlaying) return;
  const ledIdx = parseInt(cell.dataset.ledIdx);
  if (ledIdx >= state.ledLen) return;
  state.frames[state.currentFrame][ledIdx] = 0;  // BLK
  renderCross();
  renderFrameList();
});
```

- [ ] **Schritt 2: Im Browser prüfen**

Seite neu laden. Manuell testen:
1. In der Browser-Konsole: `state.activeColorIdx = 5;` (RD5) eingeben
2. Auf eine LED-Zelle im Kreuz klicken → Zelle wird rot
3. Rechtsklick auf dieselbe Zelle → Zelle wird wieder schwarz
4. Tooltip zeigt aktualisierte Farbe beim nächsten Hover
5. Keine Fehler in der Konsole

---

## Task 5: Farbpalette

**Files:**
- Modify: `pattern-editor.html` — `renderPalette` Funktion + Event-Handler

- [ ] **Schritt 1: `renderPalette` implementieren**

Die Platzhalter-Funktion `function renderPalette() { /* Task 5 */ }` ersetzen durch:

```javascript
function renderPalette() {
  const container = document.getElementById('color-groups');
  container.innerHTML = '';

  const groups = [
    { label: 'Schwarz', indices: [0] },
    { label: 'Rot',     indices: [1,2,3,4,5] },
    { label: 'Grün',    indices: [6,7,8,9,10] },
    { label: 'Blau',    indices: [11,12,13,14,15] },
    { label: 'Gelb',    indices: [16,17,18,19,20] },
    { label: 'Cyan',    indices: [21,22,23,24,25] },
    { label: 'Magenta', indices: [26,27,28,29,30] },
    { label: 'Weiß',    indices: [31,32,33,34,35] },
  ];

  groups.forEach(group => {
    const div = document.createElement('div');
    div.className = 'color-group';
    div.innerHTML = `<div class="color-group-label">${group.label}</div><div class="color-swatches"></div>`;
    const swatchContainer = div.querySelector('.color-swatches');

    group.indices.forEach(idx => {
      const swatch = document.createElement('div');
      swatch.className = 'color-swatch' + (idx === state.activeColorIdx ? ' active' : '');
      swatch.style.backgroundColor = colorCSS(idx);
      swatch.title = COLORS[idx].name;
      swatch.dataset.colorIdx = idx;
      // BLK bekommt eine sichtbare Border
      if (idx === 0) swatch.style.border = '2px solid #555';
      swatchContainer.appendChild(swatch);
    });

    container.appendChild(div);
  });

  document.getElementById('active-color-label').textContent =
    'Aktiv: ' + COLORS[state.activeColorIdx].name;
}
```

- [ ] **Schritt 2: Klick-Handler für Palette hinzufügen**

Am Ende der `// === EVENT HANDLERS ===` Sektion einfügen:

```javascript
// Farbe aus Palette wählen
document.getElementById('palette-panel').addEventListener('click', e => {
  const swatch = e.target.closest('.color-swatch');
  if (!swatch) return;
  state.activeColorIdx = parseInt(swatch.dataset.colorIdx);
  renderPalette();
});
```

- [ ] **Schritt 3: Im Browser prüfen**

Seite neu laden. Erwartetes Ergebnis:
- Rechtes Panel zeigt 8 Farbgruppen mit je 1–5 farbigen Quadraten
- BLK-Swatch ist sichtbar (mit grauer Border)
- Klick auf RD3 (drittes rotes Quadrat): Rahmen wechselt zu RD3, Label zeigt "Aktiv: RD3"
- Danach LED im Kreuz anklicken: LED wird in RD3-Farbe (mittelrot) angezeigt

---

## Task 6: Frame-Liste

**Files:**
- Modify: `pattern-editor.html` — `renderFrameList` + Frame-Button-Handler + Drag & Drop

- [ ] **Schritt 1: Mini-Preview-Funktion implementieren**

Vor `renderFrameList` einfügen:

```javascript
function drawMiniPreview(canvas, frameIdx) {
  const ctx = canvas.getContext('2d');
  const cellSize = 2;
  ctx.fillStyle = '#111';
  ctx.fillRect(0, 0, canvas.width, canvas.height);

  CROSS_LAYOUT.forEach((pos, ledIdx) => {
    if (!pos) return;
    const colorIdx = state.frames[frameIdx][ledIdx % state.ledLen];
    const c = COLORS[colorIdx];
    ctx.fillStyle = `rgb(${c.r},${c.g},${c.b})`;
    ctx.fillRect((pos.col - 1) * cellSize, (pos.row - 1) * cellSize, cellSize, cellSize);
  });
}
```

- [ ] **Schritt 2: `renderFrameList` implementieren**

Die Platzhalter-Funktion ersetzen durch:

```javascript
function renderFrameList() {
  const list = document.getElementById('frame-list');
  list.innerHTML = '';

  state.frames.forEach((frame, idx) => {
    const item = document.createElement('div');
    item.className = 'frame-item' + (idx === state.currentFrame ? ' active' : '');
    item.dataset.frameIdx = idx;
    item.draggable = true;

    const numSpan = document.createElement('span');
    numSpan.className = 'frame-num';
    numSpan.textContent = idx + 1;

    const canvas = document.createElement('canvas');
    canvas.width = 36;   // 18 cols × 2px
    canvas.height = 36;  // 18 rows × 2px

    item.appendChild(numSpan);
    item.appendChild(canvas);
    list.appendChild(item);

    drawMiniPreview(canvas, idx);
  });

  // Aktiven Frame in Sicht scrollen
  const activeItem = list.querySelector('.frame-item.active');
  if (activeItem) activeItem.scrollIntoView({ block: 'nearest' });
}
```

- [ ] **Schritt 3: Frame-Klick und Button-Handler einfügen**

Am Ende der `// === EVENT HANDLERS ===` Sektion einfügen:

```javascript
// Frame-Auswahl per Klick
document.getElementById('frame-list').addEventListener('click', e => {
  const item = e.target.closest('.frame-item');
  if (!item) return;
  state.currentFrame = parseInt(item.dataset.frameIdx);
  renderCross();
  renderFrameList();
});

// Frame hinzufügen (Kopie des aktuellen)
document.getElementById('frame-add-btn').addEventListener('click', () => {
  const copy = [...state.frames[state.currentFrame]];
  state.frames.splice(state.currentFrame + 1, 0, copy);
  state.currentFrame += 1;
  renderAll();
});

// Frame löschen
document.getElementById('frame-del-btn').addEventListener('click', () => {
  if (state.frames.length === 1) return;  // mind. 1 Frame behalten
  state.frames.splice(state.currentFrame, 1);
  state.currentFrame = Math.min(state.currentFrame, state.frames.length - 1);
  renderAll();
});

// Frame duplizieren (fügt Kopie am Ende ein)
document.getElementById('frame-dup-btn').addEventListener('click', () => {
  const copy = [...state.frames[state.currentFrame]];
  state.frames.push(copy);
  state.currentFrame = state.frames.length - 1;
  renderAll();
});
```

- [ ] **Schritt 4: Drag & Drop für Frame-Umsortierung einfügen**

Am Ende der `// === EVENT HANDLERS ===` Sektion einfügen:

```javascript
// Drag & Drop zum Umsortieren
let dragSrcIdx = null;

document.getElementById('frame-list').addEventListener('dragstart', e => {
  const item = e.target.closest('.frame-item');
  if (!item) return;
  dragSrcIdx = parseInt(item.dataset.frameIdx);
  e.dataTransfer.effectAllowed = 'move';
});

document.getElementById('frame-list').addEventListener('dragover', e => {
  e.preventDefault();
  e.dataTransfer.dropEffect = 'move';
});

document.getElementById('frame-list').addEventListener('drop', e => {
  e.preventDefault();
  const item = e.target.closest('.frame-item');
  if (!item || dragSrcIdx === null) return;
  const targetIdx = parseInt(item.dataset.frameIdx);
  if (dragSrcIdx === targetIdx) return;

  const moved = state.frames.splice(dragSrcIdx, 1)[0];
  state.frames.splice(targetIdx, 0, moved);
  state.currentFrame = targetIdx;
  dragSrcIdx = null;
  renderAll();
});
```

- [ ] **Schritt 5: Im Browser prüfen**

Seite neu laden. Erwartetes Ergebnis:
- Frame-Liste zeigt "1" mit schwarzem Mini-Kreuz
- Klick auf "+" fügt Frame 2 hinzu (Kopie), Frame 2 wird aktiv
- RD5 auf eine LED setzen → Mini-Vorschau von Frame 2 zeigt roten Punkt an dieser Position
- "−" löscht Frame 2 (Frame 1 bleibt)
- Bei einem Frame: "−" hat keinen Effekt
- "⧉" dupliziert den aktuellen Frame ans Ende

---

## Task 7: Settings-Bar + LEDLEN-Verhalten

**Files:**
- Modify: `pattern-editor.html` — Settings-Inputs + LEDLEN-Logik

- [ ] **Schritt 1: Settings-Input-Handler einfügen**

Am Ende der `// === EVENT HANDLERS ===` Sektion einfügen:

```javascript
// Settings
document.getElementById('pattern-name').addEventListener('input', e => {
  state.name = e.target.value;
});

document.getElementById('ledlen-input').addEventListener('change', e => {
  const val = Math.max(1, Math.min(66, parseInt(e.target.value) || 66));
  state.ledLen = val;
  e.target.value = val;
  renderCross();
});

document.getElementById('wait-input').addEventListener('change', e => {
  state.wait = Math.max(1, parseInt(e.target.value) || 100);
  e.target.value = state.wait;
});
```

- [ ] **Schritt 2: Im Browser prüfen**

Seite neu laden. Testen:
1. LEDLEN auf 10 setzen → LEDs 11–66 erscheinen gedimmt (opacity 0.2), nicht anklickbar
2. LED 5 anklicken → Farbe ändert sich (aktiv)
3. LED 15 anklicken → keine Reaktion (inaktiv)
4. LEDLEN zurück auf 66 → alle LEDs wieder aktiv
5. Pattern-Name eingeben → kein Fehler

---

## Task 8: Animationsvorschau

**Files:**
- Modify: `pattern-editor.html` — `// === ANIMATION ===` + Button-Handler

- [ ] **Schritt 1: Animation-Logik implementieren**

`// === ANIMATION ===` ersetzen durch:

```javascript
// === ANIMATION ===

function startPreview() {
  state.isPlaying = true;
  document.getElementById('play-btn').textContent = '❙❙';
  document.getElementById('preview-btn').textContent = '❙❙ Stop';
  scheduleNextFrame();
}

function stopPreview() {
  state.isPlaying = false;
  if (state.previewInterval) { clearTimeout(state.previewInterval); state.previewInterval = null; }
  document.getElementById('play-btn').textContent = '▶';
  document.getElementById('preview-btn').textContent = '▶ Vorschau';
  renderCross();
}

function scheduleNextFrame() {
  const speed = parseInt(document.getElementById('speed-slider').value);
  state.previewInterval = setTimeout(() => {
    if (!state.isPlaying) return;
    state.currentFrame = (state.currentFrame + 1) % state.frames.length;
    renderCross();
    renderFrameList();
    scheduleNextFrame();
  }, speed);
}
```

- [ ] **Schritt 2: Animation-Button-Handler einfügen**

Am Ende der `// === EVENT HANDLERS ===` Sektion einfügen:

```javascript
// Vorschau (Topbar-Button)
document.getElementById('preview-btn').addEventListener('click', () => {
  state.isPlaying ? stopPreview() : startPreview();
});

// Play/Pause in der Bottombar
document.getElementById('play-btn').addEventListener('click', () => {
  state.isPlaying ? stopPreview() : startPreview();
});

// Frame-für-Frame navigieren
document.getElementById('prev-frame-btn').addEventListener('click', () => {
  if (state.isPlaying) return;
  state.currentFrame = (state.currentFrame - 1 + state.frames.length) % state.frames.length;
  renderCross();
  renderFrameList();
});

document.getElementById('next-frame-btn').addEventListener('click', () => {
  if (state.isPlaying) return;
  state.currentFrame = (state.currentFrame + 1) % state.frames.length;
  renderCross();
  renderFrameList();
});
```

- [ ] **Schritt 3: Im Browser prüfen**

Mehrere Frames anlegen (z.B. 4), je einen anderen Arm einfärben. Dann:
1. "▶ Vorschau" klicken → Button wird "❙❙ Stop", Frames wechseln automatisch
2. Bei LEDLEN < 66: Vorschau zeigt alle 66 LEDs mit wiederholtem Muster (keine gedimmten Zellen)
3. Speed-Slider nach links → schnellere Animation
4. Speed-Slider nach rechts → langsame Animation
5. "❙❙ Stop" → Animation stoppt, Bearbeitung wieder möglich
6. ◀◀ / ▶▶ navigieren Frame für Frame (nur wenn nicht spielend)

---

## Task 9: Symmetrie-Hilfe

**Files:**
- Modify: `pattern-editor.html` — Sym-Button-Handler

- [ ] **Schritt 1: Symmetrie-Button-Handler einfügen**

Am Ende der `// === EVENT HANDLERS ===` Sektion einfügen:

```javascript
// PCB1 (LEDs 1-32, Indices 0-31) → PCB2 (LEDs 33-64, Indices 32-63)
document.getElementById('sym-btn').addEventListener('click', () => {
  const frame = state.frames[state.currentFrame];
  for (let i = 0; i < 32; i++) {
    frame[32 + i] = frame[i];
  }
  renderCross();
  renderFrameList();
});
```

- [ ] **Schritt 2: Im Browser prüfen**

1. Einige LEDs im linken/rechten Arm (PCB1, LEDs 1–32) einfärben
2. "PCB1 → PCB2" klicken
3. Oberer/unterer Arm (PCB2, LEDs 33–64) zeigt identisches Muster
4. Mini-Vorschau des Frames aktualisiert sich ebenfalls

---

## Task 10: Export

**Files:**
- Modify: `pattern-editor.html` — `// === EXPORT ===` + Modal-Handler

- [ ] **Schritt 1: Export-Funktion implementieren**

`// === EXPORT ===` ersetzen durch:

```javascript
// === EXPORT ===

function generateExportCode() {
  const rawName = state.name.trim().replace(/\s+/g, '_') || 'mypattern';
  const upperName = rawName.toUpperCase();
  const lowerName = rawName.toLowerCase();
  const ledLen = state.ledLen;
  const numFrames = state.frames.length;

  // Header
  let code = `// ============ ${upperName} ===================\n\n`;
  code += `#define LEDLEN${upperName} ${ledLen}\n`;
  code += `#define LEN${upperName}    ${numFrames}\n`;
  code += `#define WAIT${upperName}   ${state.wait}\n\n`;
  code += `int ${lowerName} [LEN${upperName}] [LEDLEN${upperName}] = {\n\n`;

  // Spalten-Kommentar
  const headerNums = Array.from({ length: ledLen }, (_, i) =>
    String(i + 1).padStart(4)
  ).join('');
  code += `    //${headerNums}\n`;

  // Frames
  state.frames.forEach((frame, fi) => {
    const values = frame.slice(0, ledLen)
      .map(ci => COLORS[ci].name.padStart(3))
      .join(', ');
    const comma = fi < numFrames - 1 ? ',' : '';
    code += `    { ${values} }${comma}\n`;
  });

  code += `\n};\n`;
  return code;
}
```

- [ ] **Schritt 2: Export-Modal-Handler einfügen**

Am Ende der `// === EVENT HANDLERS ===` Sektion einfügen:

```javascript
// Export
document.getElementById('export-btn').addEventListener('click', () => {
  document.getElementById('export-textarea').value = generateExportCode();
  document.getElementById('export-modal').classList.add('open');
});

document.getElementById('export-close-btn').addEventListener('click', () => {
  document.getElementById('export-modal').classList.remove('open');
});

document.getElementById('copy-btn').addEventListener('click', () => {
  const ta = document.getElementById('export-textarea');
  navigator.clipboard.writeText(ta.value).then(() => {
    const btn = document.getElementById('copy-btn');
    btn.textContent = '✓ Kopiert!';
    setTimeout(() => { btn.textContent = 'Kopieren'; }, 2000);
  });
});
```

- [ ] **Schritt 3: Im Browser prüfen**

1. Pattern-Name "testmuster" eingeben, LEDLEN=10, WAIT=50
2. Einige LEDs einfärben, 3 Frames anlegen
3. "↑ Export" klicken → Modal öffnet sich
4. Code beginnt mit `// ============ TESTMUSTER ===================`
5. `#define LEDLENTESTMUSTER 10` ist korrekt
6. `#define LENTESTMUSTER    3` ist korrekt
7. Jede Frame-Zeile hat genau 10 Farbwerte
8. "Kopieren" klicken → Button zeigt kurz "✓ Kopiert!"
9. In `AllPattern.h` einfügen und überprüfen, ob der Code syntaktisch gültig ist

---

## Task 11: Import

**Files:**
- Modify: `pattern-editor.html` — `// === IMPORT ===` + Modal-Handler

- [ ] **Schritt 1: Import-Parser implementieren**

`// === IMPORT ===` ersetzen durch:

```javascript
// === IMPORT ===

function parseImportCode(code) {
  const warnings = [];

  // LEDLEN
  const ledLenMatch = code.match(/#define\s+LEDLEN\w+\s+(\d+)/);
  const ledLen = ledLenMatch ? parseInt(ledLenMatch[1]) : 66;

  // WAIT
  const waitMatch = code.match(/#define\s+WAIT\w+\s+(\d+)/);
  const wait = waitMatch ? parseInt(waitMatch[1]) : 100;

  // Pattern-Name (aus Arrayname: "int NAME [")
  const nameMatch = code.match(/int\s+(\w+)\s*\[/);
  const name = nameMatch ? nameMatch[1] : 'imported';

  // Frames: alle { ... } Blöcke extrahieren
  // Kommentare (// ...) vorher entfernen
  const noComments = code.replace(/\/\/[^\n]*/g, '');
  const frames = [];
  const frameRegex = /\{([^}]+)\}/g;
  let match;

  while ((match = frameRegex.exec(noComments)) !== null) {
    const tokens = match[1]
      .split(',')
      .map(t => t.trim())
      .filter(t => t.length > 0);

    const frameData = new Array(66).fill(0);
    tokens.forEach((token, i) => {
      if (i >= ledLen) return;
      const idx = COLOR_INDEX[token];
      if (idx !== undefined) {
        frameData[i] = idx;
      } else {
        warnings.push(`Unbekannte Farbe: "${token}" → BLK`);
        frameData[i] = 0;
      }
    });

    frames.push(frameData);
  }

  if (frames.length === 0) {
    return { error: 'Keine Frame-Daten gefunden. Bitte vollständigen C-Array-Block einfügen.' };
  }

  return { name, ledLen, wait, frames, warnings };
}
```

- [ ] **Schritt 2: Import-Modal-Handler einfügen**

Am Ende der `// === EVENT HANDLERS ===` Sektion einfügen:

```javascript
// Import
document.getElementById('import-btn').addEventListener('click', () => {
  document.getElementById('import-textarea').value = '';
  document.getElementById('import-warning').style.display = 'none';
  document.getElementById('import-modal').classList.add('open');
});

document.getElementById('import-close-btn').addEventListener('click', () => {
  document.getElementById('import-modal').classList.remove('open');
});

document.getElementById('do-import-btn').addEventListener('click', () => {
  const code = document.getElementById('import-textarea').value;
  const result = parseImportCode(code);
  const warningEl = document.getElementById('import-warning');

  if (result.error) {
    warningEl.textContent = '⚠ ' + result.error;
    warningEl.style.display = 'block';
    return;
  }

  const hasData = state.frames.length > 1 ||
    state.frames[0].some(v => v !== 0);

  if (hasData && !confirm('Aktuelles Muster wird überschrieben. Fortfahren?')) return;

  state.name    = result.name;
  state.ledLen  = result.ledLen;
  state.wait    = result.wait;
  state.frames  = result.frames;
  state.currentFrame = 0;

  document.getElementById('import-modal').classList.remove('open');

  if (result.warnings.length > 0) {
    alert('Import erfolgreich, aber mit Warnungen:\n' + result.warnings.slice(0, 10).join('\n'));
  }

  renderAll();
});
```

- [ ] **Schritt 3: Im Browser prüfen — Import von bestehendem Muster**

Den folgenden C-Block aus `AllPattern.h` kopieren (z.B. den Anfang von `offpattern`) und in den Import-Dialog einfügen:

```c
// ============ OFFPATTERN ===================

#define LEDLENOFFPATTERN  3
#define LENOFFPATTERN     1
#define WAITOFFPATTERN   50

int offpattern [LENOFFPATTERN] [LEDLENOFFPATTERN] = {
    { BLK, BLK, BLK }
};
```

Erwartetes Ergebnis:
- Pattern-Name wird "offpattern"
- LEDLEN = 3
- WAIT = 50
- 1 Frame, alle LEDs auf BLK
- Alle LEDs > 3 erscheinen gedimmt

- [ ] **Schritt 4: Import-Export-Kreistest**

1. Beliebiges Muster aus `AllPattern.h` importieren (z.B. `blinkpattern`)
2. Editor zeigt das Muster korrekt im Kreuz
3. Sofort re-exportieren
4. Den exportierten Code mit dem Original in `AllPattern.h` vergleichen — Farbwerte müssen identisch sein (Leerzeichen und Zeilenumbrüche dürfen sich unterscheiden)

---

## Selbstprüfung gegen den Spec

| Spec-Anforderung | Task | Status |
|---|---|---|
| Single HTML-Datei, kein Server | Task 1 | ✓ |
| 36 Farben korrekt (exakte RGB-Werte) | Task 2 | ✓ |
| Kreuz-Draufsicht, 66 LEDs, korrekte Position | Task 2+3 | ✓ |
| LED-Mapping physisch korrekt (PCB1-Back gespiegelt) | Task 2 | ✓ |
| Linksklick = Farbe setzen | Task 4 | ✓ |
| Rechtsklick = BLK | Task 4 | ✓ |
| Tooltip LED-Nr + Farbname | Task 3 | ✓ |
| Farbpalette mit 36 Farben, gruppiert | Task 5 | ✓ |
| Aktive Farbe markiert | Task 5 | ✓ |
| Frame-Liste nummeriert | Task 6 | ✓ |
| Mini-Vorschau je Frame | Task 6 | ✓ |
| Frame hinzufügen (Kopie) / löschen / duplizieren | Task 6 | ✓ |
| Drag & Drop Umsortierung | Task 6 | ✓ |
| LEDLEN-Eingabe + Grayout in Bearbeitung | Task 7 | ✓ |
| LEDLEN: Vorschau zeigt Repeat für alle 66 LEDs | Task 8 | ✓ |
| Animationsvorschau Play/Pause | Task 8 | ✓ |
| Geschwindigkeits-Slider | Task 8 | ✓ |
| Frame-Liste scrollt mit in Vorschau | Task 8 | ✓ |
| ◀◀ / ▶▶ Frame-Navigation | Task 8 | ✓ |
| Symmetrie-Button PCB1→PCB2 | Task 9 | ✓ |
| Export: vollständiger C-Code | Task 10 | ✓ |
| Export: Spalten-Kommentar mit LED-Nummern | Task 10 | ✓ |
| Export: Kopieren-Button | Task 10 | ✓ |
| Import: C-Array-Block parsen | Task 11 | ✓ |
| Import: Name/LEDLEN/WAIT/Frames | Task 11 | ✓ |
| Import: Bestätigungsdialog bei vorhandenen Daten | Task 11 | ✓ |
| Import: Warnung bei unbekannten Token | Task 11 | ✓ |
