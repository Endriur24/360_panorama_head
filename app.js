class PanoHeadController {
    constructor() {
        this.device = null;
        this.characteristic = null;
        this.commandQueue = [];
        this.isSending = false;
        this.isPaused = false;
        this.isRunning = false;
        this.currentPhoto = 0;
        this.totalPhotos = 0;
        this.horizontalPhotos = 6;
        this.verticalPhotos = 3;
        
        // Ensure buttons are disabled initially
        document.getElementById('pause-btn').disabled = true;
        document.getElementById('resume-btn').disabled = true;
    }

    updateMatrixPreview() {
        const matrix = document.getElementById('photo-matrix');
        matrix.style.gridTemplateColumns = `repeat(${this.horizontalPhotos}, 1fr)`;
        matrix.innerHTML = '';
        
        for (let row = 0; row < this.verticalPhotos; row++) {
            const isEvenRow = row % 2 === 0;
            for (let col = 0; col < this.horizontalPhotos; col++) {
                const cell = document.createElement('div');
                cell.className = 'matrix-cell';
                const cellIndex = row * this.horizontalPhotos + (isEvenRow ? col : (this.horizontalPhotos - 1 - col));
                
                // Add direction indicator
                const arrow = document.createElement('i');
                arrow.className = `ri-arrow-${isEvenRow ? 'right' : 'left'}-line`;
                cell.appendChild(arrow);
                
                if (cellIndex < this.currentPhoto) {
                    cell.classList.add('completed');
                }
                matrix.appendChild(cell);
            }
        }
    }

    requestCurrentSettings() {
        this.sendCommand('?');
    }

    parseSettings(message) {
        const lines = message.split('\n');
        const settingsMap = {
            'horizontal photos': 'horizontal-input',
            'vertical photos': 'vertical-input',
            'acceleration': 'acceleration-input',
            'speed': 'speed-input',
            'delay befote photo': 'delay-input',
            'max exposure time': 'exposure-input',
            'exposure amount': 'exp-amount-input'
        };

        lines.forEach(line => {
            for (const [text, inputId] of Object.entries(settingsMap)) {
                if (line.toLowerCase().includes(text)) {
                    const value = line.match(/\d+/);
                    if (value) {
                        const numValue = parseInt(value[0]);
                        document.getElementById(inputId).value = numValue;
                        
                        // Update controller properties
                        if (text === 'horizontal photos') this.horizontalPhotos = numValue;
                        if (text === 'vertical photos') this.verticalPhotos = numValue;
                    }
                    break;
                }
            }
        });
        
        this.updateMatrixPreview();
        const progressText = document.getElementById('progress-text');
        progressText.textContent = 'Settings loaded from device';
        setTimeout(() => {
            if (!this.isRunning) {
                progressText.textContent = 'Ready to start';
            }
        }, 2000);
    }

    updateProgress() {
        const progressFill = document.getElementById('progress-fill');
        const progressText = document.getElementById('progress-text');
        const photoCount = document.getElementById('photo-count');
        
        if (!this.isRunning) {
            progressText.textContent = 'Ready to start';
            photoCount.textContent = '';
            progressFill.style.width = '0%';
            return;
        }

        const progress = (this.currentPhoto / this.totalPhotos) * 100;
        progressFill.style.width = `${progress}%`;
        photoCount.textContent = `${this.currentPhoto} of ${this.totalPhotos}`;
        
        if (this.isPaused) {
            progressText.textContent = 'Paused';
        } else {
            progressText.textContent = 'In Progress...';
        }
    }

    async connect() {
        try {
            this.device = await navigator.bluetooth.requestDevice({
                filters: [{ name: 'PanoHead' }],
                optionalServices: ['0000ffe0-0000-1000-8000-00805f9b34fb']
            });

            const server = await this.device.gatt.connect();
            const service = await server.getPrimaryService('0000ffe0-0000-1000-8000-00805f9b34fb');
            this.characteristic = await service.getCharacteristic('0000ffe1-0000-1000-8000-00805f9b34fb');

            this.characteristic.startNotifications().then(_ => {
                this.characteristic.addEventListener('characteristicvaluechanged', 
                    this.handleNotifications.bind(this));
            });

            document.getElementById('status-bar').textContent = 'Status: Connected';
        } catch (error) {
            console.error('Bluetooth connection failed:', error);
            document.getElementById('status-bar').textContent = 'Status: Connection Failed';
        }
    }

    async sendCommand(command) {
        const fullCommand = new TextEncoder().encode(command + '\r');
        this.commandQueue.push(fullCommand);
        await this.processQueue();
    }

    async processQueue() {
        if (!this.isSending && this.commandQueue.length > 0) {
            this.isSending = true;
            const cmd = this.commandQueue.shift();
            await this.characteristic.writeValue(cmd);
            this.isSending = false;
            this.processQueue();
        }
    }

    handleNotifications(event) {
        const value = new TextDecoder().decode(event.target.value);
        document.getElementById('message-log').innerHTML += `${value}<br>`;
        
        if (value.includes('Start Process')) {
            this.isRunning = true;
            this.isPaused = false;
            this.currentPhoto = 0;
            this.totalPhotos = this.horizontalPhotos * this.verticalPhotos;
            this.updateProgress();
            this.updateMatrixPreview();
            document.getElementById('pause-btn').disabled = false;
            document.getElementById('resume-btn').disabled = true;
        } else if (value.includes(' of ')) {
            const [current] = value.match(/^\d+/);
            this.currentPhoto = parseInt(current);
            this.updateProgress();
            this.updateMatrixPreview();
            
            // Re-enable pause button if process is running and not paused
            if (this.isRunning && !this.isPaused) {
                document.getElementById('pause-btn').disabled = false;
                document.getElementById('resume-btn').disabled = true;
            }
        } else if (value.includes('PAUSED')) {
            this.isPaused = true;
            document.getElementById('pause-btn').disabled = true;
            document.getElementById('resume-btn').disabled = false;
            this.updateProgress();
        } else if (value.includes('RESUMED')) {
            this.isPaused = false;
            document.getElementById('pause-btn').disabled = false;
            document.getElementById('resume-btn').disabled = true;
            this.updateProgress();
        } else if (value.includes('motor testing')) {
            const progressText = document.getElementById('progress-text');
            progressText.textContent = 'Motor Testing...';
        } else if (value.includes('DONE')) {
            progressText.textContent = 'Motor Test Complete';
        } else if (value.includes('settings saved')) {
            this.requestCurrentSettings();
            const progressText = document.getElementById('progress-text');
            progressText.textContent = 'Settings Saved';
            setTimeout(() => {
                if (!this.isRunning) {
                    progressText.textContent = 'Ready to start';
                }
            }, 2000);
        }
    }

    resetProgress() {
        this.isRunning = false;
        this.isPaused = false;
        this.currentPhoto = 0;
        this.updateProgress();
        this.updateMatrixPreview();
        document.getElementById('pause-btn').disabled = true;
        document.getElementById('resume-btn').disabled = true;
    }
}

