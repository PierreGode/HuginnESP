/* SkimGuard C5 Web Flasher (esptool-js v0.6.0) */

let _esptool = null;

async function getEsptool() {
  if (_esptool) return _esptool;
  _esptool = await import("https://unpkg.com/esptool-js@0.6.0/bundle.js");
  return _esptool;
}

const $ = (id) => document.getElementById(id);

function showOverlay() {
  $("flash-overlay").hidden = false;
  $("flash-close").hidden = true;
  $("flash-confirm-row").hidden = true;
  $("flash-log").textContent = "";
  $("flash-bar").style.width = "0%";
  $("flash-pct").textContent = "";
  $("flash-status").textContent = "Preparing installer...";
}

function hideOverlay() {
  $("flash-overlay").hidden = true;
}

function setStatus(msg) {
  $("flash-status").textContent = msg;
}

function setProgress(pct) {
  $("flash-bar").style.width = pct + "%";
  $("flash-pct").textContent = pct + "%";
}

function log(msg) {
  const el = $("flash-log");
  el.textContent += msg + "\n";
  el.scrollTop = el.scrollHeight;
}

function waitForConfirm() {
  return new Promise((resolve) => {
    const row = $("flash-confirm-row");
    const yes = $("flash-confirm-yes");
    const no = $("flash-confirm-no");
    row.hidden = false;

    function cleanup(result) {
      row.hidden = true;
      yes.removeEventListener("click", onYes);
      no.removeEventListener("click", onNo);
      resolve(result);
    }

    function onYes() { cleanup(true); }
    function onNo() { cleanup(false); }

    yes.addEventListener("click", onYes);
    no.addEventListener("click", onNo);
  });
}

window.flashDevice = async function (manifestPath) {
  if (!("serial" in navigator)) {
    alert("Web Serial requires Chrome, Edge, or Opera on desktop.");
    return;
  }

  showOverlay();

  try {
    setStatus("Select serial port...");
    let port;
    try {
      port = await navigator.serial.requestPort();
    } catch (_e) {
      setStatus("No serial port selected.");
      $("flash-close").hidden = false;
      return;
    }

    setStatus("Loading flasher...");
    const { ESPLoader, Transport } = await getEsptool();

    setStatus("Loading manifest...");
    const mResp = await fetch(manifestPath);
    if (!mResp.ok) throw new Error("Manifest fetch failed (" + mResp.status + ")");
    const manifest = await mResp.json();
    const build = manifest.builds[0];
    const part = build.parts[0];

    log("Profile: " + manifest.name);
    log("Target chip: " + build.chipFamily);

    setStatus("Downloading firmware...");
    const base = manifestPath.substring(0, manifestPath.lastIndexOf("/") + 1);
    const fwUrl = part.path.startsWith("http") ? part.path : base + part.path;
    const fwResp = await fetch(fwUrl);
    if (!fwResp.ok) throw new Error("Firmware download failed (" + fwResp.status + ")");
    const fwData = new Uint8Array(await fwResp.arrayBuffer());
    log("Image size: " + (fwData.length / 1024).toFixed(0) + " KB");

    setStatus("Connecting to device...");
    const transport = new Transport(port, true);
    const terminal = {
      clean() {},
      writeLine(data) { log(data); },
      write(_data) {}
    };

    const loader = new ESPLoader({
      transport,
      baudrate: 115200,
      terminal
    });

    const chip = await loader.main();
    log("Connected: " + chip);

    const normalize = (s) => s.replace(/[-_ ]/g, "").toUpperCase();
    if (!normalize(chip).startsWith(normalize(build.chipFamily))) {
      throw new Error("Wrong chip: expected " + build.chipFamily + " but found " + chip);
    }

    setStatus("Ready to flash. Confirm to continue.");
    log("Ready: " + manifest.name);

    const confirmed = await waitForConfirm();
    if (!confirmed) {
      setStatus("Flashing canceled.");
      await transport.disconnect();
      $("flash-close").hidden = false;
      return;
    }

    setStatus("Writing flash...");
    await loader.writeFlash({
      fileArray: [{ data: fwData, address: part.offset }],
      flashSize: "keep",
      eraseAll: false,
      compress: true,
      reportProgress(_fileIndex, written, total) {
        const pct = Math.round((written / total) * 100);
        setProgress(pct);
        setStatus("Flashing... " + pct + "%");
      }
    });

    setStatus("Resetting device...");
    await loader.after("hard_reset");
    await transport.disconnect();

    setProgress(100);
    setStatus("Flash complete. Device rebooting.");
    log("Done. SkimGuard C5 installed.");
  } catch (err) {
    console.error(err);
    setStatus("Error: " + err.message);
    log("ERROR: " + err.message);
  }

  $("flash-close").hidden = false;
};

document.addEventListener("DOMContentLoaded", () => {
  const closeBtn = $("flash-close");
  if (closeBtn) closeBtn.addEventListener("click", hideOverlay);
});
