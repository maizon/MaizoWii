#include "carousel_layout.h"
/* Integer-only, pixel-aligned 8:11 portrait cards; center above neighbors.
 * Selection changes which art is bound to each slot, not slot geometry.
 * Global x offsets caused visible oscillation in prior Premium build. */
int maizo_card_slot(int relative,MaizoCardSlot *out){
 static const MaizoCardSlot slots[5]={
  {28,200,80,110},
  {116,169,112,154},
  {240,127,160,220},
  {412,169,112,154},
  {532,200,80,110}
 };
 if(!out||relative< -2||relative>2)return 0;
 *out=slots[relative+2];
 return 1;
}
