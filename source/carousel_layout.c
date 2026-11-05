#include "carousel_layout.h"
/* Integer-only, pixel-aligned 5:7 portrait cards; center above neighbors.
 * Selection changes which art is bound to each slot, not slot geometry.
 * Global x offsets caused visible oscillation in prior Premium build. */
int maizo_card_slot(int relative,MaizoCardSlot *out){
 static const MaizoCardSlot slots[5]={
  {28,197,76,107},
  {114,165,112,157},
  {240,125,160,224},
  {414,165,112,157},
  {536,197,76,107}
 };
 if(!out||relative< -2||relative>2)return 0;
 *out=slots[relative+2];
 return 1;
}
