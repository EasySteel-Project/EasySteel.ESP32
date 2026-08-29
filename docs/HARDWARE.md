# EasySteel Hardware Specification

This document is the authoritative hardware reference for the base station, target modules, pinouts, and field cabling. Do not construct systems from older, simplified sketches.

> ⚠️ **MANDATORY PRE-COMMISSIONING CHECKS:**
> Before initial power-up, perform a multimeter continuity test to verify there is no short circuit between 5 V and GND. Next, with the ESP32 removed, verify that no level-shifter sensor output exceeds 3.3 V. Never apply 5 V directly to an ESP32 GPIO pin.

---

## 1. System Architecture & Signal Flow

The system comprises a central **Base Station (ESP32 Controller)** and up to **5 Target Modules (Steel Plates)** connected via standard **RJ45 patch cables**.

```mermaid
graph LR
    subgraph TargetModule["Target Module 1..5 (at Steel Plate)"]
        PZ["Piezo Disc Element"]
        LM["LM393 Comparator Module<br/>(Threshold Potentiometer)"]
        DLED["Duo-LED Red/Green (1x)<br/>(Common Anode +5V)"]
        PZ -->|"Shock Pulse"| LM
    end

    subgraph Cabling["Field Cabling"]
        RJ["5x RJ45 Patch Cables (Cat5e/6)"]
    end

    subgraph BaseStation["Base Station (Central Controller)"]
        LVL["TXS0108E Level Shifter<br/>(5V DO → 3.3V Logic)"]
        ESP["ESP32 NodeMCU (30-Pin)<br/>• Hardware Interrupts (IRAM_ATTR)<br/>• Wi-Fi/TCP Client<br/>• State Machine"]
        ULN["ULN2803A Darlington Array<br/>(Low-Side Cathode Sink)"]
        DISP["SH1106 OLED 128×64 (I2C)"]
        BTN["Start, Stop & Reset Buttons"]
        BUZ["5V Active Piezo Buzzer"]
    end

    LM -->|"Pin 1 (5V DO)"| RJ
    DLED -->|"Pin 3 (Red) / Pin 6 (Green)"| RJ
    RJ -->|"5V Sensor Signals"| LVL
    LVL -->|"3.3V Digital"| ESP
    ESP -->|"3.3V Output"| ULN
    ULN -->|"GND Sink (Red/Green)"| RJ
    ESP -->|"I2C"| DISP
    BTN -->|"GND"| ESP
    ESP -->|"GPIO"| BUZ
```

---

## 2. Bill of Materials (BOM)

### A. Base Station (Central Controller)

| Component | Qty | Description / Specification |
|---|:---:|---|
| **ESP32 NodeMCU** | 1x | 30-pin variant with expansion base board (set power jumper to 5V) |
| **TXS0108E Module** | 1x | 8-channel bidirectional logic level shifter (replaces discrete voltage dividers) |
| **ULN2803A IC** | 1x | 8-channel Darlington transistor array for low-side cathode switching |
| **RJ45 Breakout Boards** | 5x | 8P8C RJ45 breakout jacks for Target 1 to 5 cables |
| **OLED Display** | 1x | SH1106, 128×64 pixels, I2C, address `0x3C`, 3.3V compatible |
| **Push Buttons** | 3x | Momentary NO: Green (Start), Red (Stop/Penalty), Black (Reset/Score) |
| **Buzzer** | 1x | 5V active piezo buzzer (start tone and acoustic feedback) |
| **Power Supply** | 1x | 9V / 12V DC power supply connected to base board barrel jack |

### B. Per Target Module (Target 1 to 5)

| Component | Qty | Description / Specification |
|---|:---:|---|
| **LM393 Sensor Module** | 1x | Comparator module with digital output (DO). *Note: Desolder original onboard buzzer.* |
| **Piezo Disc** | 1x | 20mm–27mm bare piezo element soldered directly to LM393 buzzer pads |
| **Duo-LED Red/Green** | 1x | **Common Anode (+)**. Recommended: 10mm diffused for distance visibility |
| **Series Resistor Red** | 1x | **~100 Ω** for the red cathode |
| **Series Resistor Green** | 1x | **~56 Ω** for the green cathode |
| **RJ45 Breakout Board** | 1x | RJ45 socket inside the target enclosure for patch cable connection |

