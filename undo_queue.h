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
} CcUndoQueue;

void cc_undo_queue_init(CcUndoQueue* q);

void cc_undo_queue_clear(CcUndoQueue* q);

void cc_undo_queue_trim(CcUndoQueue* q, int max_undos);
void cc_undo_queue_push(CcUndoQueue* q, UndoPatch* patch);

int cc_undo_queue_can_undo(CcUndoQueue* q);
int cc_undo_queue_can_redo(CcUndoQueue* q);

void cc_undo_queue_undo(CcUndoQueue* q, CcLayer* target);
void cc_undo_queue_redo(CcUndoQueue* q, CcLayer* target);

#endif
