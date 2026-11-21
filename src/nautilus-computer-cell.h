/*
 * WinDoesNot: a drive of "This PC" (icon, name, fill bar, free space)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-view-cell.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_COMPUTER_CELL (nautilus_computer_cell_get_type())

G_DECLARE_FINAL_TYPE (NautilusComputerCell, nautilus_computer_cell, NAUTILUS, COMPUTER_CELL, NautilusViewCell)

NautilusViewCell * nautilus_computer_cell_new (NautilusListBase *view);

G_END_DECLS
