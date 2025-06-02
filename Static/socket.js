// /static/js/socket.js

const ws = new WebSocket("ws://" + location.host + "/ws");

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
};

ws.onerror = (error) => {
    console.error("WebSocket error:", error);
};

// ✅ Added function to send actuator state changes to Flask via POST
function toggleActuator(name) {
    const checkbox = document.getElementById(name);
    const state = checkbox.checked;

    fetch(`/api/actuators/${name}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ state: state })
    })
    .then(response => response.json())
    .then(data => {
        console.log(`Actuator ${name} updated:`, data);
    })
    .catch(error => {
        console.error('Error updating actuator:', error);
    });
}
