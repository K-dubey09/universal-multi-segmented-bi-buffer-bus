let Module;
let busPtr;
let producerCount = 0;
let consumerCount = 0;
let feedbackLock = false;

import { renderRing } from "./ring.js";

UniversalMultiSegmentedBiBufferBusModule().then(mod => {
  Module = mod;
  busPtr = Module._umsbb_init(1024, 2048);
  setInterval(pollRingState, 1000);
  setInterval(pollFeedback, 1000);
});

// ✅ Expose globally
window.addProducer = function addProducer() {
  const id = producerCount++;
  const div = document.createElement("div");
  div.className = "producer";
  div.innerHTML = `
    <label>Producer ${id}</label><br>
    <input id="msg${id}" placeholder="Capsule payload" />
    <select id="lane${id}">
      ${Array.from({ length: consumerCount }, (_, i) => `<option value="${i}">Lane ${i}</option>`).join("")}
    </select>
    <label><input type="checkbox" id="corrupt${id}" /> Corrupt</label>
    <button onclick="sendCapsule(${id})">Send</button>
  `;
  document.getElementById("producers").appendChild(div);
};

window.addConsumer = function addConsumer() {
  const id = consumerCount++;
  const div = document.createElement("div");
  div.className = "consumer";
  div.innerHTML = `
    <label>Consumer ${id}</label>
    <button onclick="drainCapsule(${id})">Drain Lane ${id}</button>
  `;
  document.getElementById("consumers").appendChild(div);
};

window.updateBatch = function updateBatch() {
  const size = document.getElementById("batchSlider").value;
  document.getElementById("batchDisplay").textContent = size;
};

window.clearFeedback = function clearFeedback() {
  document.getElementById("feedbackLog").innerHTML = "";
};

window.sendCapsule = function sendCapsule(id) {
  const corrupt = document.getElementById(`corrupt${id}`).checked;
  const lane = parseInt(document.getElementById(`lane${id}`).value);
  let encoded, ptr;

  if (corrupt) {
    encoded = new Uint8Array([0xDE, 0xAD, 0xBE, 0xEF]);
    ptr = Module._malloc(encoded.length);
    Module.HEAPU8.set(encoded, ptr);
    Module._umsbb_submit_to(busPtr, lane, ptr, encoded.length);
    Module._free(ptr);
    logFeedback(`❌ Producer ${id} → Lane ${lane}: Corrupted capsule`);
  } else {
    const msg = document.getElementById(`msg${id}`).value;
    encoded = new TextEncoder().encode(msg);
    ptr = Module._malloc(encoded.length);
    Module.HEAPU8.set(encoded, ptr);
    Module._umsbb_submit_to(busPtr, lane, ptr, encoded.length);
    Module._free(ptr);
    logFeedback(`🧪 Producer ${id} → Lane ${lane}: "${msg}"`);
  }
};

window.drainCapsule = function drainCapsule(lane) {
  Module._umsbb_drain_from(busPtr, lane);
  logFeedback(`🔁 Drained Lane ${lane}`);
};

function logFeedback(msg) {
  const log = document.getElementById("feedbackLog");
  if (log.children.length > 100) log.removeChild(log.firstChild); // cap entries
  const entry = document.createElement("p");
  entry.textContent = `[${new Date().toLocaleTimeString()}] ${msg}`;
  log.appendChild(entry);
}

function pollRingState() {
  const ringState = Array.from({ length: consumerCount }, (_, i) => ({
    write: Math.floor(Math.random() * 10),
    read: Math.floor(Math.random() * 10),
    throttled: Math.random() > 0.8
  }));
  renderRing(ringState);
}

function pollFeedback() {
  if (feedbackLock) return;
  feedbackLock = true;

  try {
    const countPtr = Module._malloc(4);
    const fbPtr = Module._umsbb_get_feedback(busPtr, countPtr);
    const count = Module.HEAPU32[countPtr >> 2];

    for (let i = 0; i < count && i < 50; i++) {
      const base = fbPtr + i * 32;
      const seq = Module.HEAPU32[base >> 2];
      const type = Module.HEAPU32[(base + 4) >> 2];
      const notePtr = Module.HEAPU32[(base + 16) >> 2];
      const note = readCString(notePtr);
      logFeedback(`🧾 Capsule #${seq} → ${note} [${feedbackType(type)}]`);
    }

    Module._free(countPtr);
  } catch (e) {
    console.error("Feedback poll error:", e);
  }

  feedbackLock = false;
}

function readCString(ptr) {
  let str = "";
  while (Module.HEAPU8[ptr] !== 0) {
    str += String.fromCharCode(Module.HEAPU8[ptr++]);
  }
  return str;
}

function feedbackType(code) {
  return {
    0: "OK",
    1: "CORRUPTED",
    2: "THROTTLED",
    3: "SKIPPED",
    4: "IDLE",
    5: "GPU_EXECUTED",
    6: "CPU_EXECUTED"
  }[code] || "UNKNOWN";
}