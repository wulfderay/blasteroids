	opt m+,l.,c+
	
	section data	; Store the array in the data section
	
	global tim_zapotec ; define label as global
tim_zapotec:
	incbin 'zapotec.TIM' ; include TIM file.