> **Note on LED series resistors:** `100 Ω` for Red and `56 Ω` for Green are tested values from the reference hardware build. For different LED models, batches, or supply voltages, verify forward voltage and current limits from the datasheet and measure operating current on the assembled target.

---

## 3. RJ45 Pin Assignment (Ports 1 to 5)

Each target connects to the base station via a standard 8-conductor network cable (Cat5e / Cat6):

```
                          RJ45 Jack / Plug
                       ┌─────────────────────────┐
                       │  1  2  3  4  5  6  7  8 │
                       └──┴──┴──┴──┴──┴──┴──┴──┴─┘
```

| RJ45 Pin | Color Code (T568B) | Signal | Function & Wiring |
|:---:|---|---|---|
| **Pin 1** | White / Orange | **Sensor Signal (5V DO)** | Digital output from LM393 $\rightarrow$ TXS0108E (Channel B) |
| **Pin 2** | Orange | **GND** | Common ground (bridged to Pin 7+8) $\rightarrow$ LM393 GND |
| **Pin 3** | White / Green | **Red Signal (Cathode)** | Switched ground from ULN2803A OUT 1 $\rightarrow$ Resistors (~100Ω) $\rightarrow$ Red Cathodes |
| **Pin 4** | Blue | **+5V Supply** | Common positive (bridged to Pin 5) $\rightarrow$ LM393 VCC & Common LED Anodes |
| **Pin 5** | White / Blue | **+5V Supply** | Bridge to Pin 4 (for higher current capacity) |
| **Pin 6** | Green | **Green Signal (Cathode)** | Switched ground from ULN2803A OUT 2..6 $\rightarrow$ Resistors (~56Ω) $\rightarrow$ Green Cathodes |
| **Pin 7** | White / Brown | **GND** | Bridge to Pin 2 |
| **Pin 8** | Brown | **GND** | Bridge to Pin 2 |

---

## 4. Schematics & Wiring

### 4.1 Target Module (Wiring at Steel Plate)

```
======================================================================
RJ45 [Pins 4+5 bridged] (+5V)
 │
 ├───> LM393 Module [VCC Pin]
 └───> [Duo-LED Anode / Common Pin +]

RJ45 [Pins 2+7+8 bridged] (GND)
 │
 └───> LM393 Module [GND Pin]

RJ45 [Pin 1] (Sensor DO)
 │
 └───> LM393 Module [DO Pin]  (Piezo disc soldered to buzzer pads)

RJ45 [Pin 3] (Red signal from ULN2803A)
 │
 └───> [ ~100 Ω ] ───> [Duo-LED Red Cathode -]

RJ45 [Pin 6] (Green signal from ULN2803A)
 │
 └───> [ ~56 Ω ] ───> [Duo-LED Green Cathode -]
======================================================================
```

---

### 4.2 Base Station

#### 1. Sensor Level Shifting with TXS0108E

The 5V digital outputs from the LM393 modules pass through a TXS0108E level shifter to the 3.3V ESP32 inputs. Verify output voltage on each channel before connecting the ESP32:

```
  5V Power Rail ──────────> TXS0108E VCCB (5V side)
  3.3V (ESP32 3V3) ───────> TXS0108E VCCA (3.3V side)
  3.3V (ESP32 3V3) ───────> TXS0108E OE   (Output Enable active)
  GND Rail ───────────────> TXS0108E GND

  5V Signals from Targets         TXS0108E Shifter        3.3V Inputs on ESP32
  ───────────────────────         ────────────────        ────────────────────
  RJ45 Port 1 [Pin 1] (Stop) ───> B1 ─────── A1 ────────> ESP32 Pin 32 (Stop Plate)
  RJ45 Port 2 [Pin 1] (Tgt 2) ──> B2 ─────── A2 ────────> ESP32 Pin 33 (Sensor 2)
  RJ45 Port 3 [Pin 1] (Tgt 3) ──> B3 ─────── A3 ────────> ESP32 Pin 25 (Sensor 3)
  RJ45 Port 4 [Pin 1] (Tgt 4) ──> B4 ─────── A4 ────────> ESP32 Pin 26 (Sensor 4)
  RJ45 Port 5 [Pin 1] (Tgt 5) ──> B5 ─────── A5 ────────> ESP32 Pin 27 (Sensor 5)
```

#### 2. LED & Buzzer Drivers (ULN2803A)

