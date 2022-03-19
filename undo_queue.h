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


typedef struct UndoPatch
{
    struct UndoPatch* next;
    int full_image;

    CcRect rect;
    unsigned char* data;
    size_t data_size;
} UndoPatch;

UndoPatch* undo_patch_create(const CcBitmap* src, CcRect r);
void undo_patch_destroy(UndoPatch* patch);


typedef struct
{
    // undo queue
    int undo_count;
    UndoPatch* last_undo;
    UndoPatch* first_undo;
} UndoQueue;

void undo_queue_init(UndoQueue* q);

void undo_queue_clear(UndoQueue* q);

void undo_queue_trim(UndoQueue* q, int max_undos);
void undo_queue_push(UndoQueue* q, UndoPatch* patch);

int undo_queue_can_undo(UndoQueue* q);
int undo_queue_can_redo(UndoQueue* q);

void undo_queue_undo(UndoQueue* q, CcLayer* target);
void undo_queue_redo(UndoQueue* q, CcLayer* target);

#endif
