#!/bin/bash
echo "ServoSystemDemo install"
echo "install files and service"
cp base/ServoSystemDemo /opt/
cp base/Config.ini /opt/
cp base/ServoSystemDemo.service /etc/systemd/system/
echo "add x mod, enable service"
chmod +x /opt/ServoSystemDemo
chmod +x /etc/systemd/system/ServoSystemDemo.service
systemctl enable ServoSystemDemo