/*
 * Copyright (C) 2024 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "nautilus-list-base.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_COMPUTER_VIEW (nautilus_computer_view_get_type())

G_DECLARE_FINAL_TYPE (NautilusComputerView, nautilus_computer_view, NAUTILUS, COMPUTER_VIEW, NautilusListBase)

NautilusComputerView *nautilus_computer_view_new (void);

G_END_DECLS
