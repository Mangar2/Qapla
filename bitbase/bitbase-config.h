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
 * @copyright Copyright (c) 2025 Volker Böhm
 * @Overview
 * Build configuration of the own bitbases.
 */

#pragma once

 /**
  * Loading Qapla's own bitbase files from disk.
  *
  * Undefined: the engine ships with the compiled-in KPK bitbase only and gets
  * every other ending from the Syzygy tablebases. Both formats need the same kind
  * of download, and there is no reason to maintain a second one - but KPK is 6 KB
  * inside the binary, needs no file at all, and has no fallback: the endgame
  * heuristic for that signature does nothing while only one pawn is on the board.
  *
  * Define it to get the file based bitbases back. It only controls what the engine
  * does. The generator, the verifier and their commands keep full file access
  * either way - they are offline tooling, they produced the compiled-in KPK data,
  * and they are the reference the tablebase port is checked against.
  */
  // #define QAPLA_USE_BITBASE_FILES
