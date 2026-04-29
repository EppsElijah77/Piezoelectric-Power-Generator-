
# Piezoelectric Power Generator — Foot Power 

A energy harvesting system that converts the mechanical pressure of footsteps into usable electrical energy using piezoelectric transducers.

---

## Overview

This project explores harvesting kinetic energy from everyday walking. Piezoelectric materials generate a small electric charge when mechanically stressed — by embedding them in a floor tile, each footstep becomes a tiny power source. The generated energy can be stored in a capacitor or small battery and used to power low-energy electronics like sensors, LEDs, or Bluetooth beacons.

---

## How It Works

1. **Pressure Applied** — User steps on the piezoelectric element (insole, tile, or pad)
2. **Charge Generated** — The piezo material deforms and produces an AC voltage spike
3. **Rectification** — A bridge rectifier converts AC to DC
4. **Storage** — A capacitor or supercapacitor smooths and stores the charge
5. **Output** — Regulated DC power is delivered to the load (sensor, LED, microcontroller, etc.)

---

## Components

| Component | Description |
|---|---|
| Piezoelectric Disc(s) | Primary energy transducer (e.g., 27mm or 35mm ceramic discs) |
| Bridge Rectifier | Converts AC output to DC (e.g., 1N4148 diodes) |
| Capacitor / Supercapacitor | Energy storage buffer |
| Voltage Regulator | Stabilizes output voltage (e.g., LDO or boost converter) |
| Load | Target device (LED, sensor, microcontroller) |
| Enclosure / Insole | Physical housing for the piezo stack |

---

## Circuit Diagram

```
[Piezo Disc] --> [Bridge Rectifier] --> [Capacitor] --> [Voltage Regulator] --> [Load]
```

A more detailed schematic can be found in `/schematics/`.

---

## Energy Output

| Scenario | Estimated Output |
|---|---|
| Single disc, one step | ~1–5 mW (brief pulse) |
| Stack of 10 discs, walking | ~10–50 mW average |
| Optimized multi-layer insole | ~100–200 mW under continuous walking |

> ⚠️ Piezoelectric generators produce low-average power. They are best suited for ultra-low-power applications or as supplemental charging sources.

---

## Getting Started

### 1. Assemble the Piezo Stack
- Stack multiple piezoelectric discs in parallel (increases current) or series (increases voltage)
- Connect leads carefully — piezo elements are fragile

### 2. Build the Rectifier Circuit
- Wire four diodes in a full-bridge configuration
- Connect AC input from piezo, DC output to capacitor

### 3. Add Storage and Regulation
- Connect a supercapacitor across the DC output for energy buffering
- Add a boost converter or LDO regulator to provide stable output voltage

### 4. Integrate into Insole or Enclosure
- Embed the piezo stack into a foam insole or 3D-printed housing
- Ensure the element sits under the heel or ball of the foot for maximum pressure

### 5. Test
- Walk on the insole and measure output voltage with a multimeter
- Observe capacitor charge buildup over multiple steps

---

## Project Structure

```
/
├── schematics/          # Circuit diagrams and KiCad files
├── hardware/            # Insole design, 3D print files (.stl)
├── firmware/            # Code for any connected microcontroller
├── data/                # Measurement logs and test results
├── docs/                # Additional documentation and references
└── README.md
```

---

## Applications

- Self-powered wireless sensors
- Step counters / pedometers
- Emergency LED lighting
- Wearable health monitors
- Smart floor tiles (public spaces, gyms)
- IoT edge devices in low-power environments

---

## Limitations

- Output is intermittent and depends on walking activity
- Peak voltage can be high (~50–100V open circuit) but current is very low
- Efficiency losses in rectification and regulation stages
- Piezo elements can crack under excessive or uneven force


## License

MIT License — feel free to use, modify, and share. See `LICENSE` for details.

---

## Contributing

Pull requests welcome, If you've tested different piezo configurations, insole materials, or storage circuits, open an issue or PR with your findings.
