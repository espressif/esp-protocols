#!/usr/bin/env bash
# shellcheck shell=bash

set -euo pipefail

log() {
  echo "[setup-pyenv] $*"
}

abort() {
  log "ERROR: $*"
  if [[ "${BASH_SOURCE[0]}" != "${0}" ]]; then
    return 1
  else
    exit 1
  fi
}

PYENV_PYTHON_VERSION="${PYENV_PYTHON_VERSION:-3.12.6}"
PYENV_VENV_NAME="${PYENV_VENV_NAME:-pyenv-venv}"
PYENV_ROOT_OVERRIDE="${PYENV_ROOT_OVERRIDE:-}"
PYENV_REQUIREMENTS="${PYENV_REQUIREMENTS:-}"
PYENV_REQUIREMENTS_FILE="${PYENV_REQUIREMENTS_FILE:-}"
PYENV_PIP_EXTRA_ARGS="${PYENV_PIP_EXTRA_ARGS:-}"
PYENV_PIP_EXTRA_INDEX_URL="${PYENV_PIP_EXTRA_INDEX_URL:-}"

PYENV_ROOT="${PYENV_ROOT_OVERRIDE:-${PYENV_ROOT:-$HOME/.pyenv}}"
export PYENV_ROOT
export PATH="${PYENV_ROOT}/bin:${PATH}"

if ! command -v pyenv >/dev/null 2>&1; then
  abort "pyenv command not found. Ensure pyenv is installed on the runner."
fi

# Initialize pyenv environments
eval "$(pyenv init --path)"
eval "$(pyenv init -)"
if pyenv commands | grep -q "virtualenv"; then
  eval "$(pyenv virtualenv-init -)"
else
  abort "pyenv-virtualenv plugin is required but not available."
fi

if ! pyenv versions --bare | grep -Fxq "${PYENV_PYTHON_VERSION}"; then
  log "Installing Python ${PYENV_PYTHON_VERSION} via pyenv..."
  pyenv install -s "${PYENV_PYTHON_VERSION}"
else
  log "Python ${PYENV_PYTHON_VERSION} already installed."
fi

if ! pyenv virtualenvs --bare | grep -Fxq "${PYENV_VENV_NAME}"; then
  log "Creating virtualenv ${PYENV_VENV_NAME}..."
  pyenv virtualenv "${PYENV_PYTHON_VERSION}" "${PYENV_VENV_NAME}"
else
  log "Virtualenv ${PYENV_VENV_NAME} already exists."
fi

export PYENV_VERSION="${PYENV_VENV_NAME}"
pyenv shell "${PYENV_VENV_NAME}" >/dev/null

python --version

pip_args=()
if [[ -n "${PYENV_PIP_EXTRA_ARGS}" ]]; then
  # shellcheck disable=SC2206
  pip_args+=(${PYENV_PIP_EXTRA_ARGS})
fi
if [[ -n "${PYENV_PIP_EXTRA_INDEX_URL}" ]]; then
  pip_args+=(--extra-index-url "${PYENV_PIP_EXTRA_INDEX_URL}")
fi

run_pip_install() {
  local line="$1"
  line="${line%%\#*}" # strip inline comments
  line="$(echo -E "${line}" | xargs || true)"
  if [[ -z "${line}" ]]; then
    return
  fi
  # shellcheck disable=SC2086
  python -m pip install "${pip_args[@]}" ${line}
}

if [[ -n "${PYENV_REQUIREMENTS}" ]]; then
  while IFS=$'\n' read -r req_line || [[ -n "${req_line}" ]]; do
    run_pip_install "${req_line}"
  done <<< "${PYENV_REQUIREMENTS}"
fi

if [[ -n "${PYENV_REQUIREMENTS_FILE}" ]]; then
  while IFS=$'\n' read -r req_file || [[ -n "${req_file}" ]]; do
    req_file="$(echo -E "${req_file}" | xargs || true)"
    [[ -z "${req_file}" ]] && continue
    if [[ ! -f "${req_file}" ]]; then
      abort "requirements file '${req_file}' not found."
    fi
    # shellcheck disable=SC2086
    python -m pip install "${pip_args[@]}" -r "${req_file}"
  done <<< "${PYENV_REQUIREMENTS_FILE}"
fi

log "pyenv environment '${PYENV_VENV_NAME}' is ready."

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
  exit 0
else
  return 0
fi
