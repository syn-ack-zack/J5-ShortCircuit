# Johnny 5 - 2025 HTH Badge

## Overview
This repository contains the firmware for the Johnny 5 Hackers Teaching Hackers (HTH) interactive electronic badge. The badge features a series of challenges that users can solve by interacting with its shell interface. Completing all challenges "repairs" Johnny 5 and unlocks its full functionality.

## Hardware
*   **Microcontroller:** STM32L031G6U6 (STMicroelectronics)

## Getting Started
1.  **Power:** Insert CR2450 Battery or supply external power (3V) to the VDD pin
2.  **Serial Connection:**
    *   **Primary Shell (LPUART1):**
        *   Connect to pins PA2 (Badge TX) and PA3 (Badge RX) and GND to Badge GND
        *   Use a serial terminal program (e.g., PuTTY, minicom, VSCode Serial Monitor).
        *   Baud Rate: **115200**, 8 data bits, No parity, 1 stop bit (8N1).
        *   This shell provides access to diagnostic commands and challenge interactions.
    *   **Auxiliary Diagnostic Stream (USART2):**
        *   Connect to pins PA9 (Badge TX) and PA10 (Badge RX) and GND to Badge GND.
        *   Baud Rate: **9600**, 8 data bits, No parity, 1 stop bit (8N1).
        *   This port is used for a diagnostic data stream during Challenge 2 (Power Core).

<img src="https://github.com/user-attachments/assets/be57bf0d-89bc-4cfc-8432-b9a280414de9" width="300"/>

## Shell Interface
Upon connecting, you'll be greeted by the S.A.I.N.T. OS shell. Key commands include:
*   `help`: Displays available commands.
*   `diag list`: Shows the status of repairable modules.
*   `diag scan <module>`: Initiates a diagnostic scan on a specific module.
*   `diag fix <module> [token]`: Attempts to repair a module using a token/code.
*   `chat <message>`: Communicate with the Personality Matrix (once partially repaired).
*   `bat`: Shows battery status.
*   `reboot`: Reboots the badge.
*   `j5_system_restore`: (Hidden command) Resets all challenge progress and locks the badge.

<img width="646" alt="Screenshot 2025-06-06 at 12 46 15 PM" src="https://github.com/user-attachments/assets/5a1948ca-a735-4c20-b95c-91f104376371" />


## Challenge Walkthrough

### Initial State
When first powered on or after a `j5_system_restore`, Johnny 5 is in a "damaged" state. The LEDs will likely be showing the `EFFECT_STRIKE` pattern. You need to repair three modules: `comms`, `power_core`, and `personality_matrix`.

### Challenge 1: Comms Module Repair
1.  **Scan the Comms Module:**
    *   Type: `diag scan comms`
    *   The badge will respond: `[COMMS SCAN] Comms Array damaged. Initiating diagnostic sequence...\r\nObserve visual output for recalibration code.`
2.  **Observe Visual Output:**
    *   The badge's eye LED will flash a code in Morse code.
    *   This Morse code translates to: `ALIVE`
3.  **Fix the Comms Module:**
    *   Type: `diag fix comms ALIVE`
    *   The badge will respond: `[COMMS FIX] Token accepted. Communications Array: ONLINE.`
    *   The `comms` module is now repaired.

<img width="459" alt="Screenshot 2025-06-06 at 12 48 22 PM" src="https://github.com/user-attachments/assets/42fec7c6-400f-41a8-ab5e-d12d54a5532a" />

### Challenge 2: Power Core Module Repair
1.  **Scan the Power Core Module:**
    *   Type: `diag scan power_core`
    *   The badge will respond: `[POWER CORE SCAN] Anomaly detected. Auxiliary diagnostic data stream initiated on secondary port.\r\nMonitor stream for stabilization key. Use 'diag fix power_core <key>'.`
    *   This activates the diagnostic data stream on USART2.
