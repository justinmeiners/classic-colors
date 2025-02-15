/* 
 * Copyright (c) 2021 Justin Meiners
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UNDO_QUEUE_H
#define UNDO_QUEUE_H

#include "layer.h"


typedef struct
{
    int full_image;
    CcRect rect;
    size_t data_size;
    unsigned char* data;
} UndoPatch;

#define IS_POW_2(n) (0 == ((n) & ((n) - 1)))

// How much memory should we budget?
// It should be proprotional to the size of the image being edited.
// Which is what a fixed number of steps will do.
//
//
// Estimate using a 4:1 compression:
// 1280 * 720 * 4 / 4 * 256 ~= 200 Mb

#define UNDO_QUEUE_MAX 512
#define UNDO_QUEUE_MIN 400

typedef struct
{
    UndoPatch patches[UNDO_QUEUE_MAX];
    size_t front;
    size_t back;
    size_t undo;
    size_t since_last_checkpoint;
} CcUndo;

void cc_undo_clear(CcUndo* q);
void cc_undo_record_change(CcUndo* q, const CcLayer* layer, CcRect changed_region);

void cc_undo_maybe_back(CcUndo* q, CcLayer* target);
void cc_undo_maybe_forward(CcUndo* q, CcLayer* target);

int cc_undo_can_back(CcUndo* q);
int cc_undo_can_forward(CcUndo* q);

#endif