The ULN2803A sinks LED and buzzer cathodes to ground when the ESP32 drives the corresponding pin `HIGH`:

```
  GND Rail ───────────────> ULN2803A Pin 9  (GND)
  5V Rail ────────────────> ULN2803A Pin 10 (COM / Flyback Diodes)

  ESP32 Control Signal            ULN2803A Driver         Output to Target Wiring
  ────────────────────            ───────────────         ───────────────────────
  ESP32 Pin 4  (Global Red) ────> IN 1 ──── OUT 1 (P18) ─> ALL RJ45 Ports 1 to 5 [Pin 3]
  ESP32 Pin 19 (Green Target 1) > IN 2 ──── OUT 2 (P17) ─> RJ45 Port 1 [Pin 6]
  ESP32 Pin 18 (Green Target 2) > IN 3 ──── OUT 3 (P16) ─> RJ45 Port 2 [Pin 6]
  ESP32 Pin 5  (Green Target 3) > IN 4 ──── OUT 4 (P15) ─> RJ45 Port 3 [Pin 6]
  ESP32 Pin 17 (Green Target 4) > IN 5 ──── OUT 5 (P14) ─> RJ45 Port 4 [Pin 6]
  ESP32 Pin 16 (Green Target 5) > IN 6 ──── OUT 6 (P13) ─> RJ45 Port 5 [Pin 6]
  ESP32 Pin 23 (Buzzer) ────────> IN 7 ──── OUT 7 (P12) ─> Buzzer (-) Cathode
```

> **Buzzer on ULN2803A (Channel 7):** Designed for a 5V active buzzer. Connect anode to the protected 5V rail; cathode is sinked through ULN2803A. Passive speakers or 12V sirens are not supported in the standard design.

#### 3. Buttons & Display

```
  ESP32 Pin 14 ───────────> Button 1 (Start / Green) ───────> GND Rail (Active LOW)
  ESP32 Pin 12 ───────────> Button 2 (Stop/Penalty / Red) ──> GND Rail (Active LOW)
  ESP32 Pin 13 ───────────> Button 3 (Reset/Score / Black) ─> GND Rail (Active LOW)

  ESP32 Pin 21 (SDA) ─────> OLED Display SDA
  ESP32 Pin 22 (SCL) ─────> OLED Display SCL
  3.3V / GND ─────────────> OLED Display VCC / GND
```

---

## 5. ESP32 Pinout Overview

```
                         ┌───────────────┐
                         │     ESP32     │
                         │    30-Pin     │
                         │               │
                   EN ── [   EN     D23  ] ─→ ULN2803A IN 7 (Buzzer)
                 (VP) ── [   VP     D22  ] ─→ OLED SCL (I²C)
                 (VN) ── [   VN     TX0  ] ── (Serial TX Debug)
               (open) ── [  D34     RX0  ] ── (Serial RX Debug)
               (open) ── [  D35     D21  ] ─→ OLED SDA (I²C)
 (TXS A1 / Port 1) IO32 ←─ [  D32     D19  ] ─→ ULN2803A IN 2 (LED Green 1)
 (TXS A2 / Port 2) IO33 ←─ [  D33     D18  ] ─→ ULN2803A IN 3 (LED Green 2)
 (TXS A3 / Port 3) IO25 ←─ [  D25      D5  ] ─→ ULN2803A IN 4 (LED Green 3)
 (TXS A4 / Port 4) IO26 ←─ [  D26     D17  ] ─→ ULN2803A IN 5 (LED Green 4)
 (TXS A5 / Port 5) IO27 ←─ [  D27     D16  ] ─→ ULN2803A IN 6 (LED Green 5)
     (BTN Start)  IO14 ←─ [  D14      D4  ] ─→ ULN2803A IN 1 (LED Global Red)
      (BTN Stop)  IO12 ←─ [  D12      D2  ] ── Built-in LED (Heartbeat)
     (BTN Reset)  IO13 ←─ [  D13     D15  ] ── (open)
                  GND ── [  GND     GND  ] ── Common Ground (GND)
                   5V ── [  VIN     3V3  ] ── 3.3V Output (to TXS VCCA & OLED)
                         │               │
                         │   [  USB  ]   │
                         └───────────────┘
```

