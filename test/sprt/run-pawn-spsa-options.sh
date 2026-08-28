#!/usr/bin/env bash

BASE_CMD=(
	"./build/release/qapla-engine-tester"
	"--settingsfile=test/spsa/pawn-sprt-linux.ini"
)

OPTIONS=(
	"option.pawnDoubleConnectFactor=15"
	"option.pawnProtedctedPassedFactor=11"
	"option.pawnConnectedPassedFactor=11"
	"option.pawnDistantPassedFactor=15"
	"option.pawnDoublePawnFactor=5"
	"option.pawnIsolatedPawnFactor=9"
	"option.pawnWeakPawnFactor=9"
)

for option_value in "${OPTIONS[@]}"; do
	parameter_key="${option_value%%=*}"
	parameter_name="${parameter_key#option.}"
	engine_name="Qaplaoptspsa_${parameter_name}"

	echo "Running with ${option_value} (engine=${engine_name})"
	"${BASE_CMD[@]}" \
		"--engine" \
		"name=${engine_name}" \
		"cmd=/home/mangar/dev/qapla/build/ReleaseOpt/Qapla" \
		"gauntlet=true" \
		"${option_value}"
done
