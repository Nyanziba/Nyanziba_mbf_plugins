#!/usr/bin/env bash
# Drop the developer into an interactive subshell with the pixi env, the
# colcon install/ tree, and DYLD_LIBRARY_PATH already set. Picks $SHELL
# (typically zsh on macOS) so AMENT_SHELL ends up matching the user's
# rc files and ros2 tab-completion uses compdef instead of bash-only
# `complete -F`.
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
WORKSPACE_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Prepare a transient rc fragment so the chosen interactive shell sources
# our activate.sh (and the workspace install/ if it exists) on startup.
# We can't just exec the shell with -c "source ..." because we need
# interactive mode (-i), which most shells will not honour with -c.
RC_FRAGMENT="$(mktemp -t pixi_shell_rc.XXXXXX)"
trap 'rm -f "${RC_FRAGMENT}"' EXIT

cat > "${RC_FRAGMENT}" <<'RC'
# shellcheck disable=SC1090,SC1091
source "${PIXI_SHELL_ACTIVATE}"
if [ -f "${PIXI_SHELL_WORKSPACE}/install/setup.bash" ] && [ -n "${BASH_VERSION:-}" ]; then
    set +u
    source "${PIXI_SHELL_WORKSPACE}/install/setup.bash"
    set -u
elif [ -f "${PIXI_SHELL_WORKSPACE}/install/setup.zsh" ] && [ -n "${ZSH_VERSION:-}" ]; then
    source "${PIXI_SHELL_WORKSPACE}/install/setup.zsh"
fi
export DYLD_LIBRARY_PATH="${PIXI_SHELL_WORKSPACE}/install_nav_core/lib:${DYLD_LIBRARY_PATH:-}"
export LD_LIBRARY_PATH="${PIXI_SHELL_WORKSPACE}/install_nav_core/lib:${LD_LIBRARY_PATH:-}"
echo "[pixi run shell] env activated. Type 'exit' to leave."
RC

export PIXI_SHELL_ACTIVATE="${SCRIPT_DIR}/activate.sh"
export PIXI_SHELL_WORKSPACE="${WORKSPACE_ROOT}"

# Decide which shell to invoke.
USER_SHELL="${SHELL:-/bin/bash}"
case "$(basename "${USER_SHELL}")" in
    zsh)
        # zsh sources $ZDOTDIR/.zshrc; instead point ZDOTDIR at a temp dir
        # holding only our fragment so we don't mix with the user's dotfiles.
        TMP_ZDOTDIR="$(mktemp -d -t pixi_shell_zdotdir.XXXXXX)"
        trap 'rm -rf "${TMP_ZDOTDIR}"; rm -f "${RC_FRAGMENT}"' EXIT
        cp "${RC_FRAGMENT}" "${TMP_ZDOTDIR}/.zshrc"
        ZDOTDIR="${TMP_ZDOTDIR}" exec "${USER_SHELL}" -i
        ;;
    bash|*)
        exec "${USER_SHELL}" --rcfile "${RC_FRAGMENT}" -i
        ;;
esac
