# HuginnESP for iPhone — Support

HuginnESP is the iPhone companion app for the Huginn, an open-hardware ESP32 Wi-Fi and Bluetooth survey device. The Huginn does the radio scanning; the app connects to it over Bluetooth LE, shows what it finds, records it with your position and lets you export the result.

## Getting started

1. Flash the Huginn firmware from your browser with the [web flasher](https://pierregode.github.io/HuginnESP/).
2. Power on the Huginn and open the app. On the Live tab, tap **Connect your Huginn**.
3. Open the **Map** tab and tap **Start** to record a drive. Allow location when asked; it is used only while a drive is recording.
4. Tap **Stop** when you're done. The drive appears in the **Drives** tab, where you can share it as a CSV file, upload it, or delete it.

## No Huginn yet?

On the Live tab, under **No Huginn yet?**, tap **Try the demo Huginn**. It plays a made-up drive with the same data a real Huginn sends, so you can try every screen.

## Common questions

**The app can't find my Huginn.** Make sure Bluetooth is on and that HuginnESP is allowed to use it (Settings → HuginnESP → Bluetooth). Keep the Huginn close to the phone and check it is running the latest firmware from the web flasher. The Huginn doesn't need pairing in iOS Settings.

**How do I upload to WiGLE, WDGWars or Wardrift?** Enter your existing account under Settings → Upload accounts, then upload a drive from the Drives tab. Uploading is optional; the app works fully without it.

**Where is my data stored?** Only on your phone. See the [privacy policy](PRIVACY.md).

## Contact

- Email: pierre@gode.one
- Bug reports and feature requests: [GitHub issues](https://github.com/PierreGode/HuginnESP/issues)
