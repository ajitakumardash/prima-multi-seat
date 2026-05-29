# Prima Multi Seat — Hardware Setup Guide

## Physical Setup

### Step 1: Connect Hardware

```
PC
├── GPU Output 1 → Monitor 1 (DisplayPort/HDMI)
├── GPU Output 2 → Monitor 2 (DisplayPort/HDMI)
├── USB Port → Keyboard 1 (for Seat 1)
├── USB Port → Keyboard 2 (for Seat 2)
├── USB Port → Mouse 1    (for Seat 1)
├── USB Port → Mouse 2    (for Seat 2)
├── Audio Out 1 → Speakers/Headphones (Seat 1)
└── Audio Out 2 → HDMI Monitor Audio or USB Audio (Seat 2)
```

### Step 2: Windows Display Setup

1. Right-click Desktop → Display Settings
2. Set monitors to **Extend these displays** (NOT duplicate)
3. Note which monitor is "1" and which is "2"
4. Set Monitor 1 as Primary Display

### Step 3: Windows Audio Setup

1. Right-click speaker icon → Open Sound Settings
2. Verify both audio devices appear
3. Note the device names for configuration

### Step 4: Install Prima Multi Seat

Run `PrimaMultiSeatSetup.exe` as Administrator.

### Step 5: Device Assignment

1. Open PrimaUI.exe
2. Go to Devices tab
3. Plug/unplug each keyboard — note which hDevice appears
4. Assign Keyboard 1 to Seat 1, Keyboard 2 to Seat 2
5. Repeat for mice and audio devices
6. Click Save Configuration

### Step 6: Test

1. Move mouse on Monitor 1 — should show red cursor
2. Move mouse on Monitor 2 — should show blue cursor
3. Type on Keyboard 1 — input should go to Monitor 1 apps
4. Type on Keyboard 2 — input should go to Monitor 2 apps
5. Play audio — verify each seat gets its own audio

## School Lab Deployment

For multiple PCs, use the installer silently:
```
PrimaMultiSeatSetup.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
```

Push via Group Policy software deployment or SCCM.