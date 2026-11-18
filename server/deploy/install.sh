#!/usr/bin/env bash
set -euo pipefail
# Run after placing this repository at /opt/aether and reviewing .env.
test "$(id -u)" -eq 0 || { echo 'Run with sudo'; exit 1; }
test -f /opt/aether/server/.env || { echo 'Copy server/.env.example to server/.env and edit it first'; exit 1; }
apt-get update
apt-get install -y mosquitto mosquitto-clients python3-venv python3-gpiozero python3-lgpio sudo
id aether >/dev/null 2>&1 || useradd --system --create-home aether
usermod -a -G dialout,gpio aether
python3 -m venv --system-site-packages /opt/aether/server/.venv
/opt/aether/server/.venv/bin/pip install -r /opt/aether/server/requirements.txt
mkdir -p /opt/aether/server/data
chown -R aether:aether /opt/aether/server/data
chown root:aether /opt/aether/server/.env
chmod 0640 /opt/aether/server/.env
install -m 0644 /opt/aether/server/deploy/aether-api.service /etc/systemd/system/aether-api.service
install -m 0644 /opt/aether/server/deploy/aether-target.service /etc/systemd/system/aether-target.service
visudo -cf /opt/aether/server/deploy/aether-recovery.sudoers
install -m 0440 /opt/aether/server/deploy/aether-recovery.sudoers /etc/sudoers.d/aether-recovery
if ! test -f /etc/mosquitto/aether-passwords; then
  echo 'Create MQTT credentials: mosquitto_passwd -c /etc/mosquitto/aether-passwords aether'
  echo 'Then install deploy/aether-mosquitto.conf and restart mosquitto.'
else
  install -m 0644 /opt/aether/server/deploy/aether-mosquitto.conf /etc/mosquitto/conf.d/aether.conf
fi
systemctl daemon-reload
systemctl enable aether-api.service aether-target.service
echo 'Installation prepared. Verify UART mapping, broker credentials and .env; then start the services.'
