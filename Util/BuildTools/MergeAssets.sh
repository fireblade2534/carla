#!/usr/bin/env bash
set -e

source "$(dirname "$0")/Environment.sh"

merge_version="$(get_git_repository_version)"

set -uo pipefail

merge_base="${CARLA_DIST_FOLDER}/CARLA_Shipping_${merge_version}/LinuxNoEditor"
merge_assets="${CARLA_DIST_FOLDER}/RoadRunnerAssets_${merge_version}"

if [[ ! -f "${merge_base}/CarlaUE4.sh" ]]; then
  echo "Missing packaged simulator: ${merge_base}" >&2
  exit 1
fi

if [[ ! -d "${merge_assets}/CarlaUE4/Content/RoadRunnerAssets/Static" ]]; then
  echo "Missing cooked RoadRunner assets: ${merge_assets}" >&2
  exit 1
fi

# Overlay the asset package onto the existing simulator package.
# Preserve simulator files that aren't in the asset package.
rsync -a \
  "${merge_assets}/" \
  "${merge_base}/"

echo "Merged RoadRunnerAssets into ${merge_base}"