| Pin | Direction | Function | Connected Device / Signal |
|:---:|:---:|---|---|
| **GPIO 32** | Input (ISR) | **Sensor 1 (Stop Plate)** | TXS0108E A1 $\leftarrow$ B1 $\leftarrow$ RJ45 Port 1 [Pin 1] |
| **GPIO 33** | Input (ISR) | **Sensor 2** | TXS0108E A2 $\leftarrow$ B2 $\leftarrow$ RJ45 Port 2 [Pin 1] |
| **GPIO 25** | Input (ISR) | **Sensor 3** | TXS0108E A3 $\leftarrow$ B3 $\leftarrow$ RJ45 Port 3 [Pin 1] |
| **GPIO 26** | Input (ISR) | **Sensor 4** | TXS0108E A4 $\leftarrow$ B4 $\leftarrow$ RJ45 Port 4 [Pin 1] |
| **GPIO 27** | Input (ISR) | **Sensor 5** | TXS0108E A5 $\leftarrow$ B5 $\leftarrow$ RJ45 Port 5 [Pin 1] |
| **GPIO 4**  | Output | **Global Red LED** | ULN2803A IN 1 $\rightarrow$ OUT 1 $\rightarrow$ ALL RJ45 [Pin 3] |
| **GPIO 19** | Output | **Green LED Target 1** | ULN2803A IN 2 $\rightarrow$ OUT 2 $\rightarrow$ RJ45 Port 1 [Pin 6] |
| **GPIO 18** | Output | **Green LED Target 2** | ULN2803A IN 3 $\rightarrow$ OUT 3 $\rightarrow$ RJ45 Port 2 [Pin 6] |
| **GPIO 5**  | Output | **Green LED Target 3** | ULN2803A IN 4 $\rightarrow$ OUT 4 $\rightarrow$ RJ45 Port 3 [Pin 6] |
| **GPIO 17** | Output | **Green LED Target 4** | ULN2803A IN 5 $\rightarrow$ OUT 5 $\rightarrow$ RJ45 Port 4 [Pin 6] |
| **GPIO 16** | Output | **Green LED Target 5** | ULN2803A IN 6 $\rightarrow$ OUT 6 $\rightarrow$ RJ45 Port 5 [Pin 6] |
| **GPIO 23** | Output | **Piezo Buzzer** | ULN2803A IN 7 $\rightarrow$ OUT 7 $\rightarrow$ Buzzer (-) |
| **GPIO 14** | Input (Pullup) | **Start Button (Green)** | Green Push Button $\rightarrow$ GND |
| **GPIO 12** | Input (Pullup) | **Stop/Penalty Button (Red)** | Red Push Button $\rightarrow$ GND |
| **GPIO 13** | Input (Pullup) | **Reset/Score Button (Black)**| Black Push Button $\rightarrow$ GND (Deep Sleep Wakeup) |
| **GPIO 21** | I2C | **OLED SDA** | Display SDA |
| **GPIO 22** | I2C | **OLED SCL** | Display SCL |

---

## 6. Electrical Dimensioning & Commissioning Checklist

Before field deployment, verify and log the following checks:

1. Continuity and short-circuit test across all 5V, 3.3V, and GND rails.
2. Output voltage measurement on each TXS0108E channel without the ESP32 plugged in; maximum 3.3V at GPIO pins.
3. Common ground between power supply, base station, and all target modules.
4. Separate series resistors for red and green cathodes of the Duo-LED.
5. Measure current draw per target module and total current with all LEDs illuminated.
6. Ensure power supply, wiring, connectors, and PCB traces have adequate safety margins and fuse protection on the 5V rail.
7. Clearly label RJ45 cables as EasySteel field wiring. They carry power and GPIO signals and must never be plugged into Ethernet equipment.
8. Verify strain relief, contact protection, weatherproofing, and impact shielding.
9. Test signal edges, noise immunity, and false triggers with the planned maximum cable length.

The current rating of an individual ULN2803A channel cannot be multiplied across all 8 channels without considering total thermal package dissipation and ambient temperatures.

## 7. Manufacturing Documentation Pending

The following production assets are tracked for future public releases:

- Verified schematic replacing the wiring illustration;
- PCB / perfboard layout with connector designations;
- Full pin-to-pin wiring diagram for both RJ45 ends and T568B compliance;
- Enclosure CAD models with strain relief and ballistic shielding;
- Defined maximum cable length and acceptance criteria;
- Reference assembly photographs;
- Complete Bill of Materials with manufacturer part numbers.
