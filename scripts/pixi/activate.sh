# shellcheck shell=bash
# ---------------------------------------------------------------------------
# Activate the project-local pixi env for ROS 2 / colcon on macOS arm64.
#
# Usage:
#   source scripts/pixi/activate.sh
#   colcon build --symlink-install
#   ros2 launch texnitis_mbf_sim texnitis_mbf_sim_demo.launch.py
#
# Why this exists:
#   `pixi run` activates PATH / CONDA_PREFIX but does not always source the
#   conda-forge activate.d scripts that set CC / CXX / SDKROOT /
#   MACOSX_DEPLOYMENT_TARGET. Without them CMake picks Apple Clang against
#   the system SDK and rosidl_generator_py drifts to Homebrew's python3.14
#   instead of the env's python3.12.
#
#   macOS SIP additionally strips DYLD_LIBRARY_PATH whenever /usr/bin/env
#   exec()s python3, so any rclpy script with `#!/usr/bin/env python3`
#   silently fails to dlopen ROS typesupport dylibs that live in
#   install/<pkg>/lib. We rewrite shebangs to point at the env's interpreter
#   directly after every `colcon build` so SIP never trips the launcher.
# ---------------------------------------------------------------------------

PIXI_ENV_PREFIX="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)/../../.pixi/envs/default"
PIXI_ENV_PREFIX="$(cd "${PIXI_ENV_PREFIX}" 2>/dev/null && pwd)"

if [ -z "${PIXI_ENV_PREFIX}" ] || [ ! -d "${PIXI_ENV_PREFIX}" ]; then
    echo "activate.sh: pixi env not installed yet. Run 'pixi install' first." >&2
    return 1 2>/dev/null || exit 1
fi

export CONDA_PREFIX="${PIXI_ENV_PREFIX}"
export CMAKE_PREFIX_PATH="${PIXI_ENV_PREFIX}${CMAKE_PREFIX_PATH:+:${CMAKE_PREFIX_PATH}}"

# Source every conda-forge activate.d script. These set CC/CXX/AR/SDKROOT/
# CFLAGS/CXXFLAGS/LDFLAGS/MACOSX_DEPLOYMENT_TARGET to values matching the
# env's compilers, which is what rosidl + ament_cmake need to resolve cleanly.
for activation_script in "${PIXI_ENV_PREFIX}"/etc/conda/activate.d/*.sh; do
    if [ -r "${activation_script}" ]; then
        # gfortran etc. probe binaries that may not be in the env; their
        # warnings are noise, ignore failures.
        # shellcheck disable=SC1090
        . "${activation_script}" 2>/dev/null || true
    fi
done

# Pin Python detection to the env's interpreter so rosidl_generator_py does
# not pick up Homebrew's python3.14.
PYTHON="${PIXI_ENV_PREFIX}/bin/python3.12"
[ -x "${PYTHON}" ] || PYTHON="${PIXI_ENV_PREFIX}/bin/python3"
export PYTHON
export Python3_EXECUTABLE="${PYTHON}"
export Python3_ROOT_DIR="${PIXI_ENV_PREFIX}"

# Pin deployment target so libc++ keeps std::filesystem / std::variant
# available (some activate.d scripts leave it unset).
if [ -z "${MACOSX_DEPLOYMENT_TARGET}" ]; then
    export MACOSX_DEPLOYMENT_TARGET="11.0"
fi

WORKSPACE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")"/../.. && pwd)"

# Patch every installed Python script's shebang to point straight at the
# env's interpreter. Required after every `colcon build` (colcon rewrites
# `#!/usr/bin/env python3` back); SIP would otherwise strip
# DYLD_LIBRARY_PATH and break dylib lookups for ROS typesupport.
fix_shebangs() {
    local install_dir="${WORKSPACE_ROOT}/install"
    if [ ! -d "${install_dir}" ]; then
        echo "fix_shebangs: ${install_dir} not found; nothing to do." >&2
        return 0
    fi
    local pixi_python="${PIXI_ENV_PREFIX}/bin/python3"
    local count=0
    while IFS= read -r script; do
        sed -i '' "1s|^#!/usr/bin/env python3.*|#!${pixi_python}|" "${script}"
        count=$((count + 1))
    done < <(grep -rl "^#!/usr/bin/env python3" "${install_dir}"/*/lib/ 2>/dev/null)
    echo "fix_shebangs: patched ${count} script(s) under ${install_dir}"
}

# Convenience: build + shebang fix in one go.
cb() {
    colcon build --symlink-install "$@" && fix_shebangs
}

echo "Pixi env activated: ${PIXI_ENV_PREFIX}"
echo "  CC=${CC:-<unset>}"
echo "  CXX=${CXX:-<unset>}"
echo "  Python3_EXECUTABLE=${Python3_EXECUTABLE}"
echo ""
echo "Helpers: cb [args...]    -- colcon build --symlink-install + fix_shebangs"
echo "         fix_shebangs    -- rewrite #!/usr/bin/env python3 in install/"