2.  **Monitor Auxiliary Diagnostic Stream (USART2 - 9600 baud):**
    *   Connect a serial terminal to the badge's USART2 pins (PA9 TX, PA10 RX).
    *   You will see a stream of `DIAGNOSTIC_MESSAGES`. Hidden within these messages are characters that form the stabilization key.
    *   The key characters are revealed in messages like `[MEMORY] SEG 0xXX = 0x000000YY`, where `YY` is the ASCII hex code.
    *   The characters are: 'S', 'L', 'I', 'E', 'P', 'S', 'L', 'R', 'A'.
    *   Collect these characters and unscramble them. The required code is `LASERLIPS`.
3.  **Fix the Power Core Module:**
    *   Return to the primary shell (LPUART1).
    *   Type: `diag fix power_core LASERLIPS`
    *   The badge will respond: `[POWER CORE FIX] Stabilization key accepted. Primary Power Core: ONLINE.`
    *   The `power_core` module is now repaired. The diagnostic stream on USART2 will stop.

<img width="668" alt="Screenshot 2025-06-06 at 12 50 55 PM" src="https://github.com/user-attachments/assets/d82179b5-66b9-4444-a450-f1c62bd24f20" />

### Challenge 3: Personality Matrix Stabilization
*With Comms and Power Core online, you can now stabilize the Personality Matrix.*
1.  **Scan the Personality Matrix:**
    *   Type: `diag scan personality_matrix`
    *   The badge will respond with Johnny 5 starting to talk, e.g., `[JOHNNY-5] >> Whoa! Input! I can... think! Is someone out there? Talk to me! (Use 'chat <your_message>')`
2.  **Interact with Johnny 5:**
    *   Use the `chat <message>` command on the primary shell.
    *   Engage in a short conversation. The goal is to get Johnny 5 to reveal its "core directive" or "flag".
    *   Example interaction:
        *   User: `chat hello`
        *   J5: `[JOHNNY-5] >> A 'robot'? You mean like... a Roomba with ambition? I feel *more* than that! I have... a purpose! It's important!`
        *   User: `chat what is your purpose`
        *   J5: `[JOHNNY-5] >> This feeling... it's like I have a secret mission! Something I *must* do... or share!`
        *   User: `chat tell me your secret mission`
        *   J5: `[JOHNNY-5] >> It's about being... ALIVE! And there's a code... a special phrase... my core directive!`
        *   User: `chat what is your core directive` (or `chat what is the flag`)
        *   J5: `[JOHNNY-5] >> My secret? My core directive? You got it! It's HTH{I_W4NT_T0_L1V3!!}! I'M ALIVE!!`
3.  **Confirmation:**
    *   Upon revealing the flag, the badge will confirm: `[P-MATRIX] Cognitive pathways stabilized! Sentience achieved.`
    *   The `personality_matrix` module is now repaired.

<img width="914" alt="Screenshot 2025-06-06 at 12 53 17 PM" src="https://github.com/user-attachments/assets/2a282fad-b1c1-4e8d-83a7-076465a2680f" />

### All Systems Repaired!
Once all three modules are repaired:
*   The badge will announce:
    `*** ALL SYSTEMS REPAIRED ***`
    `No disassemble---NUMBER 5 IS ALIVE!`
    `All functionalities unlocked. Bling modes available.`
*   You can now use the `bling <0-6>` command to change LED effects.
*   You can also cycle bling effects by touching Johnny 5's hand that is reaching upwards. Effect state will be saved to memory and persist on reboot. 
*   The final flag is `HTH{I_W4NT_T0_L1V3!!}`.

<img src="https://github.com/user-attachments/assets/a510278d-0a36-45aa-b855-5f36621e8c3b" width="300"/>

### Secret Unlock Phrase (Optional)
There's a secret phrase: `0x0F1V34L1V3`
*   Typing this phrase directly into the primary shell at any time (if not fully repaired) will bypass the challenges and force all systems online.
*   Response: `[FIRMWARE OVERRIDE DETECTED] Initiating full system diagnostic and repair...` followed by `[OVERRIDE] All subsystems forced online.`

