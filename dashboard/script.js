// --- MQTT Configuration ---
// The EMQX public broker supports WebSockets on port 8084 for secure (wss) connections
const MQTT_BROKER = "wss://broker.emqx.io:8084/mqtt";
const MQTT_TOPIC = "warehouse/zone1/telemetry";

// --- Thresholds (Matching ESP32 Firmware) ---
const THRESHOLDS = {
    temp: 30.0,
    gas: 300,
    stock: 20
};

// --- DOM Elements ---
const elConnIndicator = document.querySelector('.indicator');
const elConnText = document.getElementById('conn-text');
const elAlertBanner = document.getElementById('alert-banner');
const elAlertText = document.getElementById('alert-text');

const elTempCard = document.getElementById('card-temp');
const elTempVal = document.getElementById('val-temp');
const elTempBar = document.getElementById('bar-temp');
const elTempStatus = document.getElementById('status-temp');

const elGasCard = document.getElementById('card-gas');
const elGasVal = document.getElementById('val-gas');
const elGasBar = document.getElementById('bar-gas');
const elGasStatus = document.getElementById('status-gas');

const elStockCard = document.getElementById('card-stock');
const elStockVal = document.getElementById('val-stock');
const elStockBar = document.getElementById('bar-stock');
const elStockStatus = document.getElementById('status-stock');

const elMotionCard = document.getElementById('card-motion');
const elMotionVal = document.getElementById('val-motion');
const elMotionStatus = document.getElementById('status-motion');
const elRadarContainer = document.querySelector('.radar-container');

// --- Initialization ---
console.log(`Connecting to ${MQTT_BROKER}...`);
const client = mqtt.connect(MQTT_BROKER);

client.on('connect', () => {
    console.log('Connected to MQTT Broker via WebSockets!');
    elConnIndicator.classList.remove('disconnected');
    elConnIndicator.classList.add('connected');
    elConnText.textContent = "Connected to Live Telemetry";
    
    client.subscribe(MQTT_TOPIC, (err) => {
        if (!err) {
            console.log(`Subscribed to ${MQTT_TOPIC}`);
        } else {
            console.error('Subscription error:', err);
        }
    });
});

client.on('error', (err) => {
    console.error('Connection error:', err);
    elConnIndicator.classList.remove('connected');
    elConnIndicator.classList.add('disconnected');
    elConnText.textContent = "Connection Error";
});

client.on('offline', () => {
    elConnIndicator.classList.remove('connected');
    elConnIndicator.classList.add('disconnected');
    elConnText.textContent = "Broker Offline";
});

// --- Message Handling ---
client.on('message', (topic, message) => {
    try {
        const payload = JSON.parse(message.toString());
        updateDashboard(payload);
    } catch (e) {
        console.error("Failed to parse telemetry JSON", e);
    }
});

// --- UI Update Logic ---
function updateDashboard(data) {
    // Parse values
    const temp = parseFloat(data.temp_c);
    const gas = parseInt(data.gas_raw);
    const motion = parseInt(data.motion);
    const stock = parseInt(data.stock_pct);
    const globalAlert = parseInt(data.alert_status) === 1;

    // 1. Update Global Alert Banner
    if (globalAlert) {
        elAlertBanner.classList.add('active');
        elAlertText.textContent = "EMERGENCY: HARDWARE ALARMS ACTIVE";
    } else {
        elAlertBanner.classList.remove('active');
        elAlertText.textContent = "SYSTEM SECURE - ALL SENSORS NOMINAL";
    }

    // 2. Update Temperature
    elTempVal.textContent = temp.toFixed(1);
    const tempPct = Math.min((temp / 50.0) * 100, 100); // Scale up to 50C for bar
    elTempBar.style.width = `${tempPct}%`;
    
    if (temp > THRESHOLDS.temp) {
        elTempBar.style.background = 'var(--status-alert)';
        elTempStatus.textContent = "CRITICAL: OVERHEATING";
        elTempStatus.className = "status-label alert";
        elTempCard.classList.add('alert-state');
    } else {
        elTempBar.style.background = 'var(--status-warn)';
        elTempStatus.textContent = "Normal Temperature";
        elTempStatus.className = "status-label ok";
        elTempCard.classList.remove('alert-state');
    }

    // 3. Update Gas
    elGasVal.textContent = gas;
    const gasPct = Math.min((gas / 1023.0) * 100, 100);
    elGasBar.style.width = `${gasPct}%`;

    if (gas > THRESHOLDS.gas) {
        elGasBar.style.background = 'var(--status-alert)';
        elGasStatus.textContent = "CRITICAL: GAS LEAK DETECTED";
        elGasStatus.className = "status-label alert";
        elGasCard.classList.add('alert-state');
    } else {
        elGasBar.style.background = '#8b5cf6'; // Purple
        elGasStatus.textContent = "Air Quality Nominal";
        elGasStatus.className = "status-label ok";
        elGasCard.classList.remove('alert-state');
    }

    // 4. Update Stock
    elStockVal.textContent = stock;
    elStockBar.style.width = `${stock}%`;

    if (stock < THRESHOLDS.stock) {
        elStockBar.style.background = 'var(--status-alert)';
        elStockStatus.textContent = "WARNING: INVENTORY LOW";
        elStockStatus.className = "status-label alert";
        elStockCard.classList.add('alert-state');
    } else {
        elStockBar.style.background = 'var(--accent-blue)';
        elStockStatus.textContent = "Stock Level Sufficient";
        elStockStatus.className = "status-label ok";
        elStockCard.classList.remove('alert-state');
    }

    // 5. Update Motion
    if (motion === 1) {
        elMotionVal.textContent = "INTRUDER!";
        elMotionVal.className = "value-text alert";
        elMotionStatus.textContent = "Motion Detected in Sector A";
        elMotionStatus.className = "status-label alert";
        elRadarContainer.classList.add('intruder-detected');
        elMotionCard.classList.add('alert-state');
    } else {
        elMotionVal.textContent = "SECURE";
        elMotionVal.className = "value-text";
        elMotionStatus.textContent = "No Movement";
        elMotionStatus.className = "status-label ok";
        elRadarContainer.classList.remove('intruder-detected');
        elMotionCard.classList.remove('alert-state');
    }
}
