#ifndef MAIZO_NAV_H
#define MAIZO_NAV_H
/* Host-testable navigation, no Wii SDK dependency. Direction: -1 or +1.
 * Grid moves preserve the column at a short final row, clamping to its last
 * valid item instead of wrapping to an unrelated game. */
int maizo_nav_move(int current,int count,int columns,int horizontal,int direction);
int maizo_nav_scroll(int current,int scroll,int count,int columns,int visible);
#endif
