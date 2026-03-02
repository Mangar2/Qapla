#!/usr/bin/env bash

set -euo pipefail

TESTER_BIN="/home/mangar/bin/qet"
SETTINGS_TEMPLATE="test/sprt/imbalance-sprt-linux.ini"
LOG_DIR="test/log/imbalance-sprt"
COLLECT_FILE="${LOG_DIR}/imbalance-sprt-summary.log"

OPTIONS=(
	"imbalanceOwnPP=15"
	"imbalanceOwnNP=86"
	"imbalanceOwnNN=-13"
	"imbalanceOwnBP=9"
	"imbalanceOwnBN=40"
	"imbalanceOwnBB=14"
	"imbalanceOwnRP=60"
	"imbalanceOwnRN=42"
	"imbalanceOwnRB=83"
	"imbalanceOwnRR=64"
	"imbalanceOwnQP=2"
	"imbalanceOwnQN=-36"
	"imbalanceOwnQB=-23"
	"imbalanceOwnQR=-46"
	"imbalanceOwnQQ=80"
	"imbalanceOppNP=23"
	"imbalanceOppBP=51"
	"imbalanceOppBN=49"
	"imbalanceOppRP=30"
	"imbalanceOppRN=-18"
	"imbalanceOppRB=-47"
	"imbalanceOppQP=106"
	"imbalanceOppQN=27"
	"imbalanceOppQB=-4"
	"imbalanceOppQR=21"
)

mkdir -p "${LOG_DIR}"
: > "${COLLECT_FILE}"

temp_settings="$(mktemp)"
trap 'rm -f "${temp_settings}"' EXIT

cp "${SETTINGS_TEMPLATE}" "${temp_settings}"

run_count=0
total_runs=$(( ${#OPTIONS[@]} * 2 ))

for option_value in "${OPTIONS[@]}"; do
	parameter_name="${option_value%%=*}"
	base_value="${option_value#*=}"

	for delta in -16 16; do
		test_value=$(( base_value + delta ))
		((run_count += 1))

		delta_label="minus16"
		if [[ ${delta} -gt 0 ]]; then
			delta_label="plus16"
		fi

		engine_name="Qaplaoptspsa_${parameter_name}_${delta_label}"
		run_log="${LOG_DIR}/$(printf '%02d' "${run_count}")-${parameter_name}-${test_value}.log"

		echo "[${run_count}/${total_runs}] Running ${parameter_name}: ${base_value} -> ${test_value}"
		"${TESTER_BIN}" \
			"--settingsfile=${temp_settings}" \
			"--engine" \
			"name=${engine_name}" \
			"cmd=/home/mangar/dev/qapla/build/ReleaseOpt/Qapla" \
			"gauntlet=true" \
			"option.${parameter_name}=${test_value}" \
			| tee "${run_log}"

		sprt_line="$(grep -E 'SPRT final result:' "${run_log}" | tail -n 1 || true)"
		decision_line="$(grep -E '^decision:' "${run_log}" | tail -n 1 || true)"

		if [[ -z "${sprt_line}" ]]; then
			sprt_line="$(grep -Ev '^[[:space:]]*$' "${run_log}" | tail -n 1 || true)"
		fi

		if [[ -z "${sprt_line}" ]]; then
			sprt_line="NO_RESULT_LINE_FOUND"
		fi

		{
			echo "${parameter_name}=${test_value}, ${sprt_line}"
			if [[ -n "${decision_line}" ]]; then
				echo "${decision_line}"
			fi
		} >> "${COLLECT_FILE}"
	done
done

echo "Finished ${total_runs} SPRT runs. Summary written to ${COLLECT_FILE}"
