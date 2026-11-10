/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker Böhm
 * @copyright Copyright (c) 2026 Volker Böhm
 * @Overview
 * The net compiled into the engine, the default net of a build with QAPLA_USE_NNUE.
 * Generated - do not edit: chess-tools embedbin --input=test/nnue/milestones/nnue-q1234-02abcd-03a-no16-b75-40m-v3.3.0-e800.nnue
 *     --name=QaplaEmbeddedNet --header=src/nnue/embedded-net.h --source=src/nnue/embedded-net.cpp
 * Net: q1234-02abcd-03a-no16-b75 (trainer 3.3.0, epoch 800) - all of parts 01 and 02 of the 100 M book (HCE against
 * HCE, depth 9 WDL) plus quarter 03a (Qapla HCE against Spike 1.4.3), with the 10 M set, without sets 1-6, WDL
 * weight 0.75, Qapla 2.0.0-004, 2026-11-10.
 */

#pragma once

#include <cstdint>

extern const uint32_t QaplaEmbeddedNetSize;
extern const uint32_t QaplaEmbeddedNet[];
