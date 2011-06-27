;
;    Header for the memory map implementation.
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
;
;    You should have received a copy of the GNU General Public License
;    along with this program.  If not, see <http://www.gnu.org/licenses/>.
;

%ifndef __MEMPROBE_H
%define __MEMPROBE_H

%define GEBL_MMAP_SEG 0x50
%define GEBL_MMAP_OFFSET 0x00

%define GEBL_EXT_BASE 0x00100000
%define GEBL_HIGH_BASE 0x001000000
%define GEBL_LOW_BASE 0x00
%define GEBL_ACPI 0x01

; CMOS i/o ports

%define GEBL_CMOS_OUTPUT 0x70
%define GEBL_CMOS_INPUT 0x71
%define GEBL_DELAY_PORT 0x80

; CMOS data registers
%define GEBL_CMOS_EXT_MEM_LOW_ORDER_REGISTER 0x30
%define GEBL_CMOS_EXT_MEM_HIGH_ORDER_REGISTER 0x31
%define GEBL_CMOS_LOW_MEM_LOW_ORDER_REGISTER 0x15
%define GEBL_CMOS_LOW_MEM_HIGH_ORDER_REGISTER 0x16


; ram types
%define GEBL_USABLE_MEM 0x1
%define GEBL_RESERVED_MEM 0x2
%define GEBL_RECLAIMABLE_MEM 0x3
%define GEBL_NVS_MEM 0x4
%define GEBL_BAD_MEM 0x5

; pic ports
%define GEBL_PIC1_COMMAND 0x20
%define GEBL_PIC2_COMMAND 0xa0

%define GEBL_PIC1_DATA 0x21
%define GEBL_PIC2_DATA 0xa1

%macro nxte 1
	add di, 0x18
	cld	; just to be sure that di gets incremented
	mov si, mmap_entry
	mov cx, 0xc
	rep movsw	; copy copy copy!
	sub di, 0x18	; just to make addressing esier

	jmp %1
%endmacro

mmap_entry:	; 0x18-byte mmap entry
	base dq 0	; base address
	length dq 0	; length (top_addr - base_addr)
	type dd 0	; entry type
	acpi3 dd 0	; acpi 3.0 compatibility => should be 1
%endif