const controller = new PanoHeadController();

// Event Listeners
document.getElementById('connect-btn').addEventListener('click', () => controller.connect());

// Step size slider
const stepSlider = document.getElementById('tilt-step');
const stepValue = document.getElementById('step-value');

stepSlider.addEventListener('input', (e) => {
    stepValue.textContent = e.target.value;
});

document.getElementById('tilt-up').addEventListener('click', () => {
    const step = stepSlider.value;
    controller.sendCommand(`+${step}`);
});

document.getElementById('tilt-down').addEventListener('click', () => {
    const step = stepSlider.value;
    controller.sendCommand(`-${step}`);
});

// Request current settings after connect
document.getElementById('connect-btn').addEventListener('click', () => {
    controller.connect().then(() => {
        setTimeout(() => controller.requestCurrentSettings(), 1000);
    });
});
document.getElementById('set-position').addEventListener('click', () => controller.sendCommand('p'));

document.getElementById('start-btn').addEventListener('click', () => {
    if (confirm('Did you set the top position?')) {
        controller.sendCommand('s');
    }
});

document.getElementById('pause-btn').addEventListener('click', () => {
    controller.sendCommand('1');
});

document.getElementById('resume-btn').addEventListener('click', () => {
    controller.sendCommand('0');
});

document.getElementById('stop-btn').addEventListener('click', () => {
    controller.sendCommand('2');
    controller.resetProgress();
});

