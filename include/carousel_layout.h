#ifndef MAIZO_CAROUSEL_LAYOUT_H
#define MAIZO_CAROUSEL_LAYOUT_H
/* Independent Wii and host-testable coordinates. No animation state here.
 * All five visual slots must be stable regardless of the selected game. */
typedef struct { int x,y,w,h; } MaizoCardSlot;
int maizo_card_slot(int relative,MaizoCardSlot *out);
#endif
