const wsProtocol = location.protocol === "https:" ? "wss://" : "ws://";
const ws = new WebSocket(wsProtocol + location.host + "/ws");

let suppressEvents = false;

ws.onopen = () => {
    console.log("WebSocket connected to server");
};

ws.onmessage = (event) => {
    const msg = JSON.parse(event.data);
    console.log("Received WebSocket message:", msg);

    // ✅ Only suppress self-sent dashboard messages
    if (msg.source && msg.source === "dashboard") return; // ← Prevent reacting to self

    suppressEvents = true;

    if (msg.type === "sensor_update") {
        for (const [key, value] of Object.entries(msg.data)) {
            const el = document.getElementById("sensor-" + key);
            if (el) {
                el.textContent = value !== null ? value : "--";
            }
        }
    }

    if (msg.type === "actuator_update") {
        for (const [key, value] of Object.entries(msg.data)) {
            const checkbox = document.getElementById(key);
            if (checkbox) {
                checkbox.checked = value;
            }
        }
    }

    if (msg.type === "flag_update") {
        for (const [key, value] of Object.entries(msg.data)) {
            const input = document.getElementById("flag-" + key);
            if (input) {
                if (input.type === "checkbox") {
                    input.checked = value;
                } else {
                    input.value = value;
                }
            }
        }
    }

    suppressEvents = false;
};

ws.onerror = (error) => {
    console.error("WebSocket error:", error);
};

// ✅ Send actuator changes via WebSocket with source tag
function toggleActuator(name) {
    if (suppressEvents) return;
    const checkbox = document.getElementById(name);
    const state = checkbox.checked;

    const msg = {
        type: "actuator_status",
        source: "dashboard",
        data: { [name]: state }
    };
    ws.send(JSON.stringify(msg));
}

// ✅ Send flag changes via WebSocket with source tag
function updateFlag(name) {
    if (suppressEvents) return;
    const input = document.getElementById("flag-" + name);
    let value;

    if (input.type === "checkbox") {
        value = input.checked;
    } else {
        value = parseInt(input.value, 10);
    }

    const msg = {
        type: "flag_status",
        source: "dashboard",
        data: { [name]: value }
    };
    ws.send(JSON.stringify(msg));
}
