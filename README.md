# Chief Cooker (Brute Force Edition)

> [!NOTE]
> **This repository is a fork of the original [FZ-ChiefCooker](https://github.com/denr01/FZ-ChiefCooker) by [denr01](https://github.com/denr01).**
> For the original project, base features, full documentation, and usage instructions, please refer to the [upstream repository](https://github.com/denr01/FZ-ChiefCooker).

---

## Added Features in this Fork: Brute Force Mode

This fork introduces a dedicated **Brute Force Station ID** engine, enabling you to trigger restaurant pagers without needing to capture a transmission from the base station first.

### Configurable Settings:
- **Speed Profiles**:
  - **Max (Fastest)**: 2 packet repeats with 0 ms delay between iterations for rapid sweeps.
  - **Normal**: 5 packet repeats with ~80 ms delay.
  - **Slow (Reliable)**: 10 packet repeats with ~250 ms delay for maximum reception reliability.
- **Encoding Selection**:
  - **Single Protocol**: Target specific encoders (`TD157`, `TD165 / T119`, `TD174`, `L8R / T111`, `L8S / ZJ-68`).
  - **`ALL (Try All)` Mode**: Systematically cycles through all supported protocols, automatically adjusting station bounds per protocol.
- **Station & Pager Ranges**:
  - **Station Min / Max**: Selectable starting and ending station ranges with Left/Right step selection (dynamically bounded by each encoder's maximum bit capacity).
  - **Pager Min / Max**: Configurable pager number bounds (1–99).
  - **Nested Sweeping**: Tests all pagers within the range for Station $N$ before incrementing to Station $N+1$.

---

### Interactive Controls (SWEEP Screen):

- **While Sweeping**:
  - `[OK]`: **Pause** the automated sweep.
  - `[<]` / `[>]`: **Pause & Step**: Immediately pauses and steps 1 candidate backward or forward.
  - `[Back]`: Safely stop transmission, put the radio to idle, and return to menu.

- **While Paused**:
  - `[OK]`: **Resume** automated sweeping.
  - `[<]` / `[>]`: **Step Back / Forward**: Manually step by 1 candidate and transmit 1 test packet.
  - **Hold `[OK]` or Hold `[>]`**: **Continuous Transmit**: Repeatedly transmit the current candidate signal while the button is held down.

---

### 🔨 Building
Always build using the provided post-processing script to avoid memory issues on the Flipper:
```bash
python scripts/build-and-clear.py
```