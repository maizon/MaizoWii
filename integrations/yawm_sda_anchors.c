/*
 * MAIZO YAWM modern devkitPPC EABI small-data anchors.
 *
 * The official libogc libraries contain signed SDA21 references to .sdata
 * (r13) and .sdata2 (r2). The old YAWM source and its bundled libraries can
 * push the linker's automatic base symbols too far past early libogc data.
 * Define one local anchor per section, before static archives are linked.
 * Linker --defsym uses these symbols plus 0x7e00 (32256 bytes), leaving
 * 512 bytes of negative headroom and all remaining positive 16-bit span.
 *
 * This code does not perform any NAND/IOS changes.
 * GPL-2.0-or-later.
 */
__attribute__((used,section(".sdata"))) unsigned maizo_sda_anchor = 0;
__attribute__((used,section(".sdata2"))) const unsigned maizo_sda2_anchor = 0;
