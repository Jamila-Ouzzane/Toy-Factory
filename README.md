# 🧸 ToyFactory 4.0

Simulation industrielle d'une ligne de production de jouets, 100 % SVG, connectée en IoT (MQTT + Node-RED).

Source → Convoyeur → Emballage → Contrôle qualité → Stock / Rejet

## Lancer
0. Une fois (console admin) : `copy iot\mosquitto.conf "C:\Program Files\mosquitto\mosquitto.conf"` puis `net stop mosquitto && net start mosquitto`
1. `cd iot` puis `node start.js` (Node-RED ; broker = service Mosquitto)
2. Ouvrir `index.html` et cliquer sur Start (le voyant **IoT ●** passe au vert)
3. Dashboard tablette : http://localhost:1880/ui — éditeur Node-RED : http://localhost:1880

## Fonctions
- Jouets aléatoires (8 modèles, couleurs uniques), emballage cadeau, contrôle qualité
- Paramètres en temps réel, goulot d'étranglement, mode défi (100 conformes en 10 min)
- Capteurs, alertes de défaut > 30 %, maintenance prédictive (usure, température, pannes)
- Vue 3D inclinée, thème clair/sombre, FR/EN, sons
- IoT : télémétrie `usine/telemetry`, alertes `usine/alert`, commandes `usine/cmd`
