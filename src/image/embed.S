	.section ".data", "aw"
	.balign 4
	.globl _embedded_image_start
_embedded_image_start:
	.incbin EMBEDIMG
	.globl _embedded_image_end
_embedded_image_end:
