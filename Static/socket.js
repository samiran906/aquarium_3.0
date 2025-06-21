// /static/js/socket.js

const wsProtocol = location.protocol === "https:" ? "wss://" : "ws://";
const ws = new WebSocket(wsProtocol + location.host + "/ws");

ws.onopen = () => {
    console.log("WebSocket connected to server");
};

ws.onmessage = (event) => {
    const msg = JSON.parse(event.data);
    console.log("Received WebSocket message:", msg);

    if (msg.type === "sensor_update") {
        for (const [key, value] of Object.entries(msg.data)) {
            const el = document.getElementById("sensor-" + key);
            if (el) {
                el.textContent = value !== null ? value : "--";
            }

            if (key === "temperature") {
                if (window.updateGauge) {
                    window.updateGauge(value);
                }
                console.log("Updating temperature gauge with value:", value);
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
                input.value = value;
            }
        }
    }
};

ws.onerror = (error) => {
    console.error("WebSocket error:", error);
};

// ✅ Send actuator state changes to Flask
const API_BASE = window.location.origin;

function toggleActuator(name) {
    const checkbox = document.getElementById(name);
    const state = checkbox.checked;

    fetch(`${API_BASE}/api/actuators/${name}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ state: state })
    })
    .then(response => response.json())
    .then(data => console.log("Actuator update:", data))
    .catch(err => console.error("Fetch error:", err));
}

// ✅ Send updated flag value to Flask
function updateFlag(name) {
    const input = document.getElementById("flag-" + name);
    const value = input.value;

    fetch(`${API_BASE}/api/flags`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ [name]: value })
    })
    .then(response => response.json())
    .then(data => console.log("Flag update:", data))
    .catch(err => console.error("Flag update error:", err));
}

// ✅ Momentarily trigger the reset_flag to true and auto-uncheck the UI
function triggerResetFlag() {
    fetch(`${API_BASE}/api/flags`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ reset_flag: true })
    })
    .then(response => response.json())
    .then(data => {
        console.log("Reset flag triggered:", data);
        setTimeout(() => {
            const checkbox = document.getElementById("reset_flag");
            if (checkbox) checkbox.checked = false;
        }, 1000); // Reset visually after 1 second
    })
    .catch(err => console.error("Reset flag error:", err));
}
