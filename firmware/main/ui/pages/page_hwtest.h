// Hardware test page for the board bring-up.
#pragma once

#include "board.h"

// Call with the LVGL lock held.
void page_hwtest_create(const board_t *board);
