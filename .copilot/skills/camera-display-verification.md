# Viewing Device Display via Camera

## Setup
The SenseCAP Watcher device has a webcam ("HD Webcam eMeet C960") pointed at it for remote display verification.

**Note**: The device is rotated 90° counterclockwise for stability. When analyzing captured images, rotate mentally:
- Left side of image = TOP of device display
- Right side of image = BOTTOM of device display

## Capture a Screenshot
```bash
ffmpeg -f dshow -i "video=HD Webcam eMeet C960" -frames:v 1 -update 1 -y screen_capture.jpg
```

## View the Screenshot
Use the `view` tool to display the captured image:
```
view screen_capture.jpg
```

## Tips
- The round display (412x412) shows content within a circular mask
- Blue LED (WS2812) is visible at the bottom-left of the device
- The camera is relatively far away — text smaller than 14pt may be hard to read
- Capture after meaningful UI changes to verify layout
- Clean up `.jpg` captures when done

## Workflow
1. Make UI changes
2. Build and flash: `idf.py build && idf.py flash`
3. Wait for boot: `Start-Sleep -Seconds 15`
4. Capture: `ffmpeg -f dshow -i "video=HD Webcam eMeet C960" -frames:v 1 -update 1 -y screen_capture.jpg`
5. View and verify
