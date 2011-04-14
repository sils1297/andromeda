;
;    Golden Eagle bootloader. Loads the fenix kernel.
;    Copyright (C) 2011 Michel Megens
;
;    This program is free software: you can redistribute it and/or modify
;    it under the terms of the GNU General Public License as published by
;    the Free Software Foundation, either version 3 of the License, or
;    (at your option) any later version.
;
;    This program is distributed in the hope that it will be useful,
;    but WITHOUT ANY WARRANTY; without even the implied warranty of
;    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;    GNU General Public License for more details.

;    You should have received a copy of the GNU General Public License
;    along with this program.  If not, see <http://www.gnu.org/licenses/>.
;

[BITS 16]
[ORG 0X7C00]

main:
	jmp $

;
;  Output routines
; 

println:
	xor cx, cx ; strlen

startprint:
	lodsb
	or al, al
	jz newline 			; 0 byte found
	mov ah, 0x0E 			; teletype output
	xor bh, bh			; page 0
	int 0x10
	inc cx

newline:
	xor bh, bh
	mov al, 0x0A
	mov ah, 0x0E
	int 0x10

endprintln:
	xor bh, bh
	mov al, 0x08


times 512-($-$$)-2 db 0
dw 0xAA55

