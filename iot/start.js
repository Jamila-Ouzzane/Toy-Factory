// Lance Node-RED (le broker MQTT est le service Mosquitto, cf. mosquitto.conf)
const { spawn } = require('child_process');
spawn('npx', ['node-red', '-u', 'nrdata', '-p', '1880'], { stdio: 'inherit', shell: true });
console.log('\nDashboard : http://localhost:1880/ui   |   Editeur : http://localhost:1880\n');