document.getElementById('horizontal-input').addEventListener('change', (e) => {
    controller.horizontalPhotos = parseInt(e.target.value);
    controller.updateMatrixPreview();
});

document.getElementById('vertical-input').addEventListener('change', (e) => {
    controller.verticalPhotos = parseInt(e.target.value);
    controller.updateMatrixPreview();
});

document.getElementById('save-settings').addEventListener('click', () => controller.sendCommand('$'));
document.getElementById('reset-settings').addEventListener('click', () => controller.sendCommand('#'));

document.getElementById('exp-amount-input').addEventListener('change', (e) => {
    controller.sendCommand(`e${e.target.value}`);
});

// Register Service Worker
// Mobile Tab Navigation
const tabButtons = document.querySelectorAll('.tab-btn');
const tabContents = document.querySelectorAll('.tab-content');

tabButtons.forEach(button => {
    button.addEventListener('click', () => {
        const tabId = button.dataset.tab;
        
        // Update active states
        tabButtons.forEach(btn => btn.classList.remove('active'));
        tabContents.forEach(content => content.classList.remove('active'));
        
        button.classList.add('active');
        document.querySelector(`.tab-content[data-tab="${tabId}"]`).classList.add('active');
    });
});

// Creates and displays a notification when a new version of the app is available
// Returns the notification element for potential future manipulation
function createUpdateNotification(registration) {
    const notification = document.createElement('div');
    notification.className = 'update-notification';
    notification.innerHTML = `
        <p>A new version is available!</p>
        <button id="update-app">Update</button>
    `;
    document.body.appendChild(notification);
    
    document.getElementById('update-app').addEventListener('click', () => {
        notification.remove();
        // Tell the service worker to take control
        registration.waiting.postMessage('skipWaiting');
    });
    return notification;
}

// Service Worker Registration and Update Management
if ('serviceWorker' in navigator) {
    // Prevents multiple refreshes when the new service worker takes control
    let refreshing = false;

    // Listens for when the new service worker takes control and refreshes the page
    // This ensures all pages are running the same version of the app
    navigator.serviceWorker.addEventListener('controllerchange', () => {
        if (!refreshing) {
            refreshing = true;
            window.location.reload();
        }
    });

    // Register the service worker and set up update handling
    navigator.serviceWorker.register('service-worker.js')
        .then(registration => {
            // Immediately check for updates when page loads
            registration.update();

            // Listen for new service worker installation
            // This event fires when a new version of the app is available
            registration.addEventListener('updatefound', () => {
                const newWorker = registration.installing;
                // Monitor the installation process of the new service worker
                newWorker.addEventListener('statechange', () => {
                // When installation is complete and there's an existing service worker
                // This means we have an update ready to be activated
                if (newWorker.state === 'installed' && navigator.serviceWorker.controller) {
                    createUpdateNotification(registration);
                    }
                });
            });

            // Check for updates more frequently while the app is running
            // This ensures users don't miss updates during active sessions
            setInterval(() => {
                registration.update();
            }, 60000); // Check every minute for testing, adjust as needed
        })
        .catch(err => console.error('Service Worker Registration Failed:', err));
}
