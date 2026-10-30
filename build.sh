#! /bin/bash

set -euo pipefail

export UE4_ROOT="/home/fireblade2534/Documents/Code/Carla/UnrealEngine_4.26"

CARLA_BUILD_FOLDER="$PWD/Build"

sudo mkdir -p "$CARLA_BUILD_FOLDER"
sudo chown -R "$(id -u):$(id -g)" "$CARLA_BUILD_FOLDER"
chmod -R u+rwX "$CARLA_BUILD_FOLDER"

bash ./Util/BuildTools/Setup.sh --ros2
bash  -o pipefail ./Util/BuildTools/BuildLibCarla.sh --server --release --ros2
bash ./Util/BuildTools/BuildCarlaUE4.sh --build --ros2 --no-simready
bash ./Util/BuildTools/Package.sh --no-zip

