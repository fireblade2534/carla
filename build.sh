#! /bin/bash

set -euo pipefail

export UE4_ROOT="/home/fireblade2534/Documents/Code/Carla/UnrealEngine_4.26"

CARLA_BUILD_FOLDER="$PWD/Build"

sudo chown -R fireblade2534:"$(id -gn fireblade2534)" \
  /home/fireblade2534/Documents/Code/Carla/carla

sudo chmod -R u+rwX \
  /home/fireblade2534/Documents/Code/Carla/carla

bash ./Util/BuildTools/Setup.sh --ros2
bash  -o pipefail ./Util/BuildTools/BuildLibCarla.sh --server --release --ros2
bash ./Util/BuildTools/BuildOSM2ODR.sh --build
bash ./Util/BuildTools/BuildUE4Plugins.sh --build
bash ./Util/BuildTools/BuildCarlaUE4.sh --build --ros2 --no-simready
bash ./Util/BuildTools/Package.sh --no-zip

