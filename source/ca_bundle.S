/* Assemble the pinned Mozilla PEM as read-only data, no runtime USB CA file. */
.section .rodata
.balign 32
.globl maizo_ca_bundle
maizo_ca_bundle:
.incbin "data/cacert.pem"
.byte 0
.globl maizo_ca_bundle_end
maizo_ca_bundle_end